/*
 * PTZ control window.
 *
 * Button mode (default):
 *   UP / DOWN (hold)   move along the current axis, release = stop
 *   SELECT             next axis (Tilt -> Pan -> Zoom -> Fokus)
 *   SELECT (long)      options menu
 *   BACK               stop and return to the camera list
 *
 * Motion mode (enabled via options or from the menu):
 *   Hold UP            pan/tilt follow the wrist (roll = pan, pitch = tilt)
 *   Hold DOWN          zoom follows the wrist pitch
 *   SELECT             back to button mode
 *   SELECT (long)      options menu
 *   Releasing a button always stops the camera (dead-man's switch).
 *   The neutral orientation is captured at the moment a button is pressed.
 */
#include "control_window.h"
#include "comm.h"
#include "motion.h"
#include "options_menu.h"

typedef enum {
  ENGAGE_NONE = 0,
  ENGAGE_PAN_TILT,
  ENGAGE_ZOOM,
} EngageState;

static struct {
  Window *window;
  Layer *canvas;

  PtzAxis axis;
  bool motion_mode;
  EngageState engaged;

  int8_t pan, tilt, zoom;       /* speeds currently commanded, -100..100 */
  int8_t focus;
  int8_t motion_x, motion_y;    /* live wrist deflection for the indicator */
} s;

static const char *s_axis_names[AXIS_COUNT] = { "Tilt", "Pan", "Zoom", "Fokus" };

/* ------------------------------------------------------------------ */
/* Sending helpers                                                     */
/* ------------------------------------------------------------------ */

static int8_t prv_scaled(int8_t value) {
  return (int8_t)(((int16_t)value * speed_percent(g_app.settings.speed)) / 100);
}

static void prv_send_motion(void) {
  comm_send_move(g_app.selected_camera, s.pan, s.tilt, s.zoom);
  layer_mark_dirty(s.canvas);
}

static void prv_stop_all(void) {
  bool was_moving = (s.pan || s.tilt || s.zoom);
  s.pan = s.tilt = s.zoom = 0;
  if (s.focus) {
    s.focus = 0;
    comm_send_focus(g_app.selected_camera, 0);
  }
  if (was_moving || s.engaged != ENGAGE_NONE) {
    comm_send_stop(g_app.selected_camera);
  }
  s.engaged = ENGAGE_NONE;
  if (s.canvas) layer_mark_dirty(s.canvas);
}

/* ------------------------------------------------------------------ */
/* Motion control                                                      */
/* ------------------------------------------------------------------ */

static void prv_motion_update(int8_t x, int8_t y) {
  s.motion_x = x;
  s.motion_y = y;

  if (s.engaged == ENGAGE_PAN_TILT) {
    /* roll -> pan, pitch -> tilt. Tilting the top edge of the watch down
     * (y > 0) tilts the camera down (negative tilt speed). */
    int8_t pan = prv_scaled(x);
    int8_t tilt = prv_scaled((int8_t)-y);
    if (g_app.settings.invert_pan) pan = -pan;
    if (g_app.settings.invert_tilt) tilt = -tilt;
    if (pan != s.pan || tilt != s.tilt) {
      s.pan = pan;
      s.tilt = tilt;
      prv_send_motion();
      return;
    }
  } else if (s.engaged == ENGAGE_ZOOM) {
    /* push the top edge down/forward -> zoom in */
    int8_t zoom = prv_scaled(y);
    if (zoom != s.zoom) {
      s.zoom = zoom;
      prv_send_motion();
      return;
    }
  }
  layer_mark_dirty(s.canvas);
}

static void prv_engage(EngageState what) {
  /* Switching from one engagement to the other (both buttons held): make
   * sure nothing commanded by the previous one keeps running. */
  if (s.pan || s.tilt || s.zoom) {
    s.pan = s.tilt = s.zoom = 0;
    prv_send_motion();
  }
  motion_calibrate();          /* current wrist orientation = zero */
  s.engaged = what;
  vibes_short_pulse();
  light_enable_interaction();
  layer_mark_dirty(s.canvas);
}

static void prv_disengage(void) {
  if (s.engaged == ENGAGE_NONE) return;
  s.engaged = ENGAGE_NONE;
  s.pan = s.tilt = s.zoom = 0;
  comm_send_stop(g_app.selected_camera);
  layer_mark_dirty(s.canvas);
}

bool control_window_motion_mode(void) {
  return s.motion_mode;
}

void control_window_set_motion_mode(bool on) {
  if (on == s.motion_mode) return;
  prv_stop_all();
  s.motion_mode = on;
  s.motion_x = s.motion_y = 0;
  if (s.window && window_stack_contains_window(s.window)) {
    motion_set_active(on, prv_motion_update);
  }
  if (s.canvas) layer_mark_dirty(s.canvas);
}

/* ------------------------------------------------------------------ */
/* Button handling                                                     */
/* ------------------------------------------------------------------ */

static void prv_axis_press(bool positive) {
  int8_t sp = prv_scaled(positive ? 100 : -100);
  switch (s.axis) {
    case AXIS_TILT:
      s.tilt = g_app.settings.invert_tilt ? (int8_t)-sp : sp;
      prv_send_motion();
      break;
    case AXIS_PAN:
      /* UP = left, DOWN = right */
      s.pan = g_app.settings.invert_pan ? sp : (int8_t)-sp;
      prv_send_motion();
      break;
    case AXIS_ZOOM:
      s.zoom = sp;
      prv_send_motion();
      break;
    case AXIS_FOCUS:
      s.focus = sp;
      comm_send_focus(g_app.selected_camera, sp);
      layer_mark_dirty(s.canvas);
      break;
    default:
      break;
  }
  light_enable_interaction();
}

static void prv_axis_release(void) {
  switch (s.axis) {
    case AXIS_FOCUS:
      if (s.focus) {
        s.focus = 0;
        comm_send_focus(g_app.selected_camera, 0);
      }
      layer_mark_dirty(s.canvas);
      break;
    default:
      s.pan = s.tilt = s.zoom = 0;
      prv_send_motion();
      break;
  }
}

static void prv_up_down(ClickRecognizerRef rec, void *ctx) {
  if (s.motion_mode) {
    prv_engage(ENGAGE_PAN_TILT);
  } else {
    prv_axis_press(true);
  }
}

static void prv_up_up(ClickRecognizerRef rec, void *ctx) {
  if (s.motion_mode) {
    if (s.engaged == ENGAGE_PAN_TILT) {
      prv_disengage();
    } else if (s.pan || s.tilt) {
      s.pan = s.tilt = 0;
      prv_send_motion();
    }
  } else {
    prv_axis_release();
  }
}

static void prv_down_down(ClickRecognizerRef rec, void *ctx) {
  if (s.motion_mode) {
    prv_engage(ENGAGE_ZOOM);
  } else {
    prv_axis_press(false);
  }
}

static void prv_down_up(ClickRecognizerRef rec, void *ctx) {
  if (s.motion_mode) {
    if (s.engaged == ENGAGE_ZOOM) {
      prv_disengage();
    } else if (s.zoom) {
      s.zoom = 0;
      prv_send_motion();
    }
  } else {
    prv_axis_release();
  }
}

static void prv_select_click(ClickRecognizerRef rec, void *ctx) {
  if (s.motion_mode) {
    control_window_set_motion_mode(false);
  } else {
    prv_stop_all();
    s.axis = (PtzAxis)((s.axis + 1) % AXIS_COUNT);
    layer_mark_dirty(s.canvas);
  }
}

static void prv_select_long(ClickRecognizerRef rec, void *ctx) {
  prv_stop_all();
  options_menu_push();
}

static void prv_back_click(ClickRecognizerRef rec, void *ctx) {
  prv_stop_all();
  window_stack_pop(true);
}

static void prv_click_config(void *ctx) {
  window_raw_click_subscribe(BUTTON_ID_UP, prv_up_down, prv_up_up, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, prv_down_down, prv_down_up, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, prv_select_long, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click);
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

static void prv_draw_triangle(GContext *ctx, GPoint c, int16_t size, bool up_or_left, bool horizontal) {
  GPoint pts[3];
  if (!horizontal) {
    int16_t dy = up_or_left ? -size : size;
    pts[0] = GPoint(c.x, c.y + dy);
    pts[1] = GPoint(c.x - size, c.y - dy / 2);
    pts[2] = GPoint(c.x + size, c.y - dy / 2);
  } else {
    int16_t dx = up_or_left ? -size : size;
    pts[0] = GPoint(c.x + dx, c.y);
    pts[1] = GPoint(c.x - dx / 2, c.y - size);
    pts[2] = GPoint(c.x - dx / 2, c.y + size);
  }
  GPathInfo info = { .num_points = 3, .points = pts };
  GPath *path = gpath_create(&info);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void prv_draw_plus_minus(GContext *ctx, GPoint c, int16_t size, bool plus) {
  graphics_fill_rect(ctx, GRect(c.x - size, c.y - 1, size * 2 + 1, 3), 0, GCornerNone);
  if (plus) {
    graphics_fill_rect(ctx, GRect(c.x - 1, c.y - size, 3, size * 2 + 1), 0, GCornerNone);
  }
}

static void prv_draw_bar_label(GContext *ctx, GRect rect, const char *text) {
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void prv_canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const bool big = bounds.size.w >= 180;      /* Pebble Time 2 (200x228) */
  const int16_t header_h = big ? 28 : 24;
  const int16_t bar_w = big ? 36 : 30;
  const int16_t footer_h = big ? 40 : 34;

  GColor accent = PBL_IF_COLOR_ELSE(GColorDarkCandyAppleRed, GColorBlack);
  GColor engaged_col = PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorBlack);
  GColor bar_bg = PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite);

  /* background */
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  /* header with camera name */
  graphics_context_set_fill_color(ctx, accent);
  graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, header_h), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  const char *cam_name = (g_app.selected_camera >= 0) ? g_app.cameras[g_app.selected_camera].name : "Kamera";
  graphics_draw_text(ctx, cam_name, fonts_get_system_font(big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD),
                     GRect(4, big ? 2 : 1, bounds.size.w - 8, header_h), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  /* right-hand button bar */
  GRect bar = GRect(bounds.size.w - bar_w, header_h, bar_w, bounds.size.h - header_h);
  graphics_context_set_fill_color(ctx, bar_bg);
  graphics_fill_rect(ctx, bar, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(bar.origin.x, bar.origin.y), GPoint(bar.origin.x, bounds.size.h));

  int16_t bar_third = bar.size.h / 3;
  GPoint up_c = GPoint(bar.origin.x + bar_w / 2, bar.origin.y + bar_third / 2);
  GPoint mid_c = GPoint(bar.origin.x + bar_w / 2, bar.origin.y + bar_third + bar_third / 2);
  GPoint down_c = GPoint(bar.origin.x + bar_w / 2, bar.origin.y + 2 * bar_third + bar_third / 2);
  int16_t glyph = big ? 9 : 7;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_text_color(ctx, GColorBlack);
  if (s.motion_mode) {
    /* hold indicators */
    if (s.engaged == ENGAGE_PAN_TILT) {
      graphics_context_set_fill_color(ctx, engaged_col);
      graphics_fill_rect(ctx, GRect(bar.origin.x + 1, bar.origin.y, bar_w - 1, bar_third), 0, GCornerNone);
      graphics_context_set_text_color(ctx, GColorWhite);
    }
    prv_draw_bar_label(ctx, GRect(bar.origin.x, up_c.y - 16, bar_w, 32), "PAN\nTILT");
    graphics_context_set_text_color(ctx, GColorBlack);
    if (s.engaged == ENGAGE_ZOOM) {
      graphics_context_set_fill_color(ctx, engaged_col);
      graphics_fill_rect(ctx, GRect(bar.origin.x + 1, bar.origin.y + 2 * bar_third, bar_w - 1, bar.size.h - 2 * bar_third), 0, GCornerNone);
      graphics_context_set_text_color(ctx, GColorWhite);
    }
    prv_draw_bar_label(ctx, GRect(bar.origin.x, down_c.y - 9, bar_w, 20), "ZOOM");
    graphics_context_set_text_color(ctx, GColorBlack);
    prv_draw_bar_label(ctx, GRect(bar.origin.x, mid_c.y - 9, bar_w, 20), "AUS");
  } else {
    switch (s.axis) {
      case AXIS_TILT:
        prv_draw_triangle(ctx, up_c, glyph, true, false);
        prv_draw_triangle(ctx, down_c, glyph, false, false);
        break;
      case AXIS_PAN:
        prv_draw_triangle(ctx, up_c, glyph, true, true);
        prv_draw_triangle(ctx, down_c, glyph, false, true);
        break;
      case AXIS_ZOOM:
        prv_draw_plus_minus(ctx, up_c, glyph, true);
        prv_draw_plus_minus(ctx, down_c, glyph, false);
        break;
      case AXIS_FOCUS:
        prv_draw_bar_label(ctx, GRect(bar.origin.x, up_c.y - 9, bar_w, 20), "FERN");
        prv_draw_bar_label(ctx, GRect(bar.origin.x, down_c.y - 9, bar_w, 20), "NAH");
        break;
      default:
        break;
    }
    /* select = next axis: small dots */
    for (int i = -1; i <= 1; i++) {
      graphics_fill_circle(ctx, GPoint(mid_c.x + i * 6, mid_c.y), 2);
    }
  }

  /* main area */
  GRect main = GRect(0, header_h, bounds.size.w - bar_w, bounds.size.h - header_h - footer_h);

  /* mode + axis label */
  graphics_context_set_text_color(ctx, GColorBlack);
  const char *mode_txt = s.motion_mode ? "MOTION" : "TASTEN";
  graphics_draw_text(ctx, mode_txt, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(main.origin.x, main.origin.y + 2, main.size.w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  const char *axis_txt;
  if (s.motion_mode) {
    axis_txt = (s.engaged == ENGAGE_PAN_TILT) ? "Pan/Tilt" : (s.engaged == ENGAGE_ZOOM) ? "Zoom" : "Halten...";
  } else {
    axis_txt = s_axis_names[s.axis];
  }
  graphics_context_set_text_color(ctx, s.engaged != ENGAGE_NONE ? engaged_col : accent);
  graphics_draw_text(ctx, axis_txt, fonts_get_system_font(big ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD),
                     GRect(main.origin.x, main.origin.y + (big ? 16 : 14), main.size.w, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  /* pan/tilt indicator box */
  int16_t box = big ? 84 : 62;
  int16_t top = main.origin.y + (big ? 52 : 44);
  int16_t avail = main.origin.y + main.size.h - top - (big ? 14 : 12);
  if (box > avail) box = avail;
  GRect boxr = GRect(main.origin.x + (main.size.w - box) / 2 - (big ? 6 : 4), top, box, box);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, boxr);
  GPoint bc = grect_center_point(&boxr);
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack));
  graphics_draw_line(ctx, GPoint(boxr.origin.x, bc.y), GPoint(boxr.origin.x + box, bc.y));
  graphics_draw_line(ctx, GPoint(bc.x, boxr.origin.y), GPoint(bc.x, boxr.origin.y + box));

  /* dot: commanded pan/tilt (or live wrist tilt in motion mode) */
  int16_t dx, dy;
  if (s.motion_mode && s.engaged != ENGAGE_ZOOM) {
    dx = s.motion_x;
    dy = s.motion_y;
  } else {
    dx = s.pan;
    dy = (int16_t)-s.tilt;
  }
  int16_t half = box / 2 - 4;
  GPoint dot = GPoint(bc.x + dx * half / 100, bc.y + dy * half / 100);
  graphics_context_set_fill_color(ctx, (s.pan || s.tilt) ? engaged_col : accent);
  graphics_fill_circle(ctx, dot, big ? 5 : 4);

  /* zoom bar to the right of the box */
  GRect zb = GRect(boxr.origin.x + box + (big ? 6 : 4), boxr.origin.y, big ? 8 : 6, box);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, zb);
  int16_t zval = (s.motion_mode && s.engaged == ENGAGE_ZOOM) ? s.motion_y : (int16_t)-s.zoom;
  int16_t zc = zb.origin.y + zb.size.h / 2;
  int16_t zlen = zval * (zb.size.h / 2 - 2) / 100;
  graphics_context_set_fill_color(ctx, s.zoom ? engaged_col : accent);
  if (zlen >= 0) {
    graphics_fill_rect(ctx, GRect(zb.origin.x + 1, zc, zb.size.w - 2, zlen + 1), 0, GCornerNone);
  } else {
    graphics_fill_rect(ctx, GRect(zb.origin.x + 1, zc + zlen, zb.size.w - 2, -zlen + 1), 0, GCornerNone);
  }

  /* footer: speed + status / hint */
  GRect footer = GRect(0, bounds.size.h - footer_h, bounds.size.w - bar_w, footer_h);
  graphics_context_set_text_color(ctx, GColorBlack);
  static char line1[32];
  snprintf(line1, sizeof(line1), "Tempo: %s", speed_label(g_app.settings.speed));
  graphics_draw_text(ctx, line1, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(footer.origin.x + 2, footer.origin.y, footer.size.w - 4, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  const char *line2;
  if (g_app.status == STATUS_BRIDGE_UNREACHABLE) {
    line2 = "Bridge offline!";
    graphics_context_set_text_color(ctx, accent);
  } else if (g_app.status == STATUS_CMD_FAILED) {
    line2 = g_app.status_text[0] ? g_app.status_text : "Befehl fehlgeschlagen";
    graphics_context_set_text_color(ctx, accent);
  } else if (s.motion_mode) {
    line2 = "Taste halten + Handgelenk";
  } else {
    line2 = "Select: Achse | lang: Menü";
  }
  graphics_draw_text(ctx, line2, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(footer.origin.x + 2, footer.origin.y + 15, footer.size.w - 4, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

/* ------------------------------------------------------------------ */
/* Window lifecycle                                                    */
/* ------------------------------------------------------------------ */

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s.canvas = layer_create(bounds);
  layer_set_update_proc(s.canvas, prv_canvas_update);
  layer_add_child(root, s.canvas);
  window_set_click_config_provider(window, prv_click_config);
}

static void prv_window_appear(Window *window) {
  if (s.motion_mode) {
    motion_set_active(true, prv_motion_update);
  }
  layer_mark_dirty(s.canvas);
}

static void prv_window_disappear(Window *window) {
  /* leaving the screen (menu, back, notification): never leave the camera moving */
  prv_stop_all();
  motion_set_active(false, NULL);
}

static void prv_window_unload(Window *window) {
  layer_destroy(s.canvas);
  s.canvas = NULL;
  window_destroy(s.window);
  s.window = NULL;
}

void control_window_push(void) {
  if (!s.window) {
    s.window = window_create();
    window_set_window_handlers(s.window, (WindowHandlers){
      .load = prv_window_load,
      .appear = prv_window_appear,
      .disappear = prv_window_disappear,
      .unload = prv_window_unload,
    });
  }
  s.engaged = ENGAGE_NONE;
  s.pan = s.tilt = s.zoom = s.focus = 0;
  window_stack_push(s.window, true);
}

void control_window_refresh(void) {
  if (s.canvas) layer_mark_dirty(s.canvas);
}
