#include "settings.h"
#include "config.h"

#define PERSIST_KEY_SETTINGS 1

typedef enum {
  RowScreen = 0,
  RowRot,
  RowCamera,
  RowSpeed,
  RowButtons,
  RowAudio,
  RowTip,
  RowLog,
  RowBench,
  RowPanel,
  RowReset,
  RowQuit,
  RowCount,
} Row;

static Settings s_set = {
  .screen = ScreenSpiel,
  .rot90 = 0,
  .camera = 0,
  .speed_idx = SPEED_DEFAULT_IDX,
  .thumb_mode = 1,
  // Standard aus: Ein offener Stream laesst den Verstaerker auch bei digitaler
  // Stille rauschen, und Phase 1 braucht keinen Ton. Wer die Last messen will,
  // schaltet ihn hier ein.
  .audio_mode = AudioOff,
  .quiet_log = 0,
  .show_tip = 1,
};

static SettingsHooks s_hooks;
static Window *s_window;
static MenuLayer *s_menu;
static bool s_open;
static bool s_dirty;

static const char *s_screen_names[ScreenCount] = { "Spiel", "Magnet-Uebung", "Panel-Test", "Messwerte" };
static const char *s_audio_names[AudioModeCount] = { "aus", "Stille", "Ton" };
static const uint16_t s_speed_show[SPEED_STEPS] = { 70, 85, 100, 120 };

static uint16_t prv_num_rows(struct MenuLayer *m, uint16_t section, void *ctx) {
  return RowCount;
}

static int16_t prv_header_height(struct MenuLayer *m, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  menu_cell_basic_header_draw(ctx, cell, "SILBERKUGEL P1");
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  char val[24];
  const char *title = "";
  switch (idx->row) {
    case RowScreen:
      title = "Bildschirm";
      snprintf(val, sizeof(val), "%s", s_screen_names[s_set.screen % ScreenCount]);
      break;
    case RowRot:
      title = "Ansicht";
      snprintf(val, sizeof(val), "%s", s_set.rot90 ? "quer gedreht" : "hoch");
      break;
    case RowCamera:
      title = "Kamera";
      snprintf(val, sizeof(val), "%s", s_set.camera ? "folgt der Kugel" : "fest unten");
      break;
    case RowSpeed:
      title = "Tempo";
      snprintf(val, sizeof(val), "%u Prozent", (unsigned)s_speed_show[s_set.speed_idx % SPEED_STEPS]);
      break;
    case RowButtons:
      title = "Tasten";
      snprintf(val, sizeof(val), "%s", s_set.thumb_mode ? "Daumen (Up/Down)" : "Zange (Back/Down)");
      break;
    case RowAudio:
      title = "Tonlast";
      snprintf(val, sizeof(val), "%s", s_audio_names[s_set.audio_mode % AudioModeCount]);
      break;
    case RowTip:
      title = "Fingerkuppe";
      snprintf(val, sizeof(val), "%s", s_set.show_tip ? "Umriss zeigen" : "aus");
      break;
    case RowLog:
      title = "Sekundenlog";
      snprintf(val, sizeof(val), "%s", s_set.quiet_log ? "aus" : "an");
      break;
    case RowBench:
      title = "Physik-Test";
      snprintf(val, sizeof(val), "Select startet");
      break;
    case RowPanel:
      title = "Panel-Test";
      snprintf(val, sizeof(val), "Select startet");
      break;
    case RowReset:
      title = "Zaehler";
      snprintf(val, sizeof(val), "Select setzt zurueck");
      break;
    default:
      // Back oeffnet im Spiel das Menue und fuehrt hier zurueck; ohne diesen
      // Eintrag gaebe es keinen Weg aus der App.
      title = "App beenden";
      snprintf(val, sizeof(val), "Select beendet");
      break;
  }
  menu_cell_basic_draw(ctx, cell, title, val, NULL);
}

static void prv_select(struct MenuLayer *m, MenuIndex *idx, void *ctx) {
  switch (idx->row) {
    case RowScreen:
      s_set.screen = (uint8_t)((s_set.screen + 1) % ScreenCount);
      break;
    case RowRot:
      s_set.rot90 = !s_set.rot90;
      break;
    case RowCamera:
      s_set.camera = !s_set.camera;
      break;
    case RowSpeed:
      s_set.speed_idx = (uint8_t)((s_set.speed_idx + 1) % SPEED_STEPS);
      break;
    case RowButtons:
      s_set.thumb_mode = !s_set.thumb_mode;
      break;
    case RowAudio:
      s_set.audio_mode = (uint8_t)((s_set.audio_mode + 1) % AudioModeCount);
      break;
    case RowTip:
      s_set.show_tip = !s_set.show_tip;
      break;
    case RowLog:
      s_set.quiet_log = !s_set.quiet_log;
      break;
    case RowBench:
      if (s_hooks.action) {
        s_hooks.action(0);
      }
      return;
    case RowPanel:
      if (s_hooks.action) {
        s_hooks.action(1);
      }
      window_stack_pop(true);   // Der Panel-Test braucht das Bild
      return;
    case RowReset:
      if (s_hooks.action) {
        s_hooks.action(2);
      }
      return;
    default:
      settings_save();
      if (s_hooks.action) {
        s_hooks.action(3);
      }
      return;
  }
  s_dirty = true;
  if (s_hooks.apply) {
    s_hooks.apply(&s_set);
  }
  layer_mark_dirty(menu_layer_get_layer(s_menu));
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_num_rows,
    .get_header_height = prv_header_height,
    .draw_header = prv_draw_header,
    .draw_row = prv_draw_row,
    .select_click = prv_select,
  });
  menu_layer_set_click_config_onto_window(s_menu, window);
  menu_layer_set_normal_colors(s_menu, GColorBlack, GColorWhite);
  menu_layer_set_highlight_colors(s_menu, GColorWhite, GColorBlack);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void prv_window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  s_open = false;
  settings_save();
}

void settings_init(const SettingsHooks *hooks) {
  s_hooks = *hooks;
  if (persist_exists(PERSIST_KEY_SETTINGS)) {
    Settings tmp;
    int n = persist_read_data(PERSIST_KEY_SETTINGS, &tmp, sizeof(tmp));
    if (n == (int)sizeof(tmp)) {
      // Gespeichertes darf nie ausserhalb der gueltigen Bereiche landen: Ein
      // alter Stand mit anderen Zaehlungen wuerde sonst ins Leere greifen.
      s_set.screen = (uint8_t)(tmp.screen % ScreenCount);
      s_set.rot90 = tmp.rot90 ? 1 : 0;
      s_set.camera = tmp.camera ? 1 : 0;
      s_set.speed_idx = (uint8_t)(tmp.speed_idx % SPEED_STEPS);
      s_set.thumb_mode = tmp.thumb_mode ? 1 : 0;
      s_set.audio_mode = (uint8_t)(tmp.audio_mode % AudioModeCount);
      s_set.quiet_log = tmp.quiet_log ? 1 : 0;
      s_set.show_tip = tmp.show_tip ? 1 : 0;
    }
  }
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
}

void settings_deinit(void) {
  settings_save();
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}

void settings_show(void) {
  if (s_open || !s_window) {
    return;
  }
  s_open = true;
  window_stack_push(s_window, true);
}

bool settings_is_open(void) {
  return s_open;
}

Settings *settings_get(void) {
  return &s_set;
}

void settings_save(void) {
  if (!s_dirty) {
    return;
  }
  persist_write_data(PERSIST_KEY_SETTINGS, &s_set, sizeof(s_set));
  s_dirty = false;
  APP_LOG(APP_LOG_LEVEL_INFO, "[P1] Einstellungen gespeichert");
}
