/*
 * Pulsmonitor - a simple heart rate monitor for Pebble 2 (and newer Core Devices watches).
 *
 * Reads the watch's optical heart rate sensor through the Health service and shows the pulse
 *   - graphically:   a scrolling ECG-style trace plus a heart icon that pumps on every beat
 *   - acoustically:  a short vibration "click" on every beat (Pebble 2 has no speaker) and,
 *                    on watches with a speaker (Core Time 2 / Core 2 Duo), a monitor-style beep.
 *
 * Buttons
 *   SELECT       toggle the vibration click
 *   UP           toggle the beep (speaker watches only)
 *   DOWN (long)  toggle demo mode (simulated pulse, useful in the emulator)
 *
 * The sensor delivers beats per minute, not individual beats, so beats are synthesised at the
 * measured rate; the trace is therefore a stylised heartbeat, not a medical ECG.
 */
#include <pebble.h>

// ---------- Tuning ----------
#define PX_MS              20     // one trace pixel = 20 ms (50 px/s sweep, ~3 s per screen)
#define FRAME_MS           50     // redraw period (20 fps); the trace advances 2-3 px per frame
#define MAX_STEPS_PER_TICK 16     // catch-up limit when rendering falls behind
#define HR_POLL_MS         1000   // fallback polling of the heart rate metric
#define HR_STALE_SEC       15     // no fresh reading for this long -> show "--"
#define STATUS_H           20     // height of the status line at the bottom
#define VIBE_MS            30     // length of the per-beat vibration click
#define TONE_HZ            880    // per-beat beep (speaker watches)
#define TONE_MS            45
#define TONE_VOLUME        70
#define HEART_BEAT_SCALE   132    // heart size in percent right after a beat
#define HEART_DECAY        3      // percent shrink per trace pixel back to 100
#define BPM_MIN            30
#define BPM_MAX            220

#define PERSIST_VIBE       1
#define PERSIST_SOUND      2

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
static Layer *s_canvas;
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
static bool s_vibe_on = true;
static bool s_sound_on = true;
static bool s_demo;
static int s_demo_dir = 1;

static AppTimer *s_tick_timer;
static AppTimer *s_beat_timer;
static AppTimer *s_poll_timer;
static uint32_t s_last_tick_ms;

// ---------- Beat scheduling ----------

static uint32_t beat_interval_ms(void) {
  return s_bpm > 0 ? (60000u / (uint32_t)s_bpm) : 0;
}

static void on_beat(void *context);

static void schedule_beat(void) {
  if (s_beat_timer) {
    app_timer_cancel(s_beat_timer);
    s_beat_timer = NULL;
  }
  uint32_t interval = beat_interval_ms();
  if (interval > 0) {
    s_beat_timer = app_timer_register(interval, on_beat, NULL);
  }
}

static void beat_feedback(void) {
  if (s_vibe_on) {
    static const uint32_t segments[] = {VIBE_MS};
    vibes_enqueue_custom_pattern((VibePattern){.durations = segments, .num_segments = 1});
  }
#if defined(PBL_SPEAKER)
  if (s_sound_on && !speaker_is_muted()) {
    speaker_play_tone(TONE_HZ, TONE_MS, TONE_VOLUME, SpeakerWaveformSine);
  }
#endif
}

static void on_beat(void *context) {
  s_beat_timer = NULL;
  s_beat_phase = 0;
  s_heart_scale = HEART_BEAT_SCALE;
  beat_feedback();
  schedule_beat();
}

static void set_bpm(int bpm) {
  if (bpm < BPM_MIN || bpm > BPM_MAX) {
    bpm = 0;
  }
  if (bpm > 0) {
    s_last_reading = time(NULL);
  }
  if (bpm == s_bpm) {
    return;
  }
  bool was_stopped = (s_bpm == 0);
  s_bpm = bpm;
  if (s_bpm == 0) {
    schedule_beat();            // cancels the running timer
  } else if (was_stopped) {
    on_beat(NULL);              // start beating right away
  }
  // If the rate merely changed, the pending beat fires at the old interval and
  // reschedules itself with the new one - no visible hiccup.
}

// ---------- Heart rate source ----------

static void read_heart_rate(void) {
  if (s_demo) {
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
  if (event == HealthEventHeartRateUpdate || event == HealthEventSignificantUpdate) {
    read_heart_rate();
    layer_mark_dirty(s_canvas);
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
  if (s_demo) {
    demo_step();
  } else {
    read_heart_rate();
    if (s_bpm > 0 && time(NULL) - s_last_reading > HR_STALE_SEC) {
      set_bpm(0);
      if (s_sensor == SensorReading) {
        s_sensor = SensorSearching;
      }
    }
  }
  layer_mark_dirty(s_canvas);
  s_poll_timer = app_timer_register(HR_POLL_MS, on_poll, NULL);
}

// ---------- Trace / animation ----------

static uint32_t now_ms(void) {
  time_t seconds;
  uint16_t millis;
  time_ms(&seconds, &millis);
  return (uint32_t)seconds * 1000u + millis;   // wraps every ~49 days, differences stay valid
}

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
  }
}

// The sweep speed is tied to wall-clock time, not to the frame rate: if drawing a frame takes
// longer than FRAME_MS, the trace catches up by several pixels instead of slowing down.
static void on_tick(void *context) {
  const uint32_t now = now_ms();
  uint32_t steps = (now - s_last_tick_ms) / PX_MS;
  if (steps > MAX_STEPS_PER_TICK) {
    steps = MAX_STEPS_PER_TICK;
    s_last_tick_ms = now;
  } else {
    s_last_tick_ms += steps * PX_MS;
  }
  for (uint32_t i = 0; i < steps; i++) {
    animation_step();
  }
  if (steps > 0) {
    layer_mark_dirty(s_canvas);
  }
  s_tick_timer = app_timer_register(FRAME_MS, on_tick, NULL);
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

static void draw_trace(GContext *ctx, GRect area) {
  const int baseline = area.origin.y + area.size.h * 62 / 100;
  const int w = area.size.w;

  // Dotted baseline, like a monitor grid line.
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorWhite));
  for (int x = area.origin.x; x < area.origin.x + w; x += 6) {
    graphics_draw_pixel(ctx, GPoint(x, baseline));
  }

  // Two 1 px passes give a 2 px line at a fraction of the cost of a thick stroke; the whole
  // screen is redrawn 20 times a second, so this keeps the watch responsive.
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_antialiased(ctx, false);
  GPoint prev = GPointZero;
  for (int x = 0; x < w && x < PBL_DISPLAY_WIDTH; x++) {
    int idx = (s_trace_head + x) % PBL_DISPLAY_WIDTH;
    int y = baseline - (int)s_trace[idx] * area.size.h / 60;
    GPoint p = GPoint(area.origin.x + x, y);
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
      if (!s_demo) return "Kein Pulssensor (DOWN lang: Demo)";
      break;
    case SensorNoPermission:
      return "Keine Health-Freigabe";
    default:
      break;
  }
#if defined(PBL_SPEAKER)
  snprintf(buf, len, "%sVib %s  |  Ton %s",
           s_demo ? "Demo  |  " : "",
           s_vibe_on ? "an" : "aus",
           s_sound_on ? "an" : "aus");
#else
  snprintf(buf, len, "%sVibration %s",
           s_demo ? "Demo  |  " : "",
           s_vibe_on ? "an" : "aus");
#endif
  return buf;
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Layout: top block (heart + number), scrolling trace, status line.
  const GRect content = PBL_IF_ROUND_ELSE(grect_inset(bounds, GEdgeInsets(14, 26)), bounds);
  const int top_h = bounds.size.h * 36 / 100;
  const GRect top = GRect(content.origin.x, content.origin.y, content.size.w, top_h);
  const GRect status = GRect(content.origin.x, content.origin.y + content.size.h - STATUS_H,
                             content.size.w, STATUS_H);
  const GRect trace = GRect(bounds.origin.x, top.origin.y + top.size.h, bounds.size.w,
                            status.origin.y - (top.origin.y + top.size.h));

  // Heart on the left of the top block.
  const int radius = top_h * 40 / 100;
  const GPoint heart_center = GPoint(top.origin.x + radius + 4, top.origin.y + top_h / 2);
  draw_heart(ctx, heart_center, radius);

  // BPM number and label to the right of the heart.
  const int text_x = heart_center.x + radius + 6;
  const GRect number_rect = GRect(text_x, top.origin.y + (top_h - 60) / 2,
                                  top.origin.x + top.size.w - text_x, 44);
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

  draw_trace(ctx, trace);

  char status_buf[48];
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  graphics_draw_text(ctx, status_text(status_buf, sizeof(status_buf)), s_font_status, status,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// ---------- Buttons ----------

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_vibe_on = !s_vibe_on;
  persist_write_bool(PERSIST_VIBE, s_vibe_on);
  layer_mark_dirty(s_canvas);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
#if defined(PBL_SPEAKER)
  s_sound_on = !s_sound_on;
  persist_write_bool(PERSIST_SOUND, s_sound_on);
  layer_mark_dirty(s_canvas);
#endif
}

static void down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_demo = !s_demo;
  set_bpm(0);
  if (s_demo) {
    demo_step();
  }
  layer_mark_dirty(s_canvas);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_long_click_subscribe(BUTTON_ID_DOWN, 700, down_long_click_handler, NULL);
}

// ---------- Window lifecycle ----------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_font_bpm = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  s_font_label = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  s_font_status = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

static void init_sensor(void) {
#if defined(PBL_HEALTH)
  const time_t now = time(NULL);
  HealthServiceAccessibilityMask mask =
      health_service_metric_accessible(HealthMetricHeartRateBPM, now, now);
  if (mask & HealthServiceAccessibilityMaskNoPermission) {
    s_sensor = SensorNoPermission;
  } else if (mask & HealthServiceAccessibilityMaskNotSupported) {
    s_sensor = SensorUnsupported;
  } else {
    s_sensor = SensorSearching;
    health_service_events_subscribe(health_handler, NULL);
    health_service_set_heart_rate_sample_period(1);   // sample as fast as the system allows
    read_heart_rate();
  }
#else
  s_sensor = SensorUnsupported;
#endif
}

static void init(void) {
  if (persist_exists(PERSIST_VIBE)) {
    s_vibe_on = persist_read_bool(PERSIST_VIBE);
  }
  if (persist_exists(PERSIST_SOUND)) {
    s_sound_on = persist_read_bool(PERSIST_SOUND);
  }

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  init_sensor();
  s_last_tick_ms = now_ms();
  s_tick_timer = app_timer_register(FRAME_MS, on_tick, NULL);
  s_poll_timer = app_timer_register(HR_POLL_MS, on_poll, NULL);
}

static void deinit(void) {
  if (s_tick_timer) app_timer_cancel(s_tick_timer);
  if (s_poll_timer) app_timer_cancel(s_poll_timer);
  if (s_beat_timer) app_timer_cancel(s_beat_timer);
#if defined(PBL_HEALTH)
  if (s_sensor == SensorSearching || s_sensor == SensorReading) {
    health_service_set_heart_rate_sample_period(0);   // hand the sensor back to the system
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
