#include "list_window.h"
#include "comm.h"
#include "favorites_window.h"
#include "i18n.h"
#include "model.h"

// Picker for a new time entry: recently used entries first, then all projects.
// In "text mode" (after dictation) only the project is picked for a given text.

static Window *s_window;
static MenuLayer *s_menu;
static bool s_text_mode;
static char s_text[DESC_LEN];

static bool prv_has_recent(void) {
  return !s_text_mode && model_get()->recent_count > 0;
}

static bool prv_is_recent_section(uint16_t section) {
  return prv_has_recent() && section == 0;
}

static uint16_t prv_get_num_sections(MenuLayer *menu, void *context) {
  return prv_has_recent() ? 2 : 1;
}

static uint16_t prv_get_num_rows(MenuLayer *menu, uint16_t section, void *context) {
  AppModel *m = model_get();
  if (prv_is_recent_section(section)) {
    return m->recent_count;
  }
  return m->project_count + 1;   // + "Ohne Projekt"
}

static int16_t prv_get_header_height(MenuLayer *menu, uint16_t section, void *context) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell_layer, uint16_t section, void *context) {
  const char *title = prv_is_recent_section(section) ? STR(S_RECENT) : (s_text_mode ? STR(S_PICK_PROJECT) : STR(S_PROJECTS));
  menu_cell_basic_header_draw(ctx, cell_layer, title);
}

static void prv_draw_color_bar(GContext *ctx, const Layer *cell_layer, uint8_t color) {
#ifdef PBL_COLOR
  if (color) {
    const GRect b = layer_get_bounds(cell_layer);
    graphics_context_set_fill_color(ctx, (GColor) { .argb = color });
    graphics_fill_rect(ctx, GRect(0, 0, 4, b.size.h), 0, GCornerNone);
  }
#endif
}

static void prv_format_hm(char *buf, size_t len, int32_t seconds) {
  snprintf(buf, len, "%d:%02d", (int)(seconds / 3600), (int)((seconds % 3600) / 60));
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *index, void *context) {
  AppModel *m = model_get();
  if (prv_is_recent_section(index->section)) {
    if (index->row >= m->recent_count) {
      return;
    }
    const RecentEntry *e = &m->recent[index->row];
    char sub[NAME_LEN + 24];
    const char *project = e->project_name[0] ? e->project_name : STR(S_NO_PROJECT);
    if (e->today_seconds > 0) {
      char hm[16];
      char today[24];
      prv_format_hm(hm, sizeof(hm), e->today_seconds);
      snprintf(today, sizeof(today), STR(S_TODAY_H), hm);
      snprintf(sub, sizeof(sub), "%s · %s", project, today);
    } else {
      strncpy(sub, project, sizeof(sub) - 1);
      sub[sizeof(sub) - 1] = '\0';
    }
    menu_cell_basic_draw(ctx, cell_layer, e->description[0] ? e->description : STR(S_NO_DESCRIPTION), sub, NULL);
    prv_draw_color_bar(ctx, cell_layer, e->color);
    return;
  }
  if (index->row == 0) {
    menu_cell_basic_draw(ctx, cell_layer, STR(S_NO_PROJECT), s_text_mode ? s_text : STR(S_START_EMPTY), NULL);
    return;
  }
  if (index->row - 1 >= m->project_count) {
    return;
  }
  const Project *p = &m->projects[index->row - 1];
  menu_cell_basic_draw(ctx, cell_layer, p->name, s_text_mode ? s_text : NULL, NULL);
  prv_draw_color_bar(ctx, cell_layer, p->color);
}

static void prv_select_click(MenuLayer *menu, MenuIndex *index, void *context) {
  AppModel *m = model_get();
  int32_t project_id = 0;
  const char *description = s_text_mode ? s_text : "";

  if (prv_is_recent_section(index->section)) {
    if (index->row < m->recent_count) {
      project_id = m->recent[index->row].project_id;
      description = m->recent[index->row].description;
    }
  } else if (index->row > 0 && index->row - 1 < m->project_count) {
    project_id = m->projects[index->row - 1].id;
  }

  model_set_message(STR(S_STARTING), false);
  comm_send_start(project_id, description);
  window_stack_pop(true);
  favorites_window_close();   // back to the status screen in one go
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_sections = prv_get_num_sections,
    .get_num_rows = prv_get_num_rows,
    .get_header_height = prv_get_header_height,
    .draw_header = prv_draw_header,
    .draw_row = prv_draw_row,
    .select_click = prv_select_click,
  });
  menu_layer_set_highlight_colors(s_menu, PBL_IF_COLOR_ELSE(GColorFolly, GColorBlack), GColorWhite);
  menu_layer_set_center_focused(s_menu, PBL_IF_ROUND_ELSE(true, false));
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void prv_window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  window_destroy(window);
  s_window = NULL;
}

static void prv_push(void) {
  if (s_window) {
    menu_layer_reload_data(s_menu);
    return;
  }
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}

void list_window_push(void) {
  s_text_mode = false;
  prv_push();
}

void list_window_push_for_text(const char *text) {
  s_text_mode = true;
  strncpy(s_text, text ? text : "", DESC_LEN - 1);
  s_text[DESC_LEN - 1] = '\0';
  prv_push();
}

void list_window_refresh(void) {
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
}
