#include <pebble.h>

// Starter: one window, a text line, three buttons and an AppMessage link.
// Replace the UUID in package.json (python3 -c "import uuid; print(uuid.uuid4())").

enum { CMD_HELLO = 1, CMD_TEXT = 10 };

static Window *s_window;
static TextLayer *s_text;
static char s_buf[64] = "Warte auf Handy…";

static void prv_show(const char *text) {
  strncpy(s_buf, text, sizeof(s_buf) - 1);
  s_buf[sizeof(s_buf) - 1] = '\0';
  text_layer_set_text(s_text, s_buf);
}

static void prv_send(int32_t cmd) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_CMD, cmd);
  dict_write_end(iter);
  app_message_outbox_send();
}

static void prv_inbox(DictionaryIterator *iter, void *ctx) {
  Tuple *cmd = dict_find(iter, MESSAGE_KEY_CMD);
  Tuple *text = dict_find(iter, MESSAGE_KEY_TEXT);
  if (cmd && cmd->value->int32 == CMD_TEXT && text && text->type == TUPLE_CSTRING) {
    prv_show(text->value->cstring);
  }
}

static void prv_select_click(ClickRecognizerRef r, void *ctx) {
  prv_show("Frage Handy…");
  prv_send(CMD_HELLO);
}

static void prv_up_click(ClickRecognizerRef r, void *ctx)   { prv_show("UP"); }
static void prv_down_click(ClickRecognizerRef r, void *ctx) { prv_show("DOWN"); }

static void prv_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  int pad = PBL_IF_ROUND_ELSE(24, 8);
  s_text = text_layer_create(GRect(pad, b.size.h / 2 - 30, b.size.w - 2 * pad, 60));
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_text, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_text, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_text, s_buf);
  layer_add_child(root, text_layer_get_layer(s_text));
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_text);
}

static void prv_init(void) {
  app_message_register_inbox_received(prv_inbox);
  app_message_open(512, 128);

  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}

static void prv_deinit(void) {
  window_stack_pop_all(false);
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
  return 0;
}
