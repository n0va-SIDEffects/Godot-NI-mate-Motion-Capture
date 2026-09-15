#include "win_menu.h"
#include "win_gyro.h"
#include "comm.h"
#include "settings.h"
#include "preview.h"

// ===========================================================================
// Bestaetigung vor dem Ueberschreiben eines Presets
// ===========================================================================

static Window    *s_confirm_window;
static TextLayer *s_confirm_text;
static char       s_confirm_body[96];
static uint8_t    s_confirm_preset;

static void confirm_yes(ClickRecognizerRef rec, void *ctx) {
  comm_send_command(PTZ_CMD_PRESET_STORE, s_confirm_preset);
  if (settings_get()->vibrate) vibes_short_pulse();
  window_stack_pop(true);
}

static void confirm_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, confirm_yes);
}

static void confirm_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  s_confirm_text = text_layer_create(GRect(6, 10, b.size.w - 12, b.size.h - 20));
  text_layer_set_text(s_confirm_text, s_confirm_body);
  text_layer_set_font(s_confirm_text, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_confirm_text, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_confirm_text, GTextOverflowModeWordWrap);
  layer_add_child(root, text_layer_get_layer(s_confirm_text));
}

static void confirm_unload(Window *window) {
  text_layer_destroy(s_confirm_text);
  window_destroy(s_confirm_window);
  s_confirm_window = NULL;
}

static void confirm_push(uint8_t preset) {
  s_confirm_preset = preset;
  snprintf(s_confirm_body, sizeof(s_confirm_body),
           "Preset %d mit der aktuellen Kameraposition überschreiben?"
           "\n\nSELECT = ja\nBACK = nein",
           preset + 1);
  if (!s_confirm_window) {
    s_confirm_window = window_create();
    window_set_click_config_provider(s_confirm_window, confirm_click_config);
    window_set_window_handlers(s_confirm_window, (WindowHandlers) {
      .load = confirm_load,
      .unload = confirm_unload,
    });
  }
  window_stack_push(s_confirm_window, true);
}

// ===========================================================================
// Presets
// ===========================================================================

static Window    *s_preset_window;
static MenuLayer *s_preset_menu;

static uint16_t preset_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  return settings_get()->preset_count;
}

static void preset_draw(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  char title[20];
  snprintf(title, sizeof(title), "Preset %d", idx->row + 1);
  menu_cell_basic_draw(ctx, cell, title, NULL, NULL);
}

static void preset_select(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  comm_send_command(PTZ_CMD_PRESET_RECALL, idx->row);
  // Zwei Sekunden reichen den meisten Kameras fuer eine Preset-Fahrt.
  preview_request_delayed(2000);
  window_stack_pop(true);
}

static void preset_long_select(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  confirm_push((uint8_t)idx->row);
}

static void preset_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_preset_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_preset_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = preset_rows,
    .draw_row = preset_draw,
    .select_click = preset_select,
    .select_long_click = preset_long_select,
  });
  menu_layer_set_click_config_onto_window(s_preset_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_preset_menu));
}

static void preset_unload(Window *window) {
  menu_layer_destroy(s_preset_menu);
  window_destroy(s_preset_window);
  s_preset_window = NULL;
}

static void preset_push(void) {
  if (!s_preset_window) {
    s_preset_window = window_create();
    window_set_window_handlers(s_preset_window, (WindowHandlers) {
      .load = preset_load,
      .unload = preset_unload,
    });
  }
  window_stack_push(s_preset_window, true);
}

// ===========================================================================
// Kamerawahl
// ===========================================================================

static Window    *s_cam_window;
static MenuLayer *s_cam_menu;

static uint16_t cam_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  uint8_t n = comm_get_cam_count();
  return n > 0 ? n : 1;
}

static void cam_draw(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  if (comm_get_cam_count() == 0) {
    menu_cell_basic_draw(ctx, cell, "Keine Kamera",
                         "In der Telefon-App einrichten", NULL);
    return;
  }
  const char *name = comm_get_cam_name((uint8_t)idx->row);
  const bool active = idx->row == settings_get()->active_cam;
  menu_cell_basic_draw(ctx, cell, name, active ? "aktiv" : NULL, NULL);
}

static void cam_select(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  if (comm_get_cam_count() == 0) {
    return;
  }
  settings_get()->active_cam = (uint8_t)idx->row;
  settings_save();
  // Der Wechsel haelt zuerst die bisherige Kamera an, damit keine Achse
  // unbeaufsichtigt weiterlaeuft.
  comm_stop_all();
  comm_send_command(PTZ_CMD_SELECT_CAM, idx->row);
  preview_cancel();
  preview_request_delayed(600);
  window_stack_pop(true);
}

static void cam_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_cam_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_cam_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = cam_rows,
    .draw_row = cam_draw,
    .select_click = cam_select,
  });
  menu_layer_set_click_config_onto_window(s_cam_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_cam_menu));
}

static void cam_unload(Window *window) {
  menu_layer_destroy(s_cam_menu);
  window_destroy(s_cam_window);
  s_cam_window = NULL;
}

static void cam_push(void) {
  if (!s_cam_window) {
    s_cam_window = window_create();
    window_set_window_handlers(s_cam_window, (WindowHandlers) {
      .load = cam_load,
      .unload = cam_unload,
    });
  }
  window_stack_push(s_cam_window, true);
}

// ===========================================================================
// Hauptmenue
// ===========================================================================

static Window          *s_menu_window;
static SimpleMenuLayer *s_menu_layer;
static SimpleMenuItem   s_items[7];
static SimpleMenuSection s_sections[1];

static char s_sub_gyro[24];
static char s_sub_cam[PTZ_NAME_LEN + 1];
static char s_sub_speed[24];
static char s_sub_focus[24];
static char s_sub_status[64];
static char s_sub_preview[32];
static bool s_autofocus = true;

static void refresh_subtitles(void) {
  snprintf(s_sub_gyro, sizeof(s_sub_gyro), "%s", gyro_axes_name((GyroAxes)settings_get()->gyro_axes));
  snprintf(s_sub_cam, sizeof(s_sub_cam), "%s", comm_get_cam_name(settings_get()->active_cam));
  snprintf(s_sub_speed, sizeof(s_sub_speed), "Stufe %d von 5", settings_get()->speed);
  snprintf(s_sub_focus, sizeof(s_sub_focus), "%s", s_autofocus ? "automatisch" : "manuell");
  snprintf(s_sub_status, sizeof(s_sub_status), "%s", comm_get_message());
  if (preview_enabled()) {
    static const char *groessen[] = { "klein", "mittel", "gross" };
    snprintf(s_sub_preview, sizeof(s_sub_preview), "an, %s",
             groessen[settings_get()->preview_size % 3]);
  } else {
    snprintf(s_sub_preview, sizeof(s_sub_preview), "aus");
  }
  if (s_menu_layer) {
    layer_mark_dirty(simple_menu_layer_get_layer(s_menu_layer));
  }
}

static void item_gyro(int index, void *ctx)   { win_gyro_push(); }
static void item_preset(int index, void *ctx) { preset_push(); }
static void item_cam(int index, void *ctx)    { cam_push(); }

static void item_speed(int index, void *ctx) {
  PtzSettings *cfg = settings_get();
  cfg->speed = (uint8_t)(cfg->speed % 5 + 1);
  settings_save();
  refresh_subtitles();
}

static void item_focus(int index, void *ctx) {
  s_autofocus = !s_autofocus;
  comm_send_command(PTZ_CMD_AUTOFOCUS, s_autofocus ? 1 : 0);
  refresh_subtitles();
}

static void item_preview(int index, void *ctx) {
  bool on = !preview_enabled();
  preview_set_enabled(on);
  if (on) {
    preview_request();
  }
  refresh_subtitles();
}

static void item_status(int index, void *ctx) {
  // Erneut nachfragen - praktisch, wenn die Kamera zwischenzeitlich
  // ans Netz gegangen ist.
  comm_send_command(PTZ_CMD_HELLO, 0);
  refresh_subtitles();
}

static void menu_load(Window *window) {
  refresh_subtitles();

  s_items[0] = (SimpleMenuItem) {
    .title = "Neigung", .subtitle = s_sub_gyro, .callback = item_gyro };
  s_items[1] = (SimpleMenuItem) {
    .title = "Presets", .subtitle = "Abrufen, lang = speichern", .callback = item_preset };
  s_items[2] = (SimpleMenuItem) {
    .title = "Kamera", .subtitle = s_sub_cam, .callback = item_cam };
  s_items[3] = (SimpleMenuItem) {
    .title = "Tempo", .subtitle = s_sub_speed, .callback = item_speed };
  s_items[4] = (SimpleMenuItem) {
    .title = "Fokus", .subtitle = s_sub_focus, .callback = item_focus };
  s_items[5] = (SimpleMenuItem) {
    .title = "Vorschau", .subtitle = s_sub_preview, .callback = item_preview };
  s_items[6] = (SimpleMenuItem) {
    .title = "Status", .subtitle = s_sub_status, .callback = item_status };

  s_sections[0] = (SimpleMenuSection) {
    .num_items = ARRAY_LENGTH(s_items), .items = s_items };

  Layer *root = window_get_root_layer(window);
  s_menu_layer = simple_menu_layer_create(layer_get_bounds(root), window,
                                          s_sections, ARRAY_LENGTH(s_sections), NULL);
  layer_add_child(root, simple_menu_layer_get_layer(s_menu_layer));
}

static void menu_appear(Window *window) {
  refresh_subtitles();
}

static void menu_unload(Window *window) {
  simple_menu_layer_destroy(s_menu_layer);
  s_menu_layer = NULL;
  window_destroy(s_menu_window);
  s_menu_window = NULL;
}

void win_menu_push(void) {
  if (!s_menu_window) {
    s_menu_window = window_create();
    window_set_window_handlers(s_menu_window, (WindowHandlers) {
      .load = menu_load,
      .appear = menu_appear,
      .unload = menu_unload,
    });
  }
  window_stack_push(s_menu_window, true);
}
