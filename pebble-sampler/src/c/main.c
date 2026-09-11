/*
 * Sampler -- a soundboard for the Pebble Time 2 speaker.
 *
 *   Up / Down          browse sounds
 *   Select             play the selected sound (first row = random)
 *   Select (long)      options: stop, random, volume, shake toggle
 *   Shake the watch    play a random sound
 *   Back               exit
 */
#include <pebble.h>
#include "sounds.h"
#include "player.h"

#define PERSIST_KEY_VOLUME  1
#define PERSIST_KEY_SHAKE   2

#define DEFAULT_VOLUME      80
#define ROW_RANDOM          0           // first row is the random entry
#define CELL_HEIGHT         PBL_IF_RECT_ELSE(48, 52)
#define STATUS_HEIGHT       22
#define SHAKE_COOLDOWN_MS   900

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

static int s_volume = DEFAULT_VOLUME;
static bool s_shake_enabled = true;
static int s_playing = -1;          // index into SOUNDS, -1 when idle
static int s_last_random = -1;
static uint64_t s_last_shake_ms;
static char s_header[48];

/* ---------------------------------------------------------------------- */

static const Sound RANDOM_ENTRY = {
  .name = "Zufall",
  .hint = "Ueberraschung!",
  .argb = GColorDarkGrayARGB8,
  .kind = SoundKindNotes,
};

static const Sound *prv_sound_for_row(uint16_t row) {
  return row == ROW_RANDOM ? &RANDOM_ENTRY : &SOUNDS[row - 1];
}

static void prv_update_header(void) {
  if (speaker_is_muted()) {
    snprintf(s_header, sizeof(s_header), "Lautsprecher stumm!");
  } else if (s_playing >= 0) {
    snprintf(s_header, sizeof(s_header), "Spielt: %s", SOUNDS[s_playing].name);
  } else {
    snprintf(s_header, sizeof(s_header), "SAMPLER   Vol %d%%", s_volume);
  }
  if (s_status_layer) layer_mark_dirty(s_status_layer);
  if (s_menu) layer_mark_dirty(menu_layer_get_layer(s_menu));
}

static void prv_apply_highlight(uint16_t row) {
  GColor bg = (GColor){ .argb = prv_sound_for_row(row)->argb };
  menu_layer_set_highlight_colors(s_menu, PBL_IF_COLOR_ELSE(bg, GColorBlack),
                                  PBL_IF_COLOR_ELSE(gcolor_legible_over(bg), GColorWhite));
}

static void prv_play_index(int index) {
  if (index < 0 || index >= NUM_SOUNDS) return;
  if (speaker_is_muted()) {
    vibes_short_pulse();  // give at least some feedback
  }
  if (player_play(&SOUNDS[index], (uint8_t)s_volume)) {
    s_playing = index;
  } else {
    s_playing = -1;
    vibes_double_pulse();
  }
  prv_update_header();
}

static void prv_play_random(void) {
  int pick = rand() % NUM_SOUNDS;
  if (pick == s_last_random && NUM_SOUNDS > 1) pick = (pick + 1) % NUM_SOUNDS;
  s_last_random = pick;
  // Jump the selection to the picked sound so the user sees what is playing.
  menu_layer_set_selected_index(s_menu, MenuIndex(0, pick + 1), MenuRowAlignCenter, true);
  prv_play_index(pick);
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

/* ---- menu callbacks --------------------------------------------------- */

static uint16_t prv_get_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  return NUM_SOUNDS + 1;
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

static void prv_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *idx, void *data) {
  GRect b = layer_get_bounds(cell_layer);
  const Sound *snd = prv_sound_for_row(idx->row);
  bool selected = menu_cell_layer_is_highlighted(cell_layer);
  bool playing = (idx->row != ROW_RANDOM) && (s_playing == (int)idx->row - 1);

  GColor accent = (GColor){ .argb = snd->argb };
  GColor fg = selected ? PBL_IF_COLOR_ELSE(gcolor_legible_over(accent), GColorWhite) : GColorBlack;
  GColor disc = selected ? fg : PBL_IF_COLOR_ELSE(accent, GColorBlack);

  // Accent disc with a play triangle while this sound is running.
  GPoint c = GPoint(b.origin.x + 22, b.origin.y + b.size.h / 2);
  graphics_context_set_fill_color(ctx, disc);
  graphics_fill_circle(ctx, c, 12);
  if (playing) {
    prv_draw_play_icon(ctx, GPoint(c.x + 1, c.y), selected ? PBL_IF_COLOR_ELSE(accent, GColorBlack) : GColorWhite);
  } else if (idx->row == ROW_RANDOM) {
    graphics_context_set_text_color(ctx, selected ? PBL_IF_COLOR_ELSE(accent, GColorBlack) : GColorWhite);
    graphics_draw_text(ctx, "?", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(c.x - 12, c.y - 13, 24, 24), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }

  graphics_context_set_text_color(ctx, fg);
  int16_t x = b.origin.x + 44;
  int16_t w = b.size.w - 48;
  graphics_draw_text(ctx, snd->name, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(x, b.origin.y - 1, w, 28), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, snd->hint, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(x, b.origin.y + 22, w, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

static void prv_selection_changed(MenuLayer *menu, MenuIndex new_idx, MenuIndex old_idx, void *ctx) {
  prv_apply_highlight(new_idx.row);
}

static void prv_select_click(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  if (idx->row == ROW_RANDOM) {
    prv_play_random();
  } else {
    prv_play_index(idx->row - 1);
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
    if (s_volume < 20 || s_volume > 100) s_volume = DEFAULT_VOLUME;
  }
  if (persist_exists(PERSIST_KEY_SHAKE)) {
    s_shake_enabled = persist_read_bool(PERSIST_KEY_SHAKE);
  }

  player_init(prv_on_finished);
  accel_tap_service_subscribe(prv_tap_handler);

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
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
