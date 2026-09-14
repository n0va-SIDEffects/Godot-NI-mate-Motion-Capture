#include <pebble.h>
#include "config.h"
#include "e1clock.h"
#include "synth.h"
#include "audio.h"
#include "haptics.h"
#include "backlight.h"
#include "input.h"
#include "render.h"
#include "game.h"

// Glasgarten, Etappe 1. Fuenf Bildschirme, Wechsel mit langem Druck auf Select:
//   STIMMEN  Finger aufs Glas, ziehen = Tonhoehe, Up/Down = Feinstimmung, Select = neue Blume
//   LATENZ   Gabel singt dauerhaft; Select = Oktavsprung mit LRA-Marker, Doppelklick = Puffer-Probe
//   LRA      Select = naechste Rate (2, 4, 6, 8 Hz), jeder Impuls wird geloggt
//   LICHT    Select = naechster Schritt (Atmen 1..4 Hz, Dimmrampe, aus)
//   PANEL    Select = Test starten bzw. Variante wechseln (Vollbild / 10 Zeilen)
// Ausserhalb von STIMMEN steht die Spiellogik still: nur der Synth folgt Finger und Blume.

typedef enum {
  ModeStimmen = 0,
  ModeLatenz,
  ModeLra,
  ModeLicht,
  ModePanel,
  ModeCount,
} Mode;

static const char *s_mode_names[ModeCount] = { "STIMMEN", "LATENZ", "LRA", "LICHT", "PANEL" };
static const uint8_t s_lra_rates[4] = { 2, 4, 6, 8 };

static Window *s_window;
static Layer *s_layer;
static Mode s_mode = ModeStimmen;
static AppTimer *s_game_timer;
static AppTimer *s_render_timer;
static AppTimer *s_light_timer;
static AppTimer *s_log_timer;
static AppTimer *s_jump_timer;
static uint32_t s_last_tick_ms;
static uint8_t s_lra_idx;
static uint8_t s_licht_step;
static uint8_t s_panel_variant;
static uint32_t s_jump_count;
static bool s_paused;

static void prv_update_hud(void);

static void prv_mark_dirty(void) {
  if (s_layer) {
    layer_mark_dirty(s_layer);
  }
}

static void prv_jump_off(void *data) {
  s_jump_timer = NULL;
  synth_set_octave_jump(false);
}

static void prv_leave_mode(Mode m) {
  switch (m) {
    case ModeStimmen:
      haptics_stop();
      backlight_enable(false);
      break;
    case ModeLatenz:
      synth_set_force_gate(false);
      synth_set_octave_jump(false);
      if (s_jump_timer) {
        app_timer_cancel(s_jump_timer);
        s_jump_timer = NULL;
      }
      break;
    case ModeLra:
      haptics_stop();
      break;
    case ModeLicht:
      backlight_test_set_step(5);
      break;
    case ModePanel:
      render_panel_test_stop();
      break;
    default:
      break;
  }
}

static void prv_start_mode(Mode m) {
  switch (m) {
    case ModeStimmen:
      backlight_enable(true);
      break;
    case ModeLatenz:
      synth_set_force_gate(true);
      break;
    case ModeLra:
      haptics_test_rate(s_lra_rates[s_lra_idx]);
      break;
    case ModeLicht:
      backlight_test_set_step(s_licht_step);
      break;
    case ModePanel:
      break;
    default:
      break;
  }
}

static void prv_enter_mode(Mode m) {
  prv_leave_mode(s_mode);
  s_mode = m;
  game_set_active(m == ModeStimmen);
  if (m == ModeLra) s_lra_idx = 0;
  if (m == ModeLicht) s_licht_step = 0;
  if (m == ModePanel) s_panel_variant = 0;
  APP_LOG(APP_LOG_LEVEL_INFO, "[E1] Modus %s", s_mode_names[m]);
  prv_start_mode(m);
  prv_update_hud();
  prv_mark_dirty();
}

static void prv_select_click(ClickRecognizerRef rec, void *ctx) {
  input_button_pressed();
  switch (s_mode) {
    case ModeStimmen:
      game_respawn_flower();
      break;
    case ModeLatenz: {
      const AudioStats *a = audio_stats();
      s_jump_count++;
      bool marker = haptics_marker();
      synth_set_octave_jump(true);
      APP_LOG(APP_LOG_LEVEL_INFO,
              "[E1][LATENZ] Sprung %lu t=%lums Queue %lums +%dms Pipeline, Marker %s",
              (unsigned long)s_jump_count, (unsigned long)e1clock_now_ms(),
              (unsigned long)a->queue_ms_est, AUDIO_PIPELINE_ASSUMED_MS, marker ? "ja" : "NEIN");
      if (s_jump_timer) {
        app_timer_cancel(s_jump_timer);
      }
      s_jump_timer = app_timer_register(500, prv_jump_off, NULL);
      break;
    }
    case ModeLra:
      s_lra_idx = (uint8_t)((s_lra_idx + 1) % 4);
      haptics_test_rate(s_lra_rates[s_lra_idx]);
      break;
    case ModeLicht:
      s_licht_step = (uint8_t)((s_licht_step + 1) % 6);
      backlight_test_set_step(s_licht_step);
      break;
    case ModePanel: {
      const PanelStats *p = render_panel_stats();
      if (p->active) {
        break;
      }
      if (p->done) {
        s_panel_variant = (uint8_t)(1 - s_panel_variant);
      }
      render_panel_test_start(s_panel_variant);
      break;
    }
    default:
      break;
  }
  prv_update_hud();
}

static void prv_select_double(ClickRecognizerRef rec, void *ctx) {
  input_button_pressed();
  if (s_mode == ModeLatenz) {
    audio_probe_capacity();
    prv_update_hud();
  }
}

static void prv_select_long(ClickRecognizerRef rec, void *ctx) {
  input_button_pressed();
  prv_enter_mode((Mode)((s_mode + 1) % ModeCount));
}

static void prv_select_raw(ClickRecognizerRef rec, void *ctx) {
  // Druck und Loslassen stoeren den liegenden Finger: Pan-Maske sofort, nicht erst im Click-Handler
  input_button_pressed();
}

static void prv_up_click(ClickRecognizerRef rec, void *ctx) {
  input_button_pressed();
  input_nudge_chz(FINE_STEP_CHZ);
}

static void prv_down_click(ClickRecognizerRef rec, void *ctx) {
  input_button_pressed();
  input_nudge_chz(-FINE_STEP_CHZ);
}

static void prv_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_multi_click_subscribe(BUTTON_ID_SELECT, 2, 2, 300, true, prv_select_double);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, prv_select_long, NULL);
  window_raw_click_subscribe(BUTTON_ID_SELECT, prv_select_raw, prv_select_raw, NULL);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 120, prv_up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 120, prv_down_click);
}

static void prv_fmt_chz(char *buf, size_t len, int32_t chz) {
  snprintf(buf, len, "%ld.%02ld", (long)(chz / 100), (long)(chz % 100));
}

static void prv_update_hud(void) {
  char l1[64], l2[64], l3[64];
  char f1[16], f2[16], name[8];
  const GameView *g = game_view();
  const AudioStats *a = audio_stats();
  const RenderStats *r = render_stats();
  const TouchStats *t = input_stats();
  switch (s_mode) {
    case ModeStimmen:
      prv_fmt_chz(f1, sizeof(f1), g->fork_chz);
      prv_fmt_chz(f2, sizeof(f2), g->beat_chz);
      game_note_name(g->fork_chz, name, sizeof(name));
      snprintf(l1, sizeof(l1), "%s %sHz %s", g->finger ? "*" : " ", f1, name);
      snprintf(l2, sizeof(l2), "%s beat %sHz q%lums %lu.%lufps", game_state_name(), f2,
               (unsigned long)a->queue_ms_est, (unsigned long)(r->fps_x10 / 10),
               (unsigned long)(r->fps_x10 % 10));
      snprintf(l3, sizeof(l3), "touch %lu/s %lums still%upx dz%lu %s%s",
               (unsigned long)(t->ev_rate_x10 / 10), (unsigned long)t->min_interval_ms,
               (unsigned)t->still_max_px, (unsigned long)t->dz_exceed,
               input_clutch_engaged() ? "K " : "", a->muted ? "STUMM" : "");
      break;
    case ModeLatenz:
      snprintf(l1, sizeof(l1), "LATENZ  Sel=Sprung");
      snprintf(l2, sizeof(l2), "q%lums cap %luB=%lums ur%lu sw%lu", (unsigned long)a->queue_ms_est,
               (unsigned long)a->capacity_bytes, (unsigned long)a->capacity_ms,
               (unsigned long)a->underruns, (unsigned long)a->short_writes);
      snprintf(l3, sizeof(l3), "2x=Probe  pre %lu err %lu stau %lu", (unsigned long)a->preempted,
               (unsigned long)a->errors, (unsigned long)a->stalls);
      break;
    case ModeLra:
      snprintf(l1, sizeof(l1), "LRA  %u Hz  Sel=weiter", (unsigned)s_lra_rates[s_lra_idx]);
      snprintf(l2, sizeof(l2), "Impulse %lu  Modus %s", (unsigned long)haptics_call_count(),
               haptics_mode_name());
      snprintf(l3, sizeof(l3), "Zaehlbar? Log: pebble logs");
      break;
    case ModeLicht:
      snprintf(l1, sizeof(l1), "LICHT  Schritt %u  Sel=weiter", (unsigned)s_licht_step);
      snprintf(l2, sizeof(l2), "%s", s_licht_step <= 3 ? "Atmen 1/2/3/4 Hz" :
                                     (s_licht_step == 4 ? "Dimmrampe 8 Stufen" : "aus"));
      snprintf(l3, sizeof(l3), "rgb %06lx calls %lu", (unsigned long)backlight_last_rgb(),
               (unsigned long)backlight_call_count());
      break;
    case ModePanel: {
      const PanelStats *p = render_panel_stats();
      snprintf(l1, sizeof(l1), "PANEL  %s  Sel=%s", p->variant == 0 ? "Vollbild" : "10 Zeilen",
               p->active ? "laeuft" : "start");
      snprintf(l2, sizeof(l2), "voll %lu.%lu ms  band %lu.%lu ms", (unsigned long)(p->result_full_x10 / 10),
               (unsigned long)(p->result_full_x10 % 10), (unsigned long)(p->result_band_x10 / 10),
               (unsigned long)(p->result_band_x10 % 10));
      snprintf(l3, sizeof(l3), "%lu Frames  %lu.%lu fps", (unsigned long)p->frames,
               (unsigned long)(p->fps_x10 / 10), (unsigned long)(p->fps_x10 % 10));
      break;
    }
    default:
      l1[0] = l2[0] = l3[0] = '\0';
      break;
  }
  render_set_hud(l1, l2, l3);
}

static void prv_game_tick(void *data) {
  s_game_timer = app_timer_register(GAME_TICK_MS, prv_game_tick, NULL);
  uint32_t now = e1clock_now_ms();
  uint32_t dt = s_last_tick_ms ? now - s_last_tick_ms : GAME_TICK_MS;
  if (dt > 200) dt = 200;
  s_last_tick_ms = now;
  if (s_paused || !s_layer) {
    return;
  }
  input_tick(now);
  game_tick(now, dt);
}

static void prv_light_tick(void *data) {
  s_light_timer = app_timer_register(BL_UPDATE_MS, prv_light_tick, NULL);
  if (s_paused || !s_layer) {
    return;
  }
  uint32_t now = e1clock_now_ms();
  if (s_mode == ModeLicht) {
    backlight_test_tick(now);
  } else {
    backlight_tick(now);
  }
}

static void prv_render_tick(void *data) {
  s_render_timer = app_timer_register(RENDER_TICK_MS, prv_render_tick, NULL);
  if (s_paused || !s_layer) {
    return;
  }
  prv_update_hud();
  if (s_mode != ModePanel || !render_panel_stats()->active) {
    prv_mark_dirty();
  }
}

static void prv_log_tick(void *data) {
  s_log_timer = app_timer_register(LOG_TICK_MS, prv_log_tick, NULL);
  if (s_paused) {
    return;
  }
  const GameView *g = game_view();
  const AudioStats *a = audio_stats();
  const RenderStats *r = render_stats();
  const TouchStats *t = input_stats();
  // APP_LOG schneidet Nachrichten bei rund 87 Zeichen ab, deshalb drei Zeilen.
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[E1] %s fps=%lu.%lu rend=%lu.%lums q=%lums ur=%lu sw=%lu heap=%lu glitch=%lu",
          s_mode_names[s_mode], (unsigned long)(r->fps_x10 / 10), (unsigned long)(r->fps_x10 % 10),
          (unsigned long)(r->render_ms_x10 / 10), (unsigned long)(r->render_ms_x10 % 10),
          (unsigned long)a->queue_ms_est, (unsigned long)a->underruns, (unsigned long)a->short_writes,
          (unsigned long)heap_bytes_free(), (unsigned long)e1clock_glitches());
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[E1t] touch=%lu/s jit=%lu.%lupx still=%upx dz=%lu ivl=%lums lra=%lu bl=%lu stau=%lu",
          (unsigned long)(t->ev_rate_x10 / 10), (unsigned long)(t->jitter_x10_px / 10),
          (unsigned long)(t->jitter_x10_px % 10), (unsigned)t->still_max_px,
          (unsigned long)t->dz_exceed, (unsigned long)t->min_interval_ms,
          (unsigned long)haptics_call_count(), (unsigned long)backlight_call_count(),
          (unsigned long)a->stalls);
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[E1+] fork=%ld.%02ld fl=%ld.%02ld beat=%ld.%02ld st=%s rs=%lu",
          (long)(g->fork_chz / 100), (long)(g->fork_chz % 100), (long)(g->flower_chz / 100),
          (long)(g->flower_chz % 100), (long)(g->beat_chz / 100), (long)(g->beat_chz % 100),
          game_state_name(), (unsigned long)e1clock_resyncs());
}

static void prv_will_focus(bool in_focus) {
  if (!in_focus && !s_paused) {
    // Benachrichtigung legt sich ueber die App: alles ruhigstellen
    s_paused = true;
    haptics_stop();
    backlight_enable(false);
    audio_stop();
    synth_set_octave_jump(false);
    APP_LOG(APP_LOG_LEVEL_INFO, "[E1] Fokus verloren, pausiert");
  }
}

static void prv_did_focus(bool in_focus) {
  if (in_focus && s_paused) {
    s_paused = false;
    s_last_tick_ms = 0;
    audio_start();
    prv_start_mode(s_mode);
    backlight_refresh();
    APP_LOG(APP_LOG_LEVEL_INFO, "[E1] Fokus zurueck, weiter");
  }
  if (in_focus) {
    prv_mark_dirty();
  }
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_layer = layer_create(bounds);
  layer_add_child(root, s_layer);
  render_init(s_layer);
  input_init(window);
  prv_enter_mode(ModeStimmen);
}

static void prv_window_unload(Window *window) {
  // Timer-Kaskaden stoppen, bevor der Layer verschwindet
  if (s_game_timer) { app_timer_cancel(s_game_timer); s_game_timer = NULL; }
  if (s_render_timer) { app_timer_cancel(s_render_timer); s_render_timer = NULL; }
  if (s_light_timer) { app_timer_cancel(s_light_timer); s_light_timer = NULL; }
  if (s_log_timer) { app_timer_cancel(s_log_timer); s_log_timer = NULL; }
  if (s_jump_timer) { app_timer_cancel(s_jump_timer); s_jump_timer = NULL; }
  haptics_stop();
  backlight_enable(false);
  render_deinit();
  input_deinit();
  Layer *l = s_layer;
  s_layer = NULL;
  layer_destroy(l);
}

static void prv_init(void) {
  e1clock_init();
  synth_init();
  haptics_init();
  backlight_init();
  game_init();
  audio_init();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, prv_click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  audio_start();
  app_focus_service_subscribe_handlers((AppFocusHandlers) {
    .will_focus = prv_will_focus,
    .did_focus = prv_did_focus,
  });
  s_game_timer = app_timer_register(GAME_TICK_MS, prv_game_tick, NULL);
  s_render_timer = app_timer_register(RENDER_TICK_MS, prv_render_tick, NULL);
  s_light_timer = app_timer_register(BL_UPDATE_MS, prv_light_tick, NULL);
  s_log_timer = app_timer_register(LOG_TICK_MS, prv_log_tick, NULL);

  APP_LOG(APP_LOG_LEVEL_INFO, "[E1] Glasgarten E1 v%s: heap frei %lu, touch %d, stumm %d",
          E1_VERSION, (unsigned long)heap_bytes_free(), input_touch_available() ? 1 : 0,
          speaker_is_muted() ? 1 : 0);
}

static void prv_deinit(void) {
  app_focus_service_unsubscribe();
  if (s_game_timer) app_timer_cancel(s_game_timer);
  if (s_render_timer) app_timer_cancel(s_render_timer);
  if (s_light_timer) app_timer_cancel(s_light_timer);
  if (s_log_timer) app_timer_cancel(s_log_timer);
  if (s_jump_timer) app_timer_cancel(s_jump_timer);
  audio_deinit();
  haptics_deinit();
  backlight_enable(false);
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
