/*
 * Options menu (SELECT long-press in the control window) and the preset picker.
 */
#include "options_menu.h"
#include "comm.h"
#include "control_window.h"

enum {
  OPT_MOTION = 0,
  OPT_SPEED,
  OPT_PRESET_RECALL,
  OPT_PRESET_STORE,
  OPT_HOME,
  OPT_AUTOFOCUS,
  OPT_REFRESH,
  OPT_COUNT
};

#define PRESET_COUNT 9

static Window *s_window;
static MenuLayer *s_menu;

static Window *s_preset_window;
static MenuLayer *s_preset_menu;
static bool s_preset_store;   /* false = recall, true = store */

/* ---------------- preset picker ---------------- */

static uint16_t prv_preset_rows(MenuLayer *m, uint16_t section, void *ctx) {
  return PRESET_COUNT;
}

static int16_t prv_preset_header_height(MenuLayer *m, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_preset_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  menu_cell_basic_header_draw(ctx, cell, s_preset_store ? "Preset speichern" : "Preset abrufen");
}

static void prv_preset_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  static char title[16];
  snprintf(title, sizeof(title), "Preset %d", idx->row + 1);
  menu_cell_basic_draw(ctx, cell, title, NULL, NULL);
}

static void prv_preset_select(MenuLayer *m, MenuIndex *idx, void *ctx) {
  uint8_t preset = (uint8_t)(idx->row + 1);
  comm_send_preset(s_preset_store ? CMD_PRESET_STORE : CMD_PRESET_RECALL, g_app.selected_camera, preset);
  if (s_preset_store) {
    vibes_double_pulse();
  } else {
    vibes_short_pulse();
  }
  /* pop the preset picker and the options menu -> back to the control window */
  window_stack_pop(false);
  window_stack_pop(true);
}

static void prv_preset_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_preset_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_preset_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = prv_preset_rows,
    .get_header_height = prv_preset_header_height,
    .draw_header = prv_preset_draw_header,
    .draw_row = prv_preset_draw_row,
    .select_click = prv_preset_select,
  });
#ifdef PBL_COLOR
  menu_layer_set_highlight_colors(s_preset_menu, GColorDarkCandyAppleRed, GColorWhite);
#endif
  menu_layer_set_click_config_onto_window(s_preset_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_preset_menu));
}

static void prv_preset_unload(Window *window) {
  menu_layer_destroy(s_preset_menu);
  s_preset_menu = NULL;
  window_destroy(s_preset_window);
  s_preset_window = NULL;
}

static void prv_preset_push(bool store) {
  s_preset_store = store;
  if (!s_preset_window) {
    s_preset_window = window_create();
    window_set_window_handlers(s_preset_window, (WindowHandlers){
      .load = prv_preset_load,
      .unload = prv_preset_unload,
    });
  }
  window_stack_push(s_preset_window, true);
}

/* ---------------- options menu ---------------- */

static uint16_t prv_rows(MenuLayer *m, uint16_t section, void *ctx) {
  return OPT_COUNT;
}

static int16_t prv_header_height(MenuLayer *m, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  menu_cell_basic_header_draw(ctx, cell, "Optionen");
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  switch (idx->row) {
    case OPT_MOTION:
      menu_cell_basic_draw(ctx, cell, "Motion-Steuerung",
                           control_window_motion_mode() ? "An (Handgelenk)" : "Aus (Tasten)", NULL);
      break;
    case OPT_SPEED:
      menu_cell_basic_draw(ctx, cell, "Tempo", speed_label(g_app.settings.speed), NULL);
      break;
    case OPT_PRESET_RECALL:
      menu_cell_basic_draw(ctx, cell, "Preset abrufen", "Position 1-9", NULL);
      break;
    case OPT_PRESET_STORE:
      menu_cell_basic_draw(ctx, cell, "Preset speichern", "Aktuelle Position", NULL);
      break;
    case OPT_HOME:
      menu_cell_basic_draw(ctx, cell, "Home-Position", "Pan/Tilt auf 0/0", NULL);
      break;
    case OPT_AUTOFOCUS:
      menu_cell_basic_draw(ctx, cell, "Autofokus", "Einmal scharfstellen", NULL);
      break;
    case OPT_REFRESH:
      menu_cell_basic_draw(ctx, cell, "Kameras neu suchen", "NDI-Discovery", NULL);
      break;
    default:
      break;
  }
}

static void prv_select(MenuLayer *m, MenuIndex *idx, void *ctx) {
  switch (idx->row) {
    case OPT_MOTION:
      control_window_set_motion_mode(!control_window_motion_mode());
      menu_layer_reload_data(s_menu);
      break;
    case OPT_SPEED:
      g_app.settings.speed = (PtzSpeed)((g_app.settings.speed + 1) % SPEED_COUNT);
      settings_save();
      menu_layer_reload_data(s_menu);
      break;
    case OPT_PRESET_RECALL:
      prv_preset_push(false);
      break;
    case OPT_PRESET_STORE:
      prv_preset_push(true);
      break;
    case OPT_HOME:
      comm_send_simple(CMD_HOME, g_app.selected_camera);
      vibes_short_pulse();
      window_stack_pop(true);
      break;
    case OPT_AUTOFOCUS:
      comm_send_simple(CMD_AUTOFOCUS, g_app.selected_camera);
      vibes_short_pulse();
      window_stack_pop(true);
      break;
    case OPT_REFRESH:
      comm_request_refresh();
      vibes_short_pulse();
      window_stack_pop(true);
      break;
    default:
      break;
  }
}

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = prv_rows,
    .get_header_height = prv_header_height,
    .draw_header = prv_draw_header,
    .draw_row = prv_draw_row,
    .select_click = prv_select,
  });
#ifdef PBL_COLOR
  menu_layer_set_highlight_colors(s_menu, GColorDarkCandyAppleRed, GColorWhite);
#endif
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void prv_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void options_menu_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_load,
      .unload = prv_unload,
    });
  }
  window_stack_push(s_window, true);
}
