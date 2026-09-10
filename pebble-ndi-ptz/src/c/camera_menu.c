/*
 * Camera list (main window).
 */
#include "camera_menu.h"
#include "comm.h"
#include "control_window.h"

#define FIRST_RESPONSE_TIMEOUT_MS 8000

static Window *s_window;
static MenuLayer *s_menu;
static AppTimer *s_timeout_timer;
static bool s_requested;

static uint16_t prv_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  return g_app.camera_count > 0 ? g_app.camera_count : 1;
}

static int16_t prv_header_height(MenuLayer *menu, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell_layer, uint16_t section, void *data) {
  menu_cell_basic_header_draw(ctx, cell_layer, "NDI PTZ Remote");
}

static const char *prv_status_title(void) {
  switch (g_app.status) {
    case STATUS_LOADING: return "Suche Kameras...";
    case STATUS_BRIDGE_UNREACHABLE: return "Bridge nicht erreichbar";
    case STATUS_NO_CAMERAS: return "Keine PTZ-Kameras";
    case STATUS_NOT_CONFIGURED: return "Bridge nicht konfiguriert";
    case STATUS_CMD_FAILED: return "Fehler";
    default: return "Keine Kameras";
  }
}

static const char *prv_status_subtitle(void) {
  if (g_app.status_text[0]) return g_app.status_text;
  switch (g_app.status) {
    case STATUS_LOADING: return "Bitte warten";
    case STATUS_BRIDGE_UNREACHABLE: return "Bridge läuft? Gleiches WLAN?";
    case STATUS_NO_CAMERAS: return "Select lang: neu suchen";
    case STATUS_NOT_CONFIGURED: return "Einstellungen in Pebble-App";
    default: return "Select lang: neu suchen";
  }
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *idx, void *data) {
  if (g_app.camera_count == 0) {
    menu_cell_basic_draw(ctx, cell_layer, prv_status_title(), prv_status_subtitle(), NULL);
    return;
  }
  const PtzCamera *cam = &g_app.cameras[idx->row];
  const char *title = cam->name[0] ? cam->name : "(lädt...)";
  const char *sub = cam->ptz ? "PTZ" : "kein PTZ";
  menu_cell_basic_draw(ctx, cell_layer, title, sub, NULL);
}

static void prv_select(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  if (g_app.camera_count == 0) {
    comm_request_cameras();
    return;
  }
  g_app.selected_camera = (int8_t)idx->row;
  persist_write_int(PERSIST_KEY_LAST_CAM, idx->row);
  control_window_push();
}

static void prv_select_long(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  g_app.camera_count = 0;
  g_app.status = STATUS_LOADING;
  g_app.status_text[0] = '\0';
  camera_menu_reload();
  comm_request_refresh();
  vibes_short_pulse();
}

static void prv_timeout(void *data) {
  s_timeout_timer = NULL;
  if (!comm_is_connected() && g_app.camera_count == 0) {
    g_app.status = STATUS_BRIDGE_UNREACHABLE;
    strncpy(g_app.status_text, "Keine Antwort vom Handy", STATUS_TEXT_LEN - 1);
    camera_menu_reload();
  }
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = prv_num_rows,
    .get_header_height = prv_header_height,
    .draw_header = prv_draw_header,
    .draw_row = prv_draw_row,
    .select_click = prv_select,
    .select_long_click = prv_select_long,
  });
#ifdef PBL_COLOR
  menu_layer_set_highlight_colors(s_menu, GColorDarkCandyAppleRed, GColorWhite);
#endif
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void prv_window_appear(Window *window) {
  if (!s_requested) {
    s_requested = true;
    comm_request_cameras();
    s_timeout_timer = app_timer_register(FIRST_RESPONSE_TIMEOUT_MS, prv_timeout, NULL);
  }
  /* preselect the last used camera */
  if (g_app.camera_count > 0 && persist_exists(PERSIST_KEY_LAST_CAM)) {
    int32_t last = persist_read_int(PERSIST_KEY_LAST_CAM);
    if (last >= 0 && last < g_app.camera_count) {
      menu_layer_set_selected_index(s_menu, (MenuIndex){ .section = 0, .row = (uint16_t)last },
                                    MenuRowAlignCenter, false);
    }
  }
}

static void prv_window_unload(Window *window) {
  if (s_timeout_timer) {
    app_timer_cancel(s_timeout_timer);
    s_timeout_timer = NULL;
  }
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void camera_menu_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_window_load,
      .appear = prv_window_appear,
      .unload = prv_window_unload,
    });
  }
  window_stack_push(s_window, true);
}

void camera_menu_reload(void) {
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
}
