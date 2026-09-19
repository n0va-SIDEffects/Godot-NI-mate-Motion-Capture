// Geruest fuer eine Pebble-Watchapp (C-SDK 4.x).
//
// Enthaelt die Teile, die fast jede App braucht, jeweils an der Stelle, an die
// sie gehoert: Fensterlebenszyklus mit vollstaendigem Aufraeumen, eigenes
// Zeichnen, Tasten (kurz und lang), Minutentakt, Einstellungen im Flash und
// AppMessage vom Handy. Kopieren, ausduennen, umbenennen.
//
// Gebaut und geprueft mit SDK 4.33.1 fuer aplite, basalt, chalk, diorite,
// flint, emery und gabbro.

#include <pebble.h>

// ---------------------------------------------------------------- Einstellungen

// Eine einzige Struktur, ein einziger Persist-Schluessel. Neue Felder hinten
// anhaengen und beim Lesen die Groesse pruefen, dann ueberleben alte
// Installationen ein Update.
#define SETTINGS_KEY 1

typedef struct {
  GColor accent;
  bool   show_seconds;
} Settings;

static Settings s_settings;

static void settings_load(void) {
  // Zuerst die Standardwerte, damit auch ein unvollstaendiger Datensatz passt.
  s_settings.accent       = PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorWhite);
  s_settings.show_seconds = false;

  if (persist_exists(SETTINGS_KEY)) {
    persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
  }
}

static void settings_save(void) {
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

// ---------------------------------------------------------------------- Zustand

static Window    *s_window;
static Layer     *s_canvas_layer;
static TextLayer *s_time_layer;
static char       s_time_buf[16];   // statisch, nicht auf dem Stack

static void update_time(void) {
  time_t now = time(NULL);
  struct tm *tick = localtime(&now);

  strftime(s_time_buf, sizeof(s_time_buf),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", tick);
  text_layer_set_text(s_time_layer, s_time_buf);

  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

// ---------------------------------------------------------------- Eigenes Zeichnen

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // Immer aus den Grenzen des Layers rechnen, nie mit festen Pixelwerten:
  // sieben Plattformen von 144x168 bis 260x260, rund und eckig.
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, s_settings.accent);

  // Gezeichnete Flaechen kosten keinen Heap - ein Vollbild-GBitmap dagegen
  // 45 k auf emery und 66 k auf gabbro.
  GRect bar = GRect(bounds.origin.x,
                    bounds.origin.y + bounds.size.h * 3 / 4,
                    bounds.size.w,
                    bounds.size.h / 24);
  graphics_fill_rect(ctx, bar, 0, GCornerNone);
}

// ------------------------------------------------------------------------ Tasten

static void tick_handler(struct tm *tick_time, TimeUnits units_changed);

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_settings.show_seconds = !s_settings.show_seconds;
  // Sekundentakt nur solange, wie er wirklich zu sehen ist - er kostet Akku.
  tick_timer_service_subscribe(s_settings.show_seconds ? SECOND_UNIT : MINUTE_UNIT,
                               tick_handler);
  update_time();
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  vibes_short_pulse();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  // Langes Druecken: 0 = Standarddauer (rund 500 ms).
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, select_long_click_handler, NULL);
}

// ------------------------------------------------------------------------- Takt

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

// ------------------------------------------------------------------- AppMessage

static void inbox_received(DictionaryIterator *iter, void *context) {
  bool dirty = false;

  Tuple *accent = dict_find(iter, MESSAGE_KEY_Accent);
  if (accent) {
    s_settings.accent = GColorFromHEX(accent->value->int32);
    dirty = true;
  }

  Tuple *seconds = dict_find(iter, MESSAGE_KEY_ShowSeconds);
  if (seconds) {
    s_settings.show_seconds = seconds->value->int32 != 0;
    dirty = true;
  }

  if (dirty) {
    // Sofort sichern: beim naechsten Start ist das Handy vielleicht nicht da.
    settings_save();
    window_set_background_color(s_window, GColorBlack);
    layer_mark_dirty(s_canvas_layer);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  // Ohne diesen Callback verschwinden zu grosse Nachrichten lautlos.
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox verworfen: %d", (int)reason);
}

static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason,
                          void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Senden fehlgeschlagen: %d", (int)reason);
}

static void outbox_sent(DictionaryIterator *iter, void *context) {
  // Erst hier darf die naechste Nachricht raus, sonst gibt es APP_MSG_BUSY.
}

// --------------------------------------------------------- Fensterlebenszyklus

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  // unobstructed: Timeline Quick View verdeckt sonst den unteren Rand.
  GRect bounds = layer_get_unobstructed_bounds(root);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(root, s_canvas_layer);

  GRect text_frame = GRect(bounds.origin.x,
                           bounds.origin.y + bounds.size.h / 3,
                           bounds.size.w,
                           bounds.size.h / 3);
  s_time_layer = text_layer_create(text_frame);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  update_time();
}

static void window_unload(Window *window) {
  // Spiegelbild von window_load, umgekehrte Reihenfolge, restlos.
  // Was hier fehlt, leckt bei jedem Oeffnen erneut.
  text_layer_destroy(s_time_layer);
  s_time_layer = NULL;

  layer_destroy(s_canvas_layer);
  s_canvas_layer = NULL;
}

// -------------------------------------------------------------------- Programm

static void init(void) {
  settings_load();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  app_message_register_outbox_sent(outbox_sent);
  // Puffer aus der groessten Nachricht ausrechnen, nicht raten.
  app_message_open(128, 128);

  APP_LOG(APP_LOG_LEVEL_INFO, "Heap frei nach init: %u",
          (unsigned)heap_bytes_free());
}

static void deinit(void) {
  settings_save();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();   // blockiert bis zum Beenden der App
  deinit();
  return 0;
}
