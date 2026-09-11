#include "favorites_window.h"
#include "comm.h"
#include "dictation.h"
#include "i18n.h"
#include "list_window.h"
#include "model.h"
#include "status_window.h"

// Tile grid: favourites two per row, then one row with "Diktieren" | "Liste".
// UP/DOWN move the highlight, SELECT activates, a tap activates directly.

static Window *s_window;
static Layer *s_canvas;
static int s_selected;

#if PBL_DISPLAY_HEIGHT >= 200
#define TILE_FONT     FONT_KEY_GOTHIC_18_BOLD
#define TILE_SUBFONT  FONT_KEY_GOTHIC_14
#define TILE_GAP      4
#else
#define TILE_FONT     FONT_KEY_GOTHIC_14_BOLD
#define TILE_SUBFONT  FONT_KEY_GOTHIC_14
#define TILE_GAP      3
#endif

enum { CELL_DICTATE = -1, CELL_LIST = -2 };

static int prv_favorite_rows(void) {
  const int n = model_get()->favorite_count;
  return n == 0 ? 1 : (n + 1) / 2;    // an empty grid keeps one hint row
}

static int prv_cell_count(void) {
  return model_get()->favorite_count + 2;
}

// Cell index -> favourite index (>= 0) or an action cell.
static int prv_cell_kind(int cell) {
  const int n = model_get()->favorite_count;
  if (cell < n) {
    return cell;
  }
  return cell == n ? CELL_DICTATE : CELL_LIST;
}

static GRect prv_cell_rect(GRect bounds, int cell) {
  const int n = model_get()->favorite_count;
  const int rows = prv_favorite_rows() + 1;
  const int inset = PBL_IF_ROUND_ELSE(14, 2);
  const GRect area = grect_inset(bounds, GEdgeInsets(inset));
  const int cell_h = area.size.h / rows;
  const int cell_w = area.size.w / 2;
  int row, col;
  if (cell < n) {
    row = cell / 2;
    col = cell % 2;
  } else {
    row = rows - 1;
    col = cell == n ? 0 : 1;
  }
  return grect_inset(GRect(area.origin.x + col * cell_w, area.origin.y + row * cell_h, cell_w, cell_h),
                     GEdgeInsets(TILE_GAP));
}

static void prv_draw_tile(GContext *ctx, GRect r, GColor bg, const char *title, const char *sub, bool selected) {
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, r, 6, GCornersAll);
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorBlack, GColorBlack));
  graphics_context_set_stroke_width(ctx, selected ? 4 : 1);
  graphics_draw_round_rect(ctx, selected ? r : grect_inset(r, GEdgeInsets(0)), 6);
  const GColor fg = gcolor_legible_over(bg);
  graphics_context_set_text_color(ctx, fg);
  const GFont font = fonts_get_system_font(TILE_FONT);
  const GRect text = grect_inset(r, GEdgeInsets(3, 4, 3, 4));
  const GSize size = graphics_text_layout_get_content_size(title, font, text,
                                                           GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
  int y = text.origin.y + (text.size.h - size.h - (sub && sub[0] ? 16 : 0)) / 2 - 2;
  if (y < text.origin.y) {
    y = text.origin.y;
  }
  graphics_draw_text(ctx, title, font, GRect(text.origin.x, y, text.size.w, text.size.h - (y - text.origin.y)),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  if (sub && sub[0]) {
    graphics_draw_text(ctx, sub, fonts_get_system_font(TILE_SUBFONT),
                       GRect(text.origin.x, text.origin.y + text.size.h - 18, text.size.w, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void prv_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  AppModel *m = model_get();
  const int cells = prv_cell_count();

  if (m->favorite_count == 0) {
    // Hint row where the favourites would be.
    const int rows = prv_favorite_rows() + 1;
    const GRect area = grect_inset(bounds, GEdgeInsets(PBL_IF_ROUND_ELSE(14, 2)));
    GRect hint = GRect(area.origin.x, area.origin.y, area.size.w, area.size.h / rows);
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    graphics_draw_text(ctx, STR(S_NO_FAVORITES),
                       fonts_get_system_font(FONT_KEY_GOTHIC_14), grect_inset(hint, GEdgeInsets(6, 10)),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }

  for (int cell = 0; cell < cells; cell++) {
    const GRect r = prv_cell_rect(bounds, cell);
    const int kind = prv_cell_kind(cell);
    const bool selected = cell == s_selected;
    if (kind >= 0) {
      const Favorite *f = &m->favorites[kind];
      GColor bg = PBL_IF_COLOR_ELSE(f->color ? (GColor) { .argb = f->color } : GColorLightGray, GColorWhite);
      const char *title = f->description[0] ? f->description : (f->project_name[0] ? f->project_name : STR(S_FAVORITE));
      // Small tiles (144x168 watches) have no room for a second line.
      const char *sub = (f->description[0] && r.size.h >= 56) ? f->project_name : "";
      prv_draw_tile(ctx, r, bg, title, sub, selected);
    } else {
      prv_draw_tile(ctx, r, GColorWhite, kind == CELL_DICTATE ? STR(S_DICTATE) : STR(S_LIST), NULL, selected);
    }
  }
}

static void prv_activate(int cell) {
  const int kind = prv_cell_kind(cell);
  if (kind >= 0) {
    const Favorite *f = &model_get()->favorites[kind];
    model_set_message(STR(S_STARTING), false);
    comm_send_start(f->project_id, f->description);
    window_stack_pop(true);
  } else if (kind == CELL_DICTATE) {
    dictation_start();
  } else {
    list_window_push();
  }
}

static void prv_up_click(ClickRecognizerRef recognizer, void *context) {
  s_selected = (s_selected + prv_cell_count() - 1) % prv_cell_count();
  layer_mark_dirty(s_canvas);
}

static void prv_down_click(ClickRecognizerRef recognizer, void *context) {
  s_selected = (s_selected + 1) % prv_cell_count();
  layer_mark_dirty(s_canvas);
}

static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  prv_activate(s_selected);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
}

#ifdef PBL_TOUCH
static void prv_tap_handler(const Recognizer *recognizer, RecognizerEvent event) {
  if (event != RecognizerEvent_Completed || !s_canvas) {
    return;
  }
  const GPoint p = tap_recognizer_get_tap_point(recognizer);
  const GRect bounds = layer_get_bounds(s_canvas);
  for (int cell = 0; cell < prv_cell_count(); cell++) {
    if (grect_contains_point(&(GRect) { prv_cell_rect(bounds, cell).origin, prv_cell_rect(bounds, cell).size }, &p)) {
      s_selected = cell;
      layer_mark_dirty(s_canvas);
      prv_activate(cell);
      return;
    }
  }
}
#endif

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, prv_update_proc);
  layer_add_child(root, s_canvas);
  s_selected = 0;
#ifdef PBL_TOUCH
  window_set_touch_bridge_disabled(window, true);
  window_attach_recognizer(window, tap_recognizer_create(prv_tap_handler, NULL));
#endif
}

static void prv_window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
  window_destroy(window);
  s_window = NULL;
}

void favorites_window_push(void) {
  if (s_window) {
    return;
  }
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}

void favorites_window_refresh(void) {
  if (s_canvas) {
    if (s_selected >= prv_cell_count()) {
      s_selected = 0;
    }
    layer_mark_dirty(s_canvas);
  }
}

void favorites_window_close(void) {
  if (s_window) {
    window_stack_remove(s_window, false);   // unload destroys it
  }
}
