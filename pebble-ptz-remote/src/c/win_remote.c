#include "win_remote.h"
#include "win_gyro.h"
#include "win_menu.h"
#include "comm.h"
#include "settings.h"
#include "gauge.h"

static Window *s_window;
static Layer  *s_canvas;
static int8_t  s_dir;         // -1, 0 oder +1 auf der aktiven Achse

static PtzAxis active_axis(void) {
  return (PtzAxis)settings_get()->active_axis;
}

// Den gewuenschten Zustand aller Achsen setzen. Immer alle, damit eine zuvor
// bewegte Achse beim Wechsel garantiert stehen bleibt.
static void apply_direction(void) {
  const uint8_t limit = settings_speed_limit();
  const int8_t v = (int8_t)(s_dir * limit);

  switch (active_axis()) {
    case PTZ_AXIS_TILT:
      comm_set_move(0, v, 0);
      comm_set_focus(0);
      break;
    case PTZ_AXIS_PAN:
      comm_set_move(v, 0, 0);
      comm_set_focus(0);
      break;
    case PTZ_AXIS_ZOOM:
      comm_set_move(0, 0, v);
      comm_set_focus(0);
      break;
    case PTZ_AXIS_FOCUS:
      comm_set_move(0, 0, 0);
      comm_set_focus(v);
      break;
    default:
      break;
  }
  layer_mark_dirty(s_canvas);
}

static void press(int8_t dir) {
  if (comm_get_status() == PTZ_STATUS_NOCONFIG) {
    // Ohne eingerichtete Kamera waere jeder Tastendruck wirkungslos - das
    // sollte man merken, ohne erst auf die Meldung zu schauen.
    if (settings_get()->vibrate) vibes_double_pulse();
    return;
  }
  s_dir = dir;
  apply_direction();
}

static void release(void) {
  s_dir = 0;
  apply_direction();
}

// --- Tasten ---------------------------------------------------------------

static void up_down(ClickRecognizerRef rec, void *ctx)      { press(+1); }
static void up_release(ClickRecognizerRef rec, void *ctx)   { release(); }
static void down_down(ClickRecognizerRef rec, void *ctx)    { press(-1); }
static void down_release(ClickRecognizerRef rec, void *ctx) { release(); }

static void select_click(ClickRecognizerRef rec, void *ctx) {
  release();
  PtzSettings *cfg = settings_get();
  cfg->active_axis = (uint8_t)((cfg->active_axis + 1) % PTZ_AXIS_COUNT);
  settings_save();
  layer_mark_dirty(s_canvas);
}

static void select_long(ClickRecognizerRef rec, void *ctx) {
  release();
  win_menu_push();
}

static void click_config(void *ctx) {
  window_raw_click_subscribe(BUTTON_ID_UP,   up_down,   up_release,   NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, down_down, down_release, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long, NULL);
}

// --- Zeichnen -------------------------------------------------------------

static void draw(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  const PtzAxis axis = active_axis();

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  const int16_t pad = 4;
  const int16_t header_h = 24;
  gauge_draw_header(ctx, GRect(pad, pad, b.size.w - 2 * pad, header_h),
                    comm_get_cam_name(settings_get()->active_cam), comm_get_status());

  // Das Feld nimmt, was zwischen Kopf- und Fusszeile uebrig bleibt.
  const int16_t foot_h = 46;
  const int16_t top = pad + header_h + 2;
  const int16_t field_h = b.size.h - top - foot_h - pad;

  GaugeState g = {
    .pan  = axis == PTZ_AXIS_PAN  ? (int8_t)(s_dir * 100) : 0,
    .tilt = axis == PTZ_AXIS_TILT ? (int8_t)(s_dir * 100) : 0,
    .zoom = axis == PTZ_AXIS_ZOOM ? (int8_t)(s_dir * 100) : 0,
    .active = s_dir != 0,
    .pan_locked = false,
    .tilt_locked = false,
  };
  gauge_draw(ctx, GRect(pad + 2, top, b.size.w - 2 * pad - 4, field_h), &g);

  // Fusszeile: gewaehlte Achse, Geschwindigkeitsstufe, Tastenhinweis
  char line[48];
  const PtzSettings *cfg = settings_get();
  if (axis == PTZ_AXIS_FOCUS) {
    snprintf(line, sizeof(line), "FOCUS %s",
             s_dir > 0 ? "FERN" : (s_dir < 0 ? "NAH" : ""));
  } else {
    snprintf(line, sizeof(line), "%s  %d/5", ptz_axis_name(axis), cfg->speed);
  }
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, line, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(pad, b.size.h - foot_h, b.size.w - 2 * pad, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  const char *hint = (comm_get_status() == PTZ_STATUS_NOCONFIG)
      ? comm_get_message()
      : "SEL Achse · LANG Menü";
  graphics_draw_text(ctx, hint, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(pad, b.size.h - foot_h + 26, b.size.w - 2 * pad, 18),
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

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

static void window_appear(Window *window) {
  comm_set_update_handler(on_comm_update);
  s_dir = 0;
  apply_direction();
}

void win_remote_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_click_config_provider(s_window, click_config);
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload,
      .appear = window_appear,
    });
  }
  window_stack_push(s_window, true);
}
