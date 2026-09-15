#include "win_gyro.h"
#include "comm.h"
#include "settings.h"
#include "motion.h"
#include "gauge.h"
#include "preview.h"

#define FRAME_MS   100
#define ARM_MS     250   // Anlaufsperre nach dem Druck auf den Schalter

static Window   *s_window;
static Layer    *s_canvas;
static AppTimer *s_timer;
static bool      s_held;        // Totmannschalter gedrueckt
static int8_t    s_zoom_dir;    // Zoom ueber die Auf/Ab-Tasten
static time_t    s_arm_s;
static uint16_t  s_arm_ms;

static GyroAxes axes_mode(void) {
  return (GyroAxes)settings_get()->gyro_axes;
}

static bool armed(void) {
  // Ein kurzer Tipp auf den Schalter soll die Kamera nicht losschicken.
  time_t now_s; uint16_t now_ms;
  time_ms(&now_s, &now_ms);
  int32_t diff = (int32_t)(now_s - s_arm_s) * 1000
               + ((int32_t)now_ms - (int32_t)s_arm_ms);
  return diff < 0 || diff >= ARM_MS;
}

static void frame(void *ctx) {
  s_timer = NULL;
  motion_update();

  if (s_held && armed()) {
    int8_t pan = 0, tilt = 0, zoom = 0;
    motion_get_speeds(axes_mode(), settings_speed_limit(), &pan, &tilt, &zoom);
    // Der Zoom auf den Tasten hat Vorrang vor dem Zoom aus der Neigung.
    if (s_zoom_dir != 0) {
      zoom = (int8_t)(s_zoom_dir * settings_speed_limit());
    }
    comm_set_move(pan, tilt, zoom);
  } else {
    comm_set_move(0, 0, s_zoom_dir ? (int8_t)(s_zoom_dir * settings_speed_limit()) : 0);
  }

  layer_mark_dirty(s_canvas);
  s_timer = app_timer_register(FRAME_MS, frame, NULL);
}

// --- Tasten ---------------------------------------------------------------

static void hold_down(ClickRecognizerRef rec, void *ctx) {
  if (comm_get_status() == PTZ_STATUS_NOCONFIG) {
    if (settings_get()->vibrate) vibes_double_pulse();
    return;
  }
  // Die Haltung im Moment des Drueckens ist der Nullpunkt der Fahrt.
  motion_set_reference();
  time_ms(&s_arm_s, &s_arm_ms);
  s_held = true;
  preview_cancel();
  layer_mark_dirty(s_canvas);
}

static void hold_up(ClickRecognizerRef rec, void *ctx) {
  s_held = false;
  comm_stop_all();
  s_zoom_dir = 0;
  // Erst wenn die Kamera steht, lohnt sich ein frisches Bild.
  preview_request_delayed(800);
  layer_mark_dirty(s_canvas);
}

static void zoom_in_down(ClickRecognizerRef rec, void *ctx)  { s_zoom_dir = +1; }
static void zoom_out_down(ClickRecognizerRef rec, void *ctx) { s_zoom_dir = -1; }
static void zoom_release(ClickRecognizerRef rec, void *ctx)  { s_zoom_dir = 0; }

static void back_click(ClickRecognizerRef rec, void *ctx) {
  window_stack_pop(true);
}

static void back_long(ClickRecognizerRef rec, void *ctx) {
  PtzSettings *cfg = settings_get();
  cfg->gyro_axes = (uint8_t)((cfg->gyro_axes + 1) % GYRO_AXES_COUNT);
  settings_save();
  motion_set_reference();
  layer_mark_dirty(s_canvas);
}

static void click_config(void *ctx) {
  window_raw_click_subscribe(BUTTON_ID_SELECT, hold_down, hold_up, NULL);
  window_raw_click_subscribe(BUTTON_ID_UP,   zoom_in_down,  zoom_release, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, zoom_out_down, zoom_release, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
  window_long_click_subscribe(BUTTON_ID_BACK, 600, back_long, NULL);
}

// --- Zeichnen -------------------------------------------------------------

static void draw(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  const GyroAxes mode = axes_mode();
  const bool running = s_held && armed();

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  const int16_t pad = 4;
  const int16_t header_h = 24;
  gauge_draw_header(ctx, GRect(pad, pad, b.size.w - 2 * pad, header_h),
                    comm_get_cam_name(settings_get()->active_cam), comm_get_status());

  const int16_t foot_h = 44;
  const int16_t top = pad + header_h + 2;
  const int16_t field_h = b.size.h - top - foot_h - pad;

  int8_t dx = 0, dy = 0;
  motion_get_deflection(&dx, &dy);

  // Ohne gedrueckten Schalter zeigt die Anzeige die Neigung trotzdem an.
  // So laesst sich vor der ersten Fahrt pruefen, ob die Richtungen stimmen.
  bool image = preview_has_image();
  GaugeState g = {
    .pan  = dx,
    .tilt = dy,
    .zoom = 0,
    .active = running,
    .pan_locked  = (mode == GYRO_AXES_TILT_ONLY),
    .tilt_locked = (mode == GYRO_AXES_PAN_ZOOM),
    .over_image = image,
  };
  if (mode == GYRO_AXES_PAN_ZOOM) {
    g.zoom = dy;
  }
  if (s_zoom_dir != 0) {
    g.zoom = (int8_t)(s_zoom_dir * 100);
  }
  if (settings_get()->invert_pan)  g.pan  = (int8_t)-g.pan;
  if (settings_get()->invert_tilt) { g.tilt = (int8_t)-g.tilt; g.zoom = (int8_t)-g.zoom; }

  GRect field = GRect(pad + 2, top, b.size.w - 2 * pad - 4, field_h);
  if (image) {
    preview_draw(ctx, field);
  }
  gauge_draw(ctx, field, &g);

  if (preview_is_loading()) {
    int16_t bw = field.size.w - 8;
    int16_t bx = field.origin.x + 4;
    int16_t by = field.origin.y + field.size.h - 6;
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
    graphics_fill_rect(ctx, GRect(bx, by, bw, 3), 1, GCornersAll);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorBlack));
    graphics_fill_rect(ctx, GRect(bx, by, bw * preview_progress() / 100, 3), 1, GCornersAll);
  }

  graphics_context_set_text_color(ctx, GColorBlack);
  const char *state_text = running ? "FÄHRT" : (s_held ? "BEREIT" : "HALTEN");
  graphics_draw_text(ctx, state_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(pad, b.size.h - foot_h, b.size.w - 2 * pad, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  graphics_draw_text(ctx, gyro_axes_name(mode), fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(pad, b.size.h - foot_h + 25, b.size.w - 2 * pad, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void on_comm_update(void) {
  if (s_canvas) layer_mark_dirty(s_canvas);
}

// --- Fenster --------------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, draw);
  layer_add_child(root, s_canvas);
}

static void window_appear(Window *window) {
  comm_set_update_handler(on_comm_update);
  s_held = false;
  s_zoom_dir = 0;
  motion_start();
  motion_update();
  motion_set_reference();
  if (!preview_has_image()) {
    preview_request_delayed(200);
  }
  if (!s_timer) {
    s_timer = app_timer_register(FRAME_MS, frame, NULL);
  }
}

static void window_disappear(Window *window) {
  // Kein Fenster ohne Schalter darf eine laufende Fahrt hinterlassen.
  s_held = false;
  s_zoom_dir = 0;
  comm_stop_all();
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  motion_stop();
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void win_gyro_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_click_config_provider(s_window, click_config);
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .appear = window_appear,
      .disappear = window_disappear,
      .unload = window_unload,
    });
  }
  window_stack_push(s_window, true);
}
