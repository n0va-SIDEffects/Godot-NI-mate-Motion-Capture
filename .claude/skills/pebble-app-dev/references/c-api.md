# Watch-side C: patterns that appear in almost every app

All signatures below were checked against `pebble.h`. Include only `<pebble.h>`;
`MESSAGE_KEY_*` and `RESOURCE_ID_*` come in through it automatically.

## App skeleton

```c
#include <pebble.h>

static Window *s_window;

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  // create layers, add them to root
}

static void prv_window_unload(Window *window) {
  // destroy layers created in load
}

static void prv_init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load, .unload = prv_window_unload,
    .appear = prv_appear, .disappear = prv_disappear,   // optional
  });
  window_stack_push(s_window, true);
}

static void prv_deinit(void) {
  window_stack_pop_all(false);      // unloads every pushed window first
  window_destroy(s_window);
}

int main(void) { prv_init(); app_event_loop(); prv_deinit(); return 0; }
```

Sub-windows pushed later may destroy themselves in `unload`
(`window_destroy(window); s_sub = NULL;`) and are created fresh on each push.

## Buttons

```c
static void prv_select_click(ClickRecognizerRef r, void *ctx) { ... }
static void prv_select_long(ClickRecognizerRef r, void *ctx) { ... }

static void prv_click_config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, prv_select_long, NULL);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, prv_up_repeat);
}
```

A `MenuLayer` takes the buttons over with
`menu_layer_set_click_config_onto_window(menu, window)`.

## Drawing on a custom layer

```c
static void prv_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, b.size.w, 20), 4, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(10, 10));
  graphics_fill_circle(ctx, GPoint(20, 20), 5);
  graphics_draw_arc(ctx, GRect(0, 0, 30, 30), GOvalScaleModeFitCircle,
                    DEG_TO_TRIGANGLE(0), DEG_TO_TRIGANGLE(270));   // not on aplite
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "Hallo", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(0, 30, b.size.w, 30), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}
// in load:
s_canvas = layer_create(bounds);
layer_set_update_proc(s_canvas, prv_update_proc);
layer_add_child(root, s_canvas);
// to redraw: layer_mark_dirty(s_canvas);
```

Angles: 0 = 12 o'clock, clockwise, `TRIG_MAX_ANGLE` = full turn.
`grect_inset(rect, GEdgeInsets(top, right, bottom, left))` shrinks a rect.
Filled shapes from points:

```c
static const GPathInfo PLAY_INFO = { 3, (GPoint[]) {{-5, -7}, {-5, 7}, {7, 0}} };
s_path = gpath_create(&PLAY_INFO);          // in load; gpath_destroy in unload
gpath_move_to(s_path, GPoint(cx, cy));
gpath_draw_filled(ctx, s_path);
```

Drawing your own icons in an update proc avoids PNG/PDC resources entirely —
handy for small action bars. The real `ActionBarLayer` needs `GBitmap` icons
from resources.

## TextLayer (when you don't need custom drawing)

```c
s_text = text_layer_create(GRect(0, 40, bounds.size.w, 40));
text_layer_set_text(s_text, "…");                  // pointer must stay valid!
text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
text_layer_set_text_alignment(s_text, GTextAlignmentCenter);
text_layer_set_overflow_mode(s_text, GTextOverflowModeTrailingEllipsis);
text_layer_set_background_color(s_text, GColorClear);
layer_add_child(root, text_layer_get_layer(s_text));
```

`text_layer_set_text` stores the pointer; use a `static char buf[]`, not a
stack buffer.

## MenuLayer

```c
static uint16_t prv_num_sections(MenuLayer *m, void *ctx) { return 2; }
static uint16_t prv_num_rows(MenuLayer *m, uint16_t section, void *ctx) { ... }
static int16_t  prv_header_height(MenuLayer *m, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}
static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *c) {
  menu_cell_basic_header_draw(ctx, cell, "Projekte");
}
static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *c) {
  menu_cell_basic_draw(ctx, cell, "Title", "subtitle or NULL", NULL /*GBitmap*/);
}
static void prv_select(MenuLayer *m, MenuIndex *idx, void *ctx) { ... }

s_menu = menu_layer_create(bounds);
menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
  .get_num_sections = prv_num_sections, .get_num_rows = prv_num_rows,
  .get_header_height = prv_header_height, .draw_header = prv_draw_header,
  .draw_row = prv_draw_row, .select_click = prv_select,
});
menu_layer_set_highlight_colors(s_menu, PBL_IF_COLOR_ELSE(GColorFolly, GColorBlack), GColorWhite);
menu_layer_set_center_focused(s_menu, PBL_IF_ROUND_ELSE(true, false));
menu_layer_set_click_config_onto_window(s_menu, window);
layer_add_child(root, menu_layer_get_layer(s_menu));
// after data changes: menu_layer_reload_data(s_menu);
```

`SimpleMenuLayer` is quicker for static lists; `ActionMenu` gives the
hierarchical "action" popups seen in notifications.

## Time and timers

```c
static void prv_tick(struct tm *t, TimeUnits changed) { layer_mark_dirty(s_canvas); }
tick_timer_service_subscribe(SECOND_UNIT, prv_tick);   // MINUTE_UNIT for watchfaces
tick_timer_service_unsubscribe();

time_t now = time(NULL);                 // UTC seconds
struct tm *lt = localtime(&now);         // local time for display
char buf[16];
strftime(buf, sizeof(buf), clock_is_24h_style() ? "%H:%M" : "%I:%M %p", lt);

AppTimer *t = app_timer_register(1500 /*ms*/, prv_timer_cb, NULL);
app_timer_cancel(t);
```

Subscribe in `appear`, unsubscribe in `disappear` so a covered window stops
redrawing.

## AppMessage (watch side)

```c
static void prv_inbox(DictionaryIterator *iter, void *ctx) {
  Tuple *t = dict_find(iter, MESSAGE_KEY_CMD);
  if (!t) return;
  int32_t cmd = t->value->int32;          // JS numbers arrive as int32
  Tuple *s = dict_find(iter, MESSAGE_KEY_TEXT);
  if (s && s->type == TUPLE_CSTRING) strncpy(buf, s->value->cstring, sizeof(buf) - 1);
}
static void prv_inbox_dropped(AppMessageResult reason, void *ctx) { APP_LOG(APP_LOG_LEVEL_ERROR, "in dropped %d", reason); }
static void prv_outbox_failed(DictionaryIterator *i, AppMessageResult reason, void *ctx) { ... }

app_message_register_inbox_received(prv_inbox);
app_message_register_inbox_dropped(prv_inbox_dropped);
app_message_register_outbox_failed(prv_outbox_failed);
app_message_open(1024, 256);             // inbox, outbox bytes; open once in init

// sending
DictionaryIterator *iter;
if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
  dict_write_int32(iter, MESSAGE_KEY_CMD, 2);
  dict_write_cstring(iter, MESSAGE_KEY_TEXT, "hallo");
  dict_write_end(iter);
  app_message_outbox_send();             // no argument
}
```

`APP_MSG_BUSY` from `outbox_begin` means the previous send is still in
flight — show "please wait", don't retry in a loop. Large lists go as one
message per item with `INDEX`/`COUNT` fields; the JS side sends them one after
another. Sizes: `app_message_inbox_size_maximum()` if you need more than 1 KB.

## Persistent storage

```c
persist_write_int(KEY, 42);           int v = persist_read_int(KEY);
persist_write_string(KEY, str);       persist_read_string(KEY, buf, sizeof(buf));
persist_write_data(KEY, &s, sizeof(s)); persist_read_data(KEY, &s, sizeof(s));
persist_exists(KEY);  persist_delete(KEY);
_Static_assert(sizeof(MyState) <= PERSIST_DATA_MAX_LENGTH, "state too big");
```

## App glance (launcher subtitle; not on aplite)

```c
static void prv_glance(AppGlanceReloadSession *s, size_t limit, void *ctx) {
  if (limit < 1) return;
  AppGlanceSlice slice = {
    .layout = { .icon = APP_GLANCE_SLICE_DEFAULT_ICON, .subtitle_template_string = ctx },
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
  };
  app_glance_add_slice(s, slice);
}
app_glance_reload(prv_glance, "Läuft: Probe");   // typically in deinit
```

Template strings support `{time_until(...)}`; strip `{}` from user text.

## Wakeup: alarms that survive leaving the app

Pebble suspends the app when it is not in the foreground, so a running
countdown dies on BACK. Persist the end time and let the firmware relaunch you:

```c
#define KEY_END_TIME 1
#define WAKEUP_REASON_DONE 1

time_t end = time(NULL) + minutes * 60;
persist_write_int(KEY_END_TIME, end);
WakeupId id = wakeup_schedule(end, WAKEUP_REASON_DONE, true /*notify if missed*/);
if (id < 0) { /* E_RANGE: another app owns that minute; try end + 60 */ }
persist_write_int(KEY_WAKEUP_ID, id);

// in init:
if (launch_reason() == APP_LAUNCH_WAKEUP) {
  WakeupId id; int32_t reason;
  wakeup_get_launch_event(&id, &reason);      // go straight to the "done" screen
}
// when the user cancels: wakeup_cancel(id); persist_delete(KEY_END_TIME);
// when relaunched mid-countdown: remaining = persisted end - time(NULL);
//   if wakeup_query(id, NULL) is false the wakeup is gone -> reschedule.
```

Vibration on finish: `vibes_enqueue_custom_pattern((VibePattern){ .durations = d, .num_segments = n })`,
optionally repeated with an `app_timer`. Stop it with `vibes_cancel()`.

## Touch (emery / gabbro, SDK 4.33+)

`PBL_TOUCH` is defined on touch platforms. Two layers:

- **System touch navigation**: `app_touch_navigation_enable(true)` (once, in
  init) lets a `MenuLayer`/`ScrollLayer` scroll and select by touch, mapped to
  button presses by the firmware. Third-party apps are opted out by default.
- **Own gestures** on a window: disable the bridge for that window and attach
  recognizers; the window owns and destroys them.

```c
#ifdef PBL_TOUCH
static void prv_tap(const Recognizer *r, RecognizerEvent e) {
  if (e != RecognizerEvent_Completed) return;
  GPoint p = tap_recognizer_get_tap_point(r);      // screen coordinates
  ...
}
// in window load:
window_set_touch_bridge_disabled(window, true);
window_attach_recognizer(window, tap_recognizer_create(prv_tap, NULL));
// also: pan_recognizer_create(cb, ctx, PanAxis_Vertical),
//       swipe_recognizer_create(cb, ctx, SwipeDirection_Left | SwipeDirection_Right)
#endif
```

Raw events: `touch_service_subscribe(handler, ctx)` gives Touchdown /
PositionUpdate / Liftoff with x/y. `touch_service_is_enabled()` tells whether
touch is delivered at all. Keep every touch action reachable by a button too.

## Languages

`i18n_get_system_locale()` returns the watch language ("de_DE", "en_US", …).
Keep a `static const char *const strings[S_COUNT][L_COUNT]` table indexed by
a string id and a language picked once in init, and reach every visible text
through `STR(id)`. Format strings ("since %s") belong in the table too, so
word order can differ per language. Gothic fonts render Latin-1 accents. On
the phone side, `Pebble.getActiveWatchInfo().language` gives the same code,
so error messages sent to the watch and the settings page can follow it.

## Other services

- Vibration: `vibes_short_pulse()`, `vibes_double_pulse()`, `vibes_long_pulse()`.
- Light: `light_enable_interaction()`.
- Battery / Bluetooth: `battery_state_service_subscribe`, `connection_service_subscribe`.
- Accelerometer: `accel_tap_service_subscribe` (shake) or `accel_data_service_subscribe`.
- Health: `health_service_sum_today(HealthMetricStepCount)` under `#if defined(PBL_HEALTH)`.
- Dictation (mic platforms): `dictation_session_create(buf_size, cb, ctx)`,
  `dictation_session_start(session)`; the callback gets a transcription string.
- Logging: `APP_LOG(APP_LOG_LEVEL_DEBUG, "x=%d", x)` → `pebble logs`.

## Watchfaces

Set `"watchface": true`; there are no button handlers except BACK. Subscribe to
`MINUTE_UNIT` (or `SECOND_UNIT` only when seconds are displayed — battery),
draw the time in `tick_handler`, and keep `unobstructed_area_service` in mind
for the quick-view timeline overlay. Watchfaces may still have PebbleKit JS
for weather and a settings page.

## Memory hygiene

Everything created in `load` is destroyed in `unload`; every `_create` has a
`_destroy`. `pebble logs` prints heap usage at exit — a non-zero "still
allocated" line is a leak. Keep big arrays `static` rather than on the stack
(stack is small) and avoid `malloc` churn.
