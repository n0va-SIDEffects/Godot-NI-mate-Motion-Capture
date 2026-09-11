#include "status_window.h"
#include "comm.h"
#include "list_window.h"
#include "model.h"

// Main screen: project badge, elapsed time, description, footer line and a
// hand-drawn action bar (UP = refresh, SELECT = start/stop, DOWN = list).

static Window *s_window;
static Layer *s_canvas;
static GPath *s_play_path;
static GPath *s_arrow_path;

static const GPathInfo PLAY_PATH_INFO = {
  .num_points = 3,
  .points = (GPoint[]) {{-5, -7}, {-5, 7}, {7, 0}}
};

static const GPathInfo ARROW_PATH_INFO = {
  .num_points = 3,
  .points = (GPoint[]) {{-4, -5}, {-4, 5}, {5, 0}}
};

#if PBL_DISPLAY_HEIGHT >= 200
// Pebble Time 2 (emery, 200x228) and other large displays
#define BADGE_Y   10
#define BADGE_H   28
#define TIMER_Y   46
#define TIMER_H   46
#define DESC_Y    100
#define FOOTER_H  22
#define BADGE_FONT  FONT_KEY_GOTHIC_18_BOLD
#define TIMER_FONT  FONT_KEY_LECO_36_BOLD_NUMBERS
#define DESC_FONT   FONT_KEY_GOTHIC_24_BOLD
#define FOOTER_FONT FONT_KEY_GOTHIC_18
#else
// 144x168 rectangular and 180x180 round displays
#define BADGE_Y   PBL_IF_ROUND_ELSE(16, 6)
#define BADGE_H   24
#define TIMER_Y   PBL_IF_ROUND_ELSE(46, 36)
#define TIMER_H   36
#define DESC_Y    PBL_IF_ROUND_ELSE(88, 78)
#define FOOTER_H  18
#define BADGE_FONT  FONT_KEY_GOTHIC_14_BOLD
#define TIMER_FONT  FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM
#define DESC_FONT   FONT_KEY_GOTHIC_18_BOLD
#define FOOTER_FONT FONT_KEY_GOTHIC_14
#endif

static void prv_format_elapsed(char *buf, size_t len, time_t start) {
  time_t now = time(NULL);
  int32_t d = (int32_t)(now - start);
  if (d < 0) {
    d = 0;
  }
  snprintf(buf, len, "%d:%02d:%02d", (int)(d / 3600), (int)((d % 3600) / 60), (int)(d % 60));
}

static GColor prv_badge_color(const TimerStatus *s) {
  if (!s->valid) {
    return PBL_IF_COLOR_ELSE(GColorFolly, GColorBlack);
  }
  if (s->project_name[0] == '\0') {
    return PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack);
  }
#ifdef PBL_COLOR
  if (s->color) {
    return (GColor) { .argb = s->color };
  }
  return GColorDarkGray;
#else
  return GColorBlack;
#endif
}

static void prv_draw_action_bar(GContext *ctx, GRect bounds, bool running) {
  const int w = ACTION_BAR_WIDTH;
  GRect bar = PBL_IF_ROUND_ELSE(
      GRect(bounds.size.w - w - 3, 3, w, bounds.size.h - 6),
      GRect(bounds.size.w - w, 0, w, bounds.size.h));

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bar, PBL_IF_ROUND_ELSE(3, 0), PBL_IF_ROUND_ELSE(GCornersAll, GCornerNone));

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);

  const int cx = bar.origin.x + bar.size.w / 2;
  const int up_y = bar.origin.y + bar.size.h / 6;
  const int mid_y = bar.origin.y + bar.size.h / 2;
  const int down_y = bar.origin.y + (bar.size.h * 5) / 6;

  // UP: refresh (open circle with an arrow head)
  graphics_draw_arc(ctx, GRect(cx - 7, up_y - 7, 15, 15), GOvalScaleModeFitCircle,
                    DEG_TO_TRIGANGLE(60), DEG_TO_TRIGANGLE(360));
  gpath_move_to(s_arrow_path, GPoint(cx, up_y - 7));
  gpath_draw_filled(ctx, s_arrow_path);

  // SELECT: stop square while running, play triangle otherwise
  if (running) {
    graphics_fill_rect(ctx, GRect(cx - 6, mid_y - 6, 13, 13), 2, GCornersAll);
  } else {
    gpath_move_to(s_play_path, GPoint(cx + 1, mid_y));
    gpath_draw_filled(ctx, s_play_path);
  }

  // DOWN: list (three lines)
  for (int i = 0; i < 3; i++) {
    graphics_fill_rect(ctx, GRect(cx - 7, down_y - 6 + i * 5, 15, 2), 0, GCornerNone);
  }
}

static void prv_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  AppModel *m = model_get();
  const TimerStatus *s = &m->status;
  const bool running = s->valid && s->running;

  const int pad_left = PBL_IF_ROUND_ELSE(30, 8);
  const int pad_right = PBL_IF_ROUND_ELSE(8, 4);
  const int x = pad_left;
  const int cw = bounds.size.w - ACTION_BAR_WIDTH - pad_left - pad_right;
  // A progress/error message may need two lines; the start time needs one.
  const int footer_lines = m->message[0] ? 2 : 1;
  const int footer_h = FOOTER_H * footer_lines;
  const int footer_y = bounds.size.h - footer_h - PBL_IF_ROUND_ELSE(18, 4);

  // Project badge
  const GRect badge = GRect(x, BADGE_Y, cw, BADGE_H);
  const GColor badge_bg = prv_badge_color(s);
  const char *badge_text = !s->valid ? "Toggl Track"
                         : (s->project_name[0] ? s->project_name : "Kein Projekt");
  graphics_context_set_fill_color(ctx, badge_bg);
  graphics_fill_rect(ctx, badge, 6, GCornersAll);
  graphics_context_set_text_color(ctx, gcolor_legible_over(badge_bg));
  graphics_draw_text(ctx, badge_text, fonts_get_system_font(BADGE_FONT),
                     grect_inset(badge, GEdgeInsets(2, 6, 0, 6)),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Elapsed time
  char timer_text[16];
  if (running) {
    prv_format_elapsed(timer_text, sizeof(timer_text), s->start_time);
  } else {
    strncpy(timer_text, "0:00:00", sizeof(timer_text));
  }
  graphics_context_set_text_color(ctx, running ? GColorBlack : PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
  graphics_draw_text(ctx, timer_text, fonts_get_system_font(TIMER_FONT),
                     GRect(x, TIMER_Y, cw, TIMER_H),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // Description
  const char *desc;
  if (!s->valid) {
    desc = m->message_is_error ? "Nicht verbunden" : "Verbinde mit Handy…";
  } else if (running) {
    desc = s->description[0] ? s->description : "(ohne Beschreibung)";
  } else {
    desc = "Kein Timer läuft";
  }
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, desc, fonts_get_system_font(DESC_FONT),
                     GRect(x, DESC_Y, cw, footer_y - DESC_Y - 4),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Footer: progress / error message, otherwise the start time
  char footer[MESSAGE_LEN];
  GColor footer_color = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack);
  if (m->message[0]) {
    strncpy(footer, m->message, sizeof(footer));
    footer[sizeof(footer) - 1] = '\0';
    if (m->message_is_error) {
      footer_color = PBL_IF_COLOR_ELSE(GColorRed, GColorBlack);
    }
  } else if (running) {
    struct tm *lt = localtime(&s->start_time);
    strftime(footer, sizeof(footer), clock_is_24h_style() ? "seit %H:%M" : "seit %I:%M %p", lt);
  } else if (s->valid) {
    strncpy(footer, "SELECT: Timer starten", sizeof(footer));
  } else {
    footer[0] = '\0';
  }
  graphics_context_set_text_color(ctx, footer_color);
  graphics_draw_text(ctx, footer, fonts_get_system_font(FOOTER_FONT),
                     GRect(x, footer_y, cw, footer_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  prv_draw_action_bar(ctx, bounds, running);
}

// --- Buttons -----------------------------------------------------------------

static void prv_up_click(ClickRecognizerRef recognizer, void *context) {
  model_set_message("Aktualisiere…", false);
  comm_send_refresh();
  status_window_refresh();
}

static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  const TimerStatus *s = &model_get()->status;
  if (s->valid && s->running) {
    model_set_message("Stoppe…", false);
    comm_send_stop();
    status_window_refresh();
  } else {
    list_window_push();
  }
}

static void prv_down_click(ClickRecognizerRef recognizer, void *context) {
  list_window_push();
}

static void prv_select_long_click(ClickRecognizerRef recognizer, void *context) {
  // Long press always opens the list, even while a timer runs (switch task).
  list_window_push();
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, prv_select_long_click, NULL);
}

// --- Window lifecycle --------------------------------------------------------

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (model_get()->status.running) {
    status_window_refresh();
  }
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, prv_update_proc);
  layer_add_child(root, s_canvas);
  s_play_path = gpath_create(&PLAY_PATH_INFO);
  s_arrow_path = gpath_create(&ARROW_PATH_INFO);
}

static void prv_window_appear(Window *window) {
  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_handler);
  status_window_refresh();
}

static void prv_window_disappear(Window *window) {
  tick_timer_service_unsubscribe();
}

static void prv_window_unload(Window *window) {
  gpath_destroy(s_play_path);
  gpath_destroy(s_arrow_path);
  s_play_path = NULL;
  s_arrow_path = NULL;
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

void status_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, GColorWhite);
    window_set_click_config_provider(s_window, prv_click_config_provider);
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = prv_window_load,
      .appear = prv_window_appear,
      .disappear = prv_window_disappear,
      .unload = prv_window_unload,
    });
  }
  window_stack_push(s_window, true);
}

void status_window_refresh(void) {
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}

void status_window_destroy(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}
