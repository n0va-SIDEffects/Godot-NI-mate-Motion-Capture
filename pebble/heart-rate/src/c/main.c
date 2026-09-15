/*
 * Pulsmonitor - a heart rate monitor for Pebble 2 (and newer Core Devices watches).
 *
 * Reads the watch's optical heart rate sensor through the Health service and shows the pulse
 *   - graphically:   a sweeping ECG-style trace plus a heart icon that pumps on every beat
 *   - acoustically:  a short vibration "click" on every beat and, on watches with a speaker,
 *                    a patient-monitor beep rendered from a click-free 880 Hz sample.
 *
 * Beat timing comes from the best source the watch offers:
 *   - HRV peak-to-peak intervals, when available. Each interval is one measured heartbeat, so
 *     the display and the sound follow the real beat instead of an averaged rate.
 *   - otherwise the beats-per-minute metric, with the beat phase anchored to the wall clock.
 *
 * Buttons
 *   SELECT       toggle the vibration click
 *   UP           toggle the beep (speaker watches only)
 *   DOWN (long)  toggle demo mode (simulated pulse, useful in the emulator)
 */
#include <pebble.h>

#include "app_clock.h"
#include "beat_clock.h"
#include "beep.h"
#include "settings.h"

// ---------- Tuning ----------
#define PX_MS              20     // default trace pixel length; the settings page can change it
#define FRAME_MS           33     // redraw period (~30 fps); the trace advances 1-2 px per frame
#define CATCHUP_PX         PBL_DISPLAY_WIDTH   // never replay more than one screen of timeline
#define BEAT_AUDIBLE_MS    120    // a beat older than this is only drawn, not sounded
#define BEAT_MIN_GAP_MS    260    // closest two beats may fall: 230 bpm, and longer than a beep
#define LIGHT_FLASH_MS     90     // how long the backlight stays on for a beat
#define HR_POLL_MS         200    // fallback polling of the heart rate metric
#define HR_SAMPLE_SEC      1      // requested sensor sampling period
#define HR_STALE_SEC       15     // no fresh reading for this long -> show "--"
#define STATUS_H           20     // height of the status line at the bottom
#define HEART_BEAT_SCALE   132    // heart size in percent right after a beat
#define HEART_DECAY        3      // percent shrink per trace pixel back to 100
#define BPM_MIN            30
#define BPM_MAX            220
#define PPI_MIN_MS         273    // 220 bpm
#define PPI_MAX_MS         2000   // 30 bpm
// How far a measured interval may sit from the averaged rate before it is treated as a misread.
// Beat-to-beat variation stays well inside this; a halved or doubled interval does not.
#define PPI_TOLERANCE_PCT  25
// Measured intervals must keep arriving; otherwise fall back to the averaged rate.
#define LIVE_TIMEOUT_MS    5000

// One stylised P-QRS-T complex, one sample per trace pixel, in "trace units" (positive = up,
// 60 units = trace height).
static const int8_t s_beat_shape[] = {
  0, 1, 3, 3, 1, 0, 0,        // P wave
  -3, 28, -9, -2, 0,          // QRS complex
  0, 1, 3, 5, 6, 5, 3, 1, 0,  // T wave
};
#define BEAT_SHAPE_LEN ((int)(sizeof(s_beat_shape) / sizeof(s_beat_shape[0])))

// Heart outline, 28 points, unit = 1/1000 of the half-width, y grows downwards.
static const GPoint s_heart_template[] = {
  {0, -471}, {11, -533}, {82, -682}, {242, -835}, {478, -904}, {731, -842}, {927, -660},
  {1000, -409}, {927, -143}, {731, 107}, {478, 335}, {242, 544}, {82, 726}, {11, 856},
  {0, 904}, {-11, 856}, {-82, 726}, {-242, 544}, {-478, 335}, {-731, 107}, {-927, -143},
  {-1000, -409}, {-927, -660}, {-731, -842}, {-478, -904}, {-242, -835}, {-82, -682}, {-11, -533},
};
#define HEART_POINTS ((uint32_t)(sizeof(s_heart_template) / sizeof(s_heart_template[0])))

typedef enum {
  SensorSearching,     // sensor available, waiting for a valid reading
  SensorReading,       // valid, fresh reading
  SensorUnsupported,   // this watch has no heart rate sensor
  SensorNoPermission,  // user denied Health access for this app
} SensorState;

static Window *s_window;
// Three layers instead of one: only the trace is redrawn every frame. The number, the heart and
// the status line are redrawn just when they change, which keeps the frame budget small enough
// for a smooth 30 fps sweep on the watch.
static Layer *s_head_layer;
static Layer *s_trace_layer;
static Layer *s_status_layer;
static GFont s_font_bpm;
static GFont s_font_label;
static GFont s_font_status;

static int8_t s_trace[PBL_DISPLAY_WIDTH];  // ring buffer, oldest sample at s_trace_head
static uint16_t s_trace_head;
static int s_beat_phase = -1;              // index into s_beat_shape while a beat is drawn
static int s_heart_scale = 100;            // percent

static int s_bpm;                          // 0 = unknown
static time_t s_last_reading;
static SensorState s_sensor = SensorSearching;
static bool s_live;                        // beats come from measured intervals, not from an average
static Settings s_settings;
static int s_demo_dir = 1;

static AppTimer *s_frame_timer;
static AppTimer *s_poll_timer;
static AppTimer *s_light_timer;
static BeatClock s_clock;                  // places the beats on the wall clock
static uint32_t s_ppi_ms;                  // measured interval now driving the beats, 0 if none
static uint32_t s_last_ppi_ms;             // wall clock of the last believed reading
static uint32_t s_ppi_recent[3];           // the last believed readings, newest first
static uint8_t s_ppi_have;                 // how many of them are filled

#define now_ms() clock_ms()

// ---------- Beat playback ----------

static void light_off(void *context) {
  s_light_timer = NULL;
  light_enable(false);   // back to the watch's own control
}

static void beat_feedback(void) {
  if (s_settings.vibe_on) {
    const uint32_t segments[] = {s_settings.vibe_ms};
    vibes_enqueue_custom_pattern((VibePattern){.durations = segments, .num_segments = 1});
  }
  beep_play();
  if (s_settings.backlight == BacklightOnBeat) {
    // light_enable_interaction() holds the backlight for the watch's own timeout, several
    // seconds, so at any normal pulse it would simply never go out again. Switch it on and off
    // instead, which is what a flash per beat actually looks like.
    if (s_light_timer) {
      app_timer_cancel(s_light_timer);
    }
    light_enable(true);
    s_light_timer = app_timer_register(LIGHT_FLASH_MS, light_off, NULL);
  }
}

// A beat happens: start its ECG complex and pump the heart. Beats the app is only catching up on
// are drawn but stay silent, so a stalled frame can never fire a burst of clicks.
static void fire_beat(bool audible) {
  s_beat_phase = 0;
  s_heart_scale = HEART_BEAT_SCALE;
  if (audible) {
    beat_feedback();
  }
}

static void set_interval(uint32_t interval_ms) {
  beat_clock_set_interval(&s_clock, interval_ms, now_ms());
}

// ---------- Trace / animation ----------

static void trace_push(int8_t v) {
  s_trace[s_trace_head] = v;
  s_trace_head = (s_trace_head + 1) % PBL_DISPLAY_WIDTH;
}

// Advance the trace by one pixel and let the heart shrink a little.
static void animation_step(void) {
  int8_t sample = 0;
  if (s_beat_phase >= 0) {
    sample = s_beat_shape[s_beat_phase++];
    if (s_beat_phase >= BEAT_SHAPE_LEN) {
      s_beat_phase = -1;
    }
  }
  trace_push(sample);

  if (s_heart_scale > 100) {
    s_heart_scale -= HEART_DECAY;
    if (s_heart_scale < 100) {
      s_heart_scale = 100;
    }
    layer_mark_dirty(s_head_layer);
  }
}

// Draw the trace forward to the current wall clock, one pixel at a time, firing every beat that
// falls due on the way. Both the sweep and the beats are driven by the clock rather than by timer
// callbacks, so a late or slow frame cannot stretch the pulse: each beat sounds in the same step
// that draws its spike, and the spacing on screen is the measured interval. The timing itself
// lives in beat_clock.c and is exercised by tools/test_beat_clock.c.
static bool advance_to_now(void) {
  const uint32_t steps = beat_clock_begin(&s_clock, now_ms());
  for (uint32_t i = 0; i < steps; i++) {
    bool audible = false;
    if (beat_clock_step(&s_clock, &audible)) {
      fire_beat(audible);
    }
    animation_step();
  }
  return steps > 0;
}

static void on_frame(void *context) {
  if (advance_to_now()) {
    layer_mark_dirty(s_trace_layer);
  }
  s_frame_timer = app_timer_register(FRAME_MS, on_frame, NULL);
}

// ---------- Heart rate source ----------

//! The interval the averaged rate implies, 0 while no rate is known.
static uint32_t rate_interval_ms(void) {
  return s_bpm > 0 ? 60000u / (uint32_t)s_bpm : 0;
}

//! Pick what drives the beats: a measured interval while a believed one keeps arriving, the
//! averaged rate otherwise. Called whenever either source changes, and on every poll so the
//! fallback takes over on its own once the measured intervals stop or stop being believed.
static void update_beat_rate(void) {
  uint32_t interval = 0;
  const BeatSource source = beat_clock_select(s_ppi_ms, now_ms() - s_last_ppi_ms, LIVE_TIMEOUT_MS,
                                              rate_interval_ms(), &interval);
  const bool live = (source == BeatSourceMeasured);
  if (live != s_live) {
    s_live = live;
    layer_mark_dirty(s_status_layer);
  }
  set_interval(interval);
}

static void set_bpm(int bpm) {
  if (bpm < BPM_MIN || bpm > BPM_MAX) {
    bpm = 0;
  }
  if (bpm > 0) {
    s_last_reading = time(NULL);
  }
  if (bpm != s_bpm) {
    s_bpm = bpm;
    layer_mark_dirty(s_head_layer);
  }
  update_beat_rate();
}

// A freshly measured peak-to-peak interval, delivered roughly once a second.
//
// Only the interval is used, never the moment the event arrives: the watch reports the length of
// a heartbeat that has already passed, so its arrival time says nothing about where the next beat
// falls. Beating on arrival as well as on the interval produced a second click a few hundred
// milliseconds after each one. The clock loop keeps the phase and simply adopts the interval,
// which gives an even rhythm at the measured rate.
//
// The interval is also checked against the averaged rate first. The sensor can lock onto the
// second bump of the pulse wave and report half the true interval, which would double the pulse
// while the displayed rate stays put.
static void on_measured_interval(uint16_t ppi_ms) {
  if (ppi_ms < PPI_MIN_MS || ppi_ms > PPI_MAX_MS) {
    return;
  }
  if (!beat_clock_interval_plausible(ppi_ms, rate_interval_ms(), PPI_TOLERANCE_PCT)) {
    return;   // a misread: leave the beat to the averaged rate
  }
  s_ppi_recent[2] = s_ppi_recent[1];
  s_ppi_recent[1] = s_ppi_recent[0];
  s_ppi_recent[0] = ppi_ms;
  if (s_ppi_have < 3) {
    s_ppi_have++;
  }
  // Take the middle of the last three readings once there are three. A lone reading that passed
  // the check but still sits off the others is then outvoted instead of setting the tempo.
  s_ppi_ms = (s_ppi_have == 3)
      ? beat_clock_median3(s_ppi_recent[0], s_ppi_recent[1], s_ppi_recent[2])
      : ppi_ms;
  s_last_ppi_ms = now_ms();
  s_last_reading = time(NULL);
  if (s_bpm == 0) {
    s_bpm = 60000 / ppi_ms;
    layer_mark_dirty(s_head_layer);
  }
  update_beat_rate();
}

static void read_heart_rate(void) {
  if (s_settings.demo) {
    return;
  }
#if defined(PBL_HEALTH)
  if (s_sensor == SensorUnsupported || s_sensor == SensorNoPermission) {
    return;
  }
  HealthValue value = health_service_peek_current_value(HealthMetricHeartRateBPM);
  if (value <= 0) {
    value = health_service_peek_current_value(HealthMetricHeartRateRawBPM);
  }
  if (value > 0) {
    set_bpm((int)value);
    s_sensor = SensorReading;
  }
#endif
}

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void *context) {
  switch (event) {
    case HealthEventHRVUpdate:
      on_measured_interval(health_service_peek_hrv_ppi_ms());
      break;
    case HealthEventHeartRateUpdate:
    case HealthEventSignificantUpdate:
      read_heart_rate();
      break;
    default:
      break;
  }
}
#endif

static void demo_step(void) {
  // Slow wander between 58 and 112 bpm so the display visibly reacts.
  int bpm = s_bpm > 0 ? s_bpm : 70;
  bpm += s_demo_dir;
  if (bpm >= 112) s_demo_dir = -1;
  if (bpm <= 58) s_demo_dir = 1;
  set_bpm(bpm);
}

static void on_poll(void *context) {
  if (s_settings.demo) {
    demo_step();
  } else {
    read_heart_rate();
    if (s_bpm > 0 && time(NULL) - s_last_reading > HR_STALE_SEC) {
      s_ppi_ms = 0;
      s_ppi_have = 0;
      set_bpm(0);
      if (s_sensor == SensorReading) {
        s_sensor = SensorSearching;
        layer_mark_dirty(s_status_layer);
      }
    } else {
      update_beat_rate();   // lets the measured interval time out on its own
    }
  }
  s_poll_timer = app_timer_register(HR_POLL_MS, on_poll, NULL);
}

// ---------- Drawing ----------

static void draw_heart(GContext *ctx, GPoint center, int radius) {
  GPoint points[HEART_POINTS];
  int r = radius * s_heart_scale / 100;
  for (uint32_t i = 0; i < HEART_POINTS; i++) {
    points[i] = GPoint(s_heart_template[i].x * r / 1000, s_heart_template[i].y * r / 1000);
  }
  GPath path = {
    .num_points = HEART_POINTS,
    .points = points,
    .rotation = 0,
    .offset = center,
  };
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  gpath_draw_filled(ctx, &path);
}

static void head_update_proc(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  const int radius = b.size.h * 40 / 100;
  const GPoint heart_center = GPoint(b.origin.x + radius + 4, b.origin.y + b.size.h / 2);
  draw_heart(ctx, heart_center, radius);

  const int text_x = heart_center.x + radius + 6;
  const GRect number_rect = GRect(text_x, b.origin.y + (b.size.h - 60) / 2,
                                  b.origin.x + b.size.w - text_x, 44);
  const GRect label_rect = GRect(number_rect.origin.x, number_rect.origin.y + 42,
                                 number_rect.size.w, 16);
  char bpm_buf[12];
  if (s_bpm > 0) {
    snprintf(bpm_buf, sizeof(bpm_buf), "%d", s_bpm);
  } else {
    snprintf(bpm_buf, sizeof(bpm_buf), "--");
  }
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, bpm_buf, s_font_bpm, number_rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_bpm > 0 ? "BPM" : "Suche Puls...", s_font_label, label_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void trace_update_proc(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  const int baseline = b.origin.y + b.size.h * 62 / 100;
  const int w = b.size.w;

  // Dotted baseline, like a monitor grid line, in a dimmed version of the trace colour.
  graphics_context_set_stroke_color(ctx, settings_baseline_color(&s_settings));
  for (int x = b.origin.x; x < b.origin.x + w; x += 6) {
    graphics_draw_pixel(ctx, GPoint(x, baseline));
  }

  // Two 1 px passes give a 2 px line at a fraction of the cost of a thick stroke.
  graphics_context_set_stroke_color(ctx, settings_trace_color(&s_settings));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_antialiased(ctx, false);
  GPoint prev = GPointZero;
  for (int x = 0; x < w && x < PBL_DISPLAY_WIDTH; x++) {
    int idx = (s_trace_head + x) % PBL_DISPLAY_WIDTH;
    int y = baseline - (int)s_trace[idx] * b.size.h / 60;
    GPoint p = GPoint(b.origin.x + x, y);
    if (x > 0) {
      graphics_draw_line(ctx, prev, p);
      graphics_draw_line(ctx, GPoint(prev.x, prev.y + 1), GPoint(p.x, p.y + 1));
    }
    prev = p;
  }
}

static const char *status_text(char *buf, size_t len) {
  switch (s_sensor) {
    case SensorUnsupported:
      if (!s_settings.demo) return "Kein Pulssensor (DOWN lang: Demo)";
      break;
    case SensorNoPermission:
      return "Keine Health-Freigabe";
    default:
      break;
  }
  const char *mode = s_settings.demo ? "Demo" : (s_live ? "Live" : NULL);
#if defined(PBL_SPEAKER)
  snprintf(buf, len, "%s%sVib %s  |  Ton %s",
           mode ? mode : "", mode ? "  |  " : "",
           s_settings.vibe_on ? "an" : "aus",
           s_settings.sound_on ? "an" : "aus");
#else
  snprintf(buf, len, "%s%sVibration %s",
           mode ? mode : "", mode ? "  |  " : "",
           s_settings.vibe_on ? "an" : "aus");
#endif
  return buf;
}

static void status_update_proc(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  char status_buf[48];
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  graphics_draw_text(ctx, status_text(status_buf, sizeof(status_buf)), s_font_status, b,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

//! Put the settings to work. Called once at startup and again whenever the phone sends new ones.
static void apply_settings(void) {
  s_clock.px_ms = s_settings.px_ms;
  beep_setup(&s_settings);
  light_enable(s_settings.backlight == BacklightAlwaysOn);
  if (s_settings.demo) {
    s_ppi_ms = 0;
    s_ppi_have = 0;
  }
  if (s_head_layer) {
    layer_mark_dirty(s_head_layer);
    layer_mark_dirty(s_trace_layer);
    layer_mark_dirty(s_status_layer);
  }
}

// ---------- Buttons ----------

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_settings.vibe_on = !s_settings.vibe_on;
  settings_save(&s_settings);
  apply_settings();
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
#if defined(PBL_SPEAKER)
  s_settings.sound_on = !s_settings.sound_on;
  settings_save(&s_settings);
  apply_settings();
#endif
}

static void down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_settings.demo = !s_settings.demo;
  settings_save(&s_settings);
  s_ppi_ms = 0;
  s_ppi_have = 0;
  set_bpm(0);
  if (s_settings.demo) {
    demo_step();
  }
  apply_settings();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_long_click_subscribe(BUTTON_ID_DOWN, 700, down_long_click_handler, NULL);
}

// ---------- Window lifecycle ----------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  s_font_bpm = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  s_font_label = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  s_font_status = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  // Layout: top block (heart + number), sweeping trace, status line. The trace uses the full
  // width; on a round display the text blocks are inset so nothing is clipped by the bezel.
  const GRect content = PBL_IF_ROUND_ELSE(grect_inset(bounds, GEdgeInsets(14, 26)), bounds);
  const int top_h = bounds.size.h * 36 / 100;
  const int status_y = content.origin.y + content.size.h - STATUS_H;
  const int trace_y = content.origin.y + top_h;

  s_head_layer = layer_create(GRect(content.origin.x, content.origin.y, content.size.w, top_h));
  layer_set_update_proc(s_head_layer, head_update_proc);
  layer_add_child(root, s_head_layer);

  s_trace_layer = layer_create(GRect(bounds.origin.x, trace_y, bounds.size.w, status_y - trace_y));
  layer_set_update_proc(s_trace_layer, trace_update_proc);
  layer_add_child(root, s_trace_layer);

  s_status_layer = layer_create(GRect(content.origin.x, status_y, content.size.w, STATUS_H));
  layer_set_update_proc(s_status_layer, status_update_proc);
  layer_add_child(root, s_status_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_head_layer);
  layer_destroy(s_trace_layer);
  layer_destroy(s_status_layer);
  s_head_layer = s_trace_layer = s_status_layer = NULL;
}

static void init_sensor(void) {
#if defined(PBL_HEALTH)
  const time_t now = time(NULL);
  HealthServiceAccessibilityMask mask =
      health_service_metric_accessible(HealthMetricHeartRateBPM, now, now);
  if (mask & HealthServiceAccessibilityMaskNoPermission) {
    s_sensor = SensorNoPermission;
    return;
  }
  if (mask & HealthServiceAccessibilityMaskNotSupported) {
    s_sensor = SensorUnsupported;
    return;
  }
  s_sensor = SensorSearching;
  health_service_events_subscribe(health_handler, NULL);
  health_service_set_heart_rate_sample_period(HR_SAMPLE_SEC);
  // Peak-to-peak intervals give one event per measured heartbeat. Where the watch supports them
  // the beats are real; where it does not this call is a no-op and the rate drives the beats.
  (void)health_service_set_hrv_sample_period(HR_SAMPLE_SEC);
  read_heart_rate();
#else
  s_sensor = SensorUnsupported;
#endif
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  settings_read_dict(&s_settings, iter);
  settings_save(&s_settings);
  apply_settings();
}

//! Ask the phone for the stored settings.
//!
//! The settings page only sends them the moment it is closed. Save them while the app is not
//! running and that message reaches nobody, so without this the watch would keep whatever it had
//! until the page happened to be closed with the app open.
static void request_settings(void *context) {
  static int attempts_left = 5;
  if (attempts_left <= 0) {
    return;
  }
  attempts_left--;
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    app_timer_register(2000, request_settings, NULL);
    return;
  }
  dict_write_uint8(out, MESSAGE_KEY_REQUEST_SETTINGS, 1);
  dict_write_end(out);
  app_message_outbox_send();
}

static void outbox_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *ctx) {
  // Usually the phone is not connected yet; try again in a moment.
  app_timer_register(2000, request_settings, NULL);
}

static void init(void) {
  settings_load(&s_settings);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  beat_clock_init(&s_clock, now_ms(), PX_MS, CATCHUP_PX, BEAT_AUDIBLE_MS, BEAT_MIN_GAP_MS);
  apply_settings();
  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_outbox_failed(outbox_failed_handler);
  // The settings page sends every key in one message: ten numbers, so a few hundred bytes. Asking
  // for the largest possible inbox instead would claim 8 KB of heap for nothing.
  app_message_open(512, 64);
  request_settings(NULL);
  init_sensor();
  s_frame_timer = app_timer_register(FRAME_MS, on_frame, NULL);
  s_poll_timer = app_timer_register(HR_POLL_MS, on_poll, NULL);
}

static void deinit(void) {
  if (s_light_timer) app_timer_cancel(s_light_timer);
  beep_teardown();
  light_enable(false);   // hand the backlight back to the watch
  if (s_frame_timer) app_timer_cancel(s_frame_timer);
  if (s_poll_timer) app_timer_cancel(s_poll_timer);
#if defined(PBL_HEALTH)
  if (s_sensor == SensorSearching || s_sensor == SensorReading) {
    // Hand the sensor back to the system so it is not kept awake after the app exits.
    health_service_set_heart_rate_sample_period(0);
    (void)health_service_set_hrv_sample_period(0);
    health_service_events_unsubscribe();
  }
#endif
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
