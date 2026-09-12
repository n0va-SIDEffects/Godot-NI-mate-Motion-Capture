/*
 * Sampler -- a soundboard for the Pebble Time 2 speaker.
 *
 *   Up / Down          browse sounds (or swipe on the touchscreen)
 *   Select / tap       play the selected sound (first row = random)
 *   Select (long)      options: stop, random, volume, shake toggle
 *   Shake the watch    play a random sound
 *   Back               exit
 *
 * Settings can also be changed on the phone (Clay config page), and up to
 * four user samples are loaded from the phone into RAM on each start.
 */
#include <pebble.h>
#include "sounds.h"
#include "player.h"
#include "phone.h"

#define PERSIST_KEY_VOLUME  1
#define PERSIST_KEY_SHAKE   2
#define PERSIST_KEY_TOUCH   3
#define PERSIST_KEY_ENABLED 4           // uint64_t bitmask of shown sounds

#define DEFAULT_VOLUME      80
#define ROW_RANDOM          0           // first row is the random entry
#define CELL_HEIGHT         PBL_IF_RECT_ELSE(48, 52)
#define STATUS_HEIGHT       22
#define SHAKE_COOLDOWN_MS   900
#define PLAYING_PHONE_BASE  1000        // s_playing >= this: phone slot (s_playing - base)

enum {
  ActionStop = 1,
  ActionRandom,
  ActionToggleShake,
  ActionVolumeBase = 100,  // ActionVolumeBase + volume
};

static Window *s_window;
static Layer *s_status_layer;
static MenuLayer *s_menu;
static ActionMenuLevel *s_action_root;
static ActionMenuLevel *s_action_volume;

static uint64_t s_enabled_mask = ~(uint64_t)0;  // bit i: SOUNDS[i] is shown
static uint8_t s_visible[64];                   // visible row order -> sound index
static int s_num_visible;
static int s_volume = DEFAULT_VOLUME;
static bool s_shake_enabled = true;
static bool s_touch_enabled = true;
static int s_playing = -1;          // -1 idle, sound index, or PLAYING_PHONE_BASE + slot
static int s_last_random = -1;
static uint64_t s_last_shake_ms;
static char s_header[48];
static char s_hint_buf[32];

/* ---- row model ---------------------------------------------------------- */

// Rebuilds the list of shown sounds from the enabled mask. An empty selection
// falls back to showing everything, so the list can never end up blank.
static void prv_rebuild_visible(void) {
  s_num_visible = 0;
  for (int i = 0; i < NUM_SOUNDS && i < 64; i++) {
    if (s_enabled_mask & ((uint64_t)1 << i)) s_visible[s_num_visible++] = (uint8_t)i;
  }
  if (s_num_visible == 0) {
    for (int i = 0; i < NUM_SOUNDS && i < 64; i++) s_visible[s_num_visible++] = (uint8_t)i;
  }
}

static int prv_visible_pos(int sound_index) {
  for (int i = 0; i < s_num_visible; i++) {
    if (s_visible[i] == sound_index) return i;
  }
  return -1;
}

typedef enum { RowRandom, RowPhone, RowSound } RowKind;

typedef struct {
  RowKind kind;
  int index;   // phone slot or sound index
} RowRef;

static RowRef prv_row(uint16_t row) {
  if (row == ROW_RANDOM) return (RowRef) { RowRandom, 0 };
  int phone = phone_active_slots();
  if ((int)row <= phone) return (RowRef) { RowPhone, row - 1 };
  return (RowRef) { RowSound, s_visible[row - 1 - phone] };
}

static uint16_t prv_row_for_sound(int index) {
  int pos = prv_visible_pos(index);
  return (uint16_t)(1 + phone_active_slots() + (pos < 0 ? 0 : pos));
}

static const char *prv_playing_name(void) {
  if (s_playing >= PLAYING_PHONE_BASE) {
    const PhoneSlot *slot = phone_slot(s_playing - PLAYING_PHONE_BASE);
    return slot ? slot->name : "?";
  }
  return SOUNDS[s_playing].name;
}

static void prv_update_header(void) {
  if (speaker_is_muted()) {
    snprintf(s_header, sizeof(s_header), "Lautsprecher stumm!");
  } else if (s_playing >= 0) {
    snprintf(s_header, sizeof(s_header), "Spielt: %s", prv_playing_name());
  } else {
    snprintf(s_header, sizeof(s_header), "SAMPLER   Vol %d%%", s_volume);
  }
  if (s_status_layer) layer_mark_dirty(s_status_layer);
  if (s_menu) layer_mark_dirty(menu_layer_get_layer(s_menu));
}

static uint8_t prv_row_argb(uint16_t row) {
  RowRef r = prv_row(row);
  switch (r.kind) {
    case RowRandom: return GColorDarkGrayARGB8;
    case RowPhone:  return GColorVividCeruleanARGB8;
    default:        return SOUNDS[r.index].argb;
  }
}

static void prv_apply_highlight(uint16_t row) {
  GColor bg = (GColor){ .argb = prv_row_argb(row) };
  menu_layer_set_highlight_colors(s_menu, PBL_IF_COLOR_ELSE(bg, GColorBlack),
                                  PBL_IF_COLOR_ELSE(gcolor_legible_over(bg), GColorWhite));
}

/* ---- playback ----------------------------------------------------------- */

static void prv_after_play(bool ok, int playing_id) {
  if (ok) {
    s_playing = playing_id;
  } else {
    s_playing = -1;
    vibes_double_pulse();
  }
  prv_update_header();
}

static void prv_play_index(int index) {
  if (index < 0 || index >= NUM_SOUNDS) return;
  if (speaker_is_muted()) vibes_short_pulse();  // give at least some feedback
  prv_after_play(player_play(&SOUNDS[index], (uint8_t)s_volume), index);
}

static void prv_play_phone(int slot) {
  const PhoneSlot *s = phone_slot(slot);
  if (!s || s->state != PhoneSlotReady) {
    vibes_short_pulse();
    return;
  }
  if (speaker_is_muted()) vibes_short_pulse();
  prv_after_play(player_play_memory(s->data, s->total, (uint8_t)s_volume), PLAYING_PHONE_BASE + slot);
}

static void prv_play_random(void) {
  int ready_phone = 0;
  for (int i = 0; i < PHONE_MAX_SLOTS; i++) {
    if (phone_slot(i)->state == PhoneSlotReady) ready_phone++;
  }
  int pool = s_num_visible + ready_phone;
  int pick = rand() % pool;
  if (pick == s_last_random && pool > 1) pick = (pick + 1) % pool;
  s_last_random = pick;
  if (pick < s_num_visible) {
    int index = s_visible[pick];
    menu_layer_set_selected_index(s_menu, MenuIndex(0, prv_row_for_sound(index)), MenuRowAlignCenter, true);
    prv_play_index(index);
    return;
  }
  int nth = pick - s_num_visible;
  for (int i = 0; i < PHONE_MAX_SLOTS; i++) {
    if (phone_slot(i)->state != PhoneSlotReady) continue;
    if (nth-- == 0) {
      menu_layer_set_selected_index(s_menu, MenuIndex(0, 1 + i), MenuRowAlignCenter, true);
      prv_play_phone(i);
      return;
    }
  }
}

static void prv_stop(void) {
  player_stop();
  s_playing = -1;
  prv_update_header();
}

static void prv_on_finished(SpeakerFinishReason reason) {
  s_playing = -1;
  prv_update_header();
}

/* ---- phone callbacks ---------------------------------------------------- */

static void prv_on_phone_slot_changed(int slot) {
  const PhoneSlot *s = phone_slot(slot);
  // A slot that is being replaced while it plays must not keep playing freed memory.
  if (s_playing == PLAYING_PHONE_BASE + slot && s->state != PhoneSlotReady) {
    prv_stop();
  }
  if (s_menu) {
    menu_layer_reload_data(s_menu);
    layer_mark_dirty(menu_layer_get_layer(s_menu));
  }
}

static void prv_apply_touch(void) {
  app_touch_navigation_enable(s_touch_enabled);
}

static void prv_on_phone_settings(const PhoneSettings *st) {
  if (st->volume >= 10 && st->volume <= 100) {
    s_volume = st->volume;
    persist_write_int(PERSIST_KEY_VOLUME, s_volume);
    speaker_set_volume((uint8_t)s_volume);
  }
  if (st->shake >= 0) {
    s_shake_enabled = st->shake != 0;
    persist_write_bool(PERSIST_KEY_SHAKE, s_shake_enabled);
  }
  if (st->touch >= 0) {
    s_touch_enabled = st->touch != 0;
    persist_write_bool(PERSIST_KEY_TOUCH, s_touch_enabled);
    prv_apply_touch();
  }
  if (st->has_enabled) {
    s_enabled_mask = st->enabled_mask;
    persist_write_data(PERSIST_KEY_ENABLED, &s_enabled_mask, sizeof(s_enabled_mask));
    prv_rebuild_visible();
    if (s_menu) {
      menu_layer_reload_data(s_menu);
      menu_layer_set_selected_index(s_menu, MenuIndex(0, ROW_RANDOM), MenuRowAlignTop, false);
      prv_apply_highlight(ROW_RANDOM);
    }
  }
  prv_update_header();
}

/* ---- menu callbacks --------------------------------------------------- */

static uint16_t prv_get_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  return 1 + phone_active_slots() + s_num_visible;
}

static int16_t prv_get_cell_height(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  return CELL_HEIGHT;
}

static void prv_status_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_header, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(b.origin.x + 6, b.origin.y - 3, b.size.w - 12, b.size.h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void prv_draw_play_icon(GContext *ctx, GPoint center, GColor color) {
  GPathInfo info = {
    .num_points = 3,
    .points = (GPoint[]) { {-4, -6}, {6, 0}, {-4, 6} },
  };
  GPath *path = gpath_create(&info);
  gpath_move_to(path, center);
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

// Fills name/hint/disc glyph for a row.
static void prv_row_text(uint16_t row, const char **name, const char **hint, const char **glyph, bool *playing) {
  RowRef r = prv_row(row);
  *glyph = NULL;
  *playing = false;
  switch (r.kind) {
    case RowRandom:
      *name = "Zufall";
      *hint = "Ueberraschung!";
      *glyph = "?";
      break;
    case RowPhone: {
      const PhoneSlot *s = phone_slot(r.index);
      *name = s->name;
      switch (s->state) {
        case PhoneSlotLoading: {
          int pct = s->total ? (int)((s->received * 100) / s->total) : 0;
          snprintf(s_hint_buf, sizeof(s_hint_buf), "Laedt vom Handy %d%%", pct);
          *hint = s_hint_buf;
          *glyph = "...";
          break;
        }
        case PhoneSlotReady:
          snprintf(s_hint_buf, sizeof(s_hint_buf), "Handy-Sample %lu,%lu s",
                   (unsigned long)(s->total / 8000), (unsigned long)((s->total % 8000) / 800));
          *hint = s_hint_buf;
          break;
        default:
          *hint = "Laden fehlgeschlagen";
          *glyph = "!";
          break;
      }
      *playing = (s_playing == PLAYING_PHONE_BASE + r.index);
      break;
    }
    default:
      *name = SOUNDS[r.index].name;
      *hint = SOUNDS[r.index].hint;
      *playing = (s_playing == r.index);
      break;
  }
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *idx, void *data) {
  GRect b = layer_get_bounds(cell_layer);
  bool selected = menu_cell_layer_is_highlighted(cell_layer);
  const char *name, *hint, *glyph;
  bool playing;
  prv_row_text(idx->row, &name, &hint, &glyph, &playing);

  GColor accent = (GColor){ .argb = prv_row_argb(idx->row) };
  GColor fg = selected ? PBL_IF_COLOR_ELSE(gcolor_legible_over(accent), GColorWhite) : GColorBlack;
  GColor disc = selected ? fg : PBL_IF_COLOR_ELSE(accent, GColorBlack);
  GColor on_disc = selected ? PBL_IF_COLOR_ELSE(accent, GColorBlack) : GColorWhite;

  // Accent disc with a play triangle while this sound is running.
  GPoint c = GPoint(b.origin.x + 22, b.origin.y + b.size.h / 2);
  graphics_context_set_fill_color(ctx, disc);
  graphics_fill_circle(ctx, c, 12);
  if (playing) {
    prv_draw_play_icon(ctx, GPoint(c.x + 1, c.y), on_disc);
  } else if (glyph) {
    graphics_context_set_text_color(ctx, on_disc);
    graphics_draw_text(ctx, glyph, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(c.x - 14, c.y - 13, 28, 24), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }

  graphics_context_set_text_color(ctx, fg);
  int16_t x = b.origin.x + 44;
  int16_t w = b.size.w - 48;
  graphics_draw_text(ctx, name, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(x, b.origin.y - 1, w, 28), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, hint, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(x, b.origin.y + 22, w, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

static void prv_selection_changed(MenuLayer *menu, MenuIndex new_idx, MenuIndex old_idx, void *ctx) {
  prv_apply_highlight(new_idx.row);
}

static void prv_select_click(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  RowRef r = prv_row(idx->row);
  switch (r.kind) {
    case RowRandom: prv_play_random(); break;
    case RowPhone:  prv_play_phone(r.index); break;
    default:        prv_play_index(r.index); break;
  }
}

/* ---- action menu (long press) ----------------------------------------- */

static void prv_action_performed(ActionMenu *menu, const ActionMenuItem *item, void *ctx) {
  int action = (int)(intptr_t)action_menu_item_get_action_data(item);
  switch (action) {
    case ActionStop:
      prv_stop();
      break;
    case ActionRandom:
      prv_play_random();
      break;
    case ActionToggleShake:
      s_shake_enabled = !s_shake_enabled;
      persist_write_bool(PERSIST_KEY_SHAKE, s_shake_enabled);
      break;
    default:
      if (action >= ActionVolumeBase) {
        s_volume = action - ActionVolumeBase;
        persist_write_int(PERSIST_KEY_VOLUME, s_volume);
        speaker_set_volume((uint8_t)s_volume);
        prv_update_header();
      }
      break;
  }
}

static void prv_action_menu_closed(ActionMenu *menu, const ActionMenuItem *item, void *ctx) {
  action_menu_hierarchy_destroy(s_action_root, NULL, NULL);
  s_action_root = NULL;
  s_action_volume = NULL;
}

static void prv_select_long_click(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  s_action_root = action_menu_level_create(4);
  action_menu_level_add_action(s_action_root, "Stopp", prv_action_performed, (void *)ActionStop);
  action_menu_level_add_action(s_action_root, "Zufalls-Sound", prv_action_performed, (void *)ActionRandom);

  s_action_volume = action_menu_level_create(5);
  static const char *VOLUME_LABELS[] = { "20 %", "40 %", "60 %", "80 %", "100 %" };
  for (int i = 0; i < 5; i++) {
    int vol = (i + 1) * 20;
    action_menu_level_add_action(s_action_volume, VOLUME_LABELS[i], prv_action_performed,
                                 (void *)(intptr_t)(ActionVolumeBase + vol));
  }
  action_menu_level_add_child(s_action_root, s_action_volume, "Lautstaerke");
  action_menu_level_add_action(s_action_root,
                               s_shake_enabled ? "Schuetteln: An" : "Schuetteln: Aus",
                               prv_action_performed, (void *)ActionToggleShake);

  ActionMenuConfig config = {
    .root_level = s_action_root,
    .colors = {
      .background = PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite),
      .foreground = GColorBlack,
    },
    .align = ActionMenuAlignCenter,
    .did_close = prv_action_menu_closed,
  };
  action_menu_open(&config);
}

/* ---- shake to shuffle -------------------------------------------------- */

static uint64_t prv_now_ms(void) {
  time_t sec;
  uint16_t ms;
  time_ms(&sec, &ms);
  return (uint64_t)sec * 1000 + ms;
}

static void prv_tap_handler(AccelAxisType axis, int32_t direction) {
  if (!s_shake_enabled) return;
  uint64_t now = prv_now_ms();
  if (now - s_last_shake_ms < SHAKE_COOLDOWN_MS) return;
  s_last_shake_ms = now;
  prv_play_random();
}

/* ---- window ------------------------------------------------------------ */

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_status_layer = layer_create(GRect(0, 0, bounds.size.w, STATUS_HEIGHT));
  layer_set_update_proc(s_status_layer, prv_status_update_proc);
  layer_add_child(root, s_status_layer);

  s_menu = menu_layer_create(GRect(0, STATUS_HEIGHT, bounds.size.w, bounds.size.h - STATUS_HEIGHT));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_get_num_rows,
    .get_cell_height = prv_get_cell_height,
    .draw_row = prv_draw_row,
    .select_click = prv_select_click,
    .select_long_click = prv_select_long_click,
    .selection_changed = prv_selection_changed,
  });
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack);
  menu_layer_set_click_config_onto_window(s_menu, window);
  prv_apply_highlight(ROW_RANDOM);
  layer_add_child(root, menu_layer_get_layer(s_menu));
  prv_update_header();
}

static void prv_window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  layer_destroy(s_status_layer);
  s_status_layer = NULL;
}

static void prv_init(void) {
  srand((unsigned)time(NULL));
  if (persist_exists(PERSIST_KEY_VOLUME)) {
    s_volume = persist_read_int(PERSIST_KEY_VOLUME);
    if (s_volume < 10 || s_volume > 100) s_volume = DEFAULT_VOLUME;
  }
  if (persist_exists(PERSIST_KEY_SHAKE)) s_shake_enabled = persist_read_bool(PERSIST_KEY_SHAKE);
  if (persist_exists(PERSIST_KEY_TOUCH)) s_touch_enabled = persist_read_bool(PERSIST_KEY_TOUCH);
  if (persist_exists(PERSIST_KEY_ENABLED)) {
    persist_read_data(PERSIST_KEY_ENABLED, &s_enabled_mask, sizeof(s_enabled_mask));
  }
  prv_rebuild_visible();

  player_init(prv_on_finished);
  phone_init(prv_on_phone_slot_changed, prv_on_phone_settings);
  accel_tap_service_subscribe(prv_tap_handler);
  prv_apply_touch();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}

static void prv_deinit(void) {
  accel_tap_service_unsubscribe();
  player_deinit();
  phone_deinit();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
