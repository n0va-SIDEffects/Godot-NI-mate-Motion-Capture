#include <pebble.h>
#include "config.h"
#include "e1clock.h"
#include "fixed.h"
#include "physics.h"
#include "game.h"
#include "render.h"
#include "input.h"
#include "haptics.h"
#include "nudge.h"
#include "audio.h"
#include "tone.h"

// SILBERKUGEL, Phase 1. Vier Bildschirme, Wechsel mit langem Druck auf Up:
//   SPIEL   Flipper auf dem grauen Testtisch
//   MAGNET  Uebung: Kugel blind unter der Fingerkuppe halten (Jury-Hauptkritik)
//   PANEL   Vollbildzeit messen (Vollbild gegen 10 Zeilen)
//   MESS    Zahlen, Physik-Stresstest, Umschalter
//
// Tastenbelegung im Zangengriff (Konzept): Back = linker Flipper,
// Down = rechter Flipper, Select = Magnetgriff, Up = Plunger.
// Weil Back damit belegt ist, laesst sich die App aus SPIEL heraus nicht mit
// Back verlassen; in den anderen Bildschirmen und im Daumen-Modus schon.

typedef enum {
  ModeSpiel = 0,
  ModeMagnet,
  ModePanel,
  ModeMess,
  ModeCount,
} Mode;

static const char *s_mode_names[ModeCount] = { "SPIEL", "MAGNET", "PANEL", "MESS" };

static Window *s_window;
static Layer *s_layer;
static World s_world;
static Mode s_mode = ModeSpiel;
static AppTimer *s_game_timer;
static AppTimer *s_render_timer;
static AppTimer *s_log_timer;
static uint32_t s_last_tick_ms;
static uint32_t s_tick_gap_max;
static uint32_t s_game_ms_max;
static bool s_paused;
// Standard ist der Daumen-Modus, weil Back keine Rohevents liefert und
// wiederholende Klicks auf Back die App verlassen (beides gemessen, siehe
// docs/emulator-befund.md). Der Zangengriff bleibt als Option, dort schlaegt
// der linke Flipper beim Loslassen und faellt von selbst zurueck.
static bool s_thumb_mode = true;
static bool s_audio_load = true; // Ton als Last mitlaufen lassen
static bool s_quiet;             // Sekundenlog aus (APP_LOG haelt den App-Task an)
static uint8_t s_panel_variant;
static uint32_t s_log_tick;
static uint32_t s_back_hold_until;     // Zangengriff: Auslauf des Back-Flippers
static uint32_t s_back_repeats;        // wie oft der wiederholende Klick kam
// Risiko aus dem Konzept: Kommen die Rohevents der Seitentasten ueberhaupt an?
static uint32_t s_raw_down_ev[NUM_BUTTONS];
static uint32_t s_raw_up_ev[NUM_BUTTONS];

static void prv_update_hud(void);

static void prv_mark_dirty(void) {
  if (s_layer) {
    layer_mark_dirty(s_layer);
  }
}

// --------------------------------------------------------------------- Tasten

static void prv_flipper_down(ClickRecognizerRef rec, void *ctx) {
  ButtonId id = click_recognizer_get_button_id(rec);
  if (id < NUM_BUTTONS) {
    s_raw_down_ev[id]++;
  }
  nudge_button_mask();
  uint8_t idx = 0;
  if (s_thumb_mode) {
    idx = (id == BUTTON_ID_UP) ? 0 : 1;
  } else {
    idx = (id == BUTTON_ID_BACK) ? 0 : 1;
  }
  phys_set_flipper(&s_world, idx, true);
  // Der Thunk faellt auf den Tastendruck, nicht auf den Kontakt mit der Kugel:
  // Ein echter Automat knallt auch bei Leerschlaegen.
  haptics_pulse(LRA_FLIPPER_MS, HapEvent);
}

static void prv_flipper_up(ClickRecognizerRef rec, void *ctx) {
  ButtonId id = click_recognizer_get_button_id(rec);
  if (id < NUM_BUTTONS) {
    s_raw_up_ev[id]++;
  }
  nudge_button_mask();
  uint8_t idx = 0;
  if (s_thumb_mode) {
    idx = (id == BUTTON_ID_UP) ? 0 : 1;
  } else {
    idx = (id == BUTTON_ID_BACK) ? 0 : 1;
  }
  phys_set_flipper(&s_world, idx, false);
}

// Ein reiner Raw-Handler auf Back genuegt dem System nicht: Es poppt das
// Fenster trotzdem, solange kein gewoehnlicher Click-Handler auf Back sitzt.
// Im Emulator gemessen (siehe docs/emulator-befund.md). Deshalb dieser leere
// Handler: Er faengt den Systemgriff ab, die Arbeit machen die Raw-Handler.
// Linker Flipper im Zangengriff. Auf Back geht nur der gewoehnliche Klick:
// Rohevents kommen dort nicht an, und wiederholende Klicks lassen das System
// die App verlassen. Der Klick faellt auf das Loslassen der Taste, der Flipper
// schlaegt also verzoegert und faellt nach BACK_HOLD_MS von selbst zurueck.
// Kugel fangen und Post-Pass gehen damit nicht; dafuer gibt es den
// Daumen-Modus, in dem Up und Down echte Rohevents liefern.
static void prv_back_flip(ClickRecognizerRef rec, void *ctx) {
  uint32_t now = e1clock_now_ms();
  s_back_hold_until = now + BACK_HOLD_MS;
  s_back_repeats++;
  nudge_button_mask();
  phys_set_flipper(&s_world, 0, true);
  haptics_pulse(LRA_FLIPPER_MS, HapEvent);
}

static void prv_enter_mode(Mode m);

// Im Daumen-Modus ist Back frei: kurzer Druck wechselt den Bildschirm, langer
// Druck verlaesst die App (das nimmt einem das System ohnehin nicht ab).
static void prv_back_mode(ClickRecognizerRef rec, void *ctx) {
  nudge_button_mask();
  prv_enter_mode((Mode)((s_mode + 1) % ModeCount));
}

// Select ist der Magnetgriff. Solange die Kugel aber noch in der Abschussbahn
// liegt, gibt es nichts zu greifen: Dann ist es der Plunger. So bleibt der
// Daumen-Modus ohne Touch vollstaendig spielbar, und im Emulator laesst sich
// ueberhaupt eine Kugel ins Spiel bringen.
static void prv_select_raw_down(ClickRecognizerRef rec, void *ctx) {
  s_raw_down_ev[BUTTON_ID_SELECT]++;
  nudge_button_mask();
  if (game_ball_state() == BallLane) {
    game_plunge_button(true);
  } else {
    game_set_grab(true);
  }
}

static void prv_select_raw_up(ClickRecognizerRef rec, void *ctx) {
  s_raw_up_ev[BUTTON_ID_SELECT]++;
  nudge_button_mask();
  game_plunge_button(false);
  game_set_grab(false);
}

static void prv_select_click(ClickRecognizerRef rec, void *ctx) {
  nudge_button_mask();
  switch (s_mode) {
    case ModeSpiel:
      if (game_ball_state() == BallIdle || game_ball_state() == BallDrained) {
        game_new_ball();
      }
      break;
    case ModeMagnet:
      game_reset_stats();
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
    case ModeMess:
      game_bench_physics();
      break;
    default:
      break;
  }
  prv_update_hud();
  prv_mark_dirty();
}

static void prv_plunger_raw_down(ClickRecognizerRef rec, void *ctx) {
  s_raw_down_ev[BUTTON_ID_UP]++;
  nudge_button_mask();
  game_plunge_button(true);
}

static void prv_plunger_raw_up(ClickRecognizerRef rec, void *ctx) {
  s_raw_up_ev[BUTTON_ID_UP]++;
  nudge_button_mask();
  game_plunge_button(false);
}

static void prv_down_click(ClickRecognizerRef rec, void *ctx) {
  nudge_button_mask();
  if (s_mode == ModeMess) {
    s_thumb_mode = !s_thumb_mode;
    APP_LOG(APP_LOG_LEVEL_INFO, "[P1] Belegung: %s",
            s_thumb_mode ? "Daumen-Modus (Up/Down)" : "Zangengriff (Back/Down)");
    window_set_click_config_provider(s_window, window_get_click_config_provider(s_window));
    prv_update_hud();
    prv_mark_dirty();
  }
}

static void prv_down_multi(ClickRecognizerRef rec, void *ctx) {
  if (s_mode != ModeMess) {
    return;
  }
  if (click_number_of_clicks_counted(rec) >= 3) {
    s_quiet = !s_quiet;
    APP_LOG(APP_LOG_LEVEL_INFO, "[P1] Sekundenlog %s", s_quiet ? "AUS" : "an");
  } else {
    s_audio_load = !s_audio_load;
    if (s_audio_load) {
      audio_start();
    } else {
      audio_stop();
    }
    APP_LOG(APP_LOG_LEVEL_INFO, "[P1] Tonlast %s", s_audio_load ? "an" : "AUS");
  }
  prv_update_hud();
  prv_mark_dirty();
}

static void prv_up_click(ClickRecognizerRef rec, void *ctx) {
  nudge_button_mask();
  game_speed_next();
  prv_update_hud();
  prv_mark_dirty();
}

static void prv_up_long(ClickRecognizerRef rec, void *ctx) {
  nudge_button_mask();
  game_plunge_button(false);   // ein gehaltener Plunger wird nicht abgeschossen
  prv_enter_mode((Mode)((s_mode + 1) % ModeCount));
}

static void prv_click_config(void *ctx) {
  // Der lange Druck auf Up wechselt ueberall den Bildschirm. Select ist die
  // Aktionstaste. Alles andere haengt am Bildschirm und an der Belegung.
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);

  if (s_mode == ModeSpiel || s_mode == ModeMagnet) {
    window_raw_click_subscribe(BUTTON_ID_SELECT, prv_select_raw_down, prv_select_raw_up, NULL);
    if (s_thumb_mode) {
      // Beide Flipper am Daumen, beide mit echtem Halten.
      window_raw_click_subscribe(BUTTON_ID_UP, prv_flipper_down, prv_flipper_up, NULL);
      window_raw_click_subscribe(BUTTON_ID_DOWN, prv_flipper_down, prv_flipper_up, NULL);
      window_single_click_subscribe(BUTTON_ID_BACK, prv_back_mode);
    } else {
      window_single_click_subscribe(BUTTON_ID_BACK, prv_back_flip);
      window_raw_click_subscribe(BUTTON_ID_DOWN, prv_flipper_down, prv_flipper_up, NULL);
      window_raw_click_subscribe(BUTTON_ID_UP, prv_plunger_raw_down, prv_plunger_raw_up, NULL);
      window_long_click_subscribe(BUTTON_ID_UP, 1000, prv_up_long, NULL);
    }
  } else {
    window_long_click_subscribe(BUTTON_ID_UP, 1000, prv_up_long, NULL);
    window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
    window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
    window_multi_click_subscribe(BUTTON_ID_DOWN, 2, 3, 300, true, prv_down_multi);
  }
}

// --------------------------------------------------------------------- Modi

static void prv_enter_mode(Mode m) {
  if (s_mode == ModePanel) {
    render_panel_test_stop();
  }
  if (s_mode == ModeMagnet && m != ModeMagnet) {
    game_set_magnet_drill(false);
  }
  s_mode = m;
  haptics_geiger(0);
  phys_set_flipper(&s_world, 0, false);
  phys_set_flipper(&s_world, 1, false);
  game_set_grab(false);
  if (m == ModeMagnet) {
    game_set_magnet_drill(true);
  } else if (m == ModeSpiel) {
    if (game_ball_state() == BallIdle) {
      game_new_ball();
    }
  } else if (m == ModePanel) {
    s_panel_variant = 0;
  }
  window_set_click_config_provider(s_window, prv_click_config);
  APP_LOG(APP_LOG_LEVEL_INFO, "[P1] Bildschirm %s", s_mode_names[m]);
  prv_update_hud();
  prv_mark_dirty();
}

// --------------------------------------------------------------------- HUD

static void prv_update_hud(void) {
  char l1[48], l2[48], l3[48];
  const GameStats *g = game_stats();
  const RenderStats *r = render_stats();
  const AudioStats *a = audio_stats();
  const TouchState *t = input_state();
  const HapStats *h = haptics_stats();
  const NudgeState *n = nudge_state();

  switch (s_mode) {
    case ModeSpiel:
      // Nur eine Zeile, und die liegt oben im Rand: Der Flipperbereich unten
      // muss frei bleiben, sonst misst man die Lesbarkeit des eigenen HUD.
      snprintf(l1, sizeof(l1), "%c L%u B%lu ab%lu b%lu %lu.%lufps%s",
               game_ball_state() == BallLane ? 'P' : (s_thumb_mode ? 'D' : 'Z'),
               (unsigned)g->charge, (unsigned long)g->balls, (unsigned long)g->drains,
               (unsigned long)g->bumper, (unsigned long)(r->fps_x10 / 10),
               (unsigned long)(r->fps_x10 % 10),
               n->tilted ? " TILT" : (n->warn ? " !" : ""));
      if (game_speed_pct() != 100) {
        size_t k = strlen(l1);
        snprintf(l1 + k, sizeof(l1) - k, " %u%%", (unsigned)game_speed_pct());
      }
      l2[0] = '\0';
      l3[0] = '\0';
      break;
    case ModeMagnet: {
      uint32_t pct = g->play_ms ? (g->occluded_ms * 100) / g->play_ms : 0;
      snprintf(l1, sizeof(l1), "MAGNET %lu/%lu L%u halt %lums",
               (unsigned long)g->hold_targets, (unsigned long)g->hold_attempts,
               (unsigned)g->charge, (unsigned long)g->hold_cur_ms);
      l2[0] = '\0';
      snprintf(l3, sizeof(l3), "best %lums verdeckt %lu%% geiger %lu",
               (unsigned long)g->hold_best_ms, (unsigned long)pct, (unsigned long)h->geiger);
      break;
    }
    case ModePanel: {
      const PanelStats *p = render_panel_stats();
      snprintf(l1, sizeof(l1), "PANEL %s  Sel=%s", p->variant == 0 ? "Vollbild" : "10 Zeilen",
               p->active ? "laeuft" : "start");
      snprintf(l2, sizeof(l2), "voll %lu.%lu ms  band %lu.%lu ms",
               (unsigned long)(p->result_full_x10 / 10), (unsigned long)(p->result_full_x10 % 10),
               (unsigned long)(p->result_band_x10 / 10), (unsigned long)(p->result_band_x10 % 10));
      snprintf(l3, sizeof(l3), "%lu Frames  %lu.%lu fps", (unsigned long)p->frames,
               (unsigned long)(p->fps_x10 / 10), (unsigned long)(p->fps_x10 % 10));
      break;
    }
    case ModeMess:
      snprintf(l1, sizeof(l1), "MESS  Sel=Physik-Test  g=%u",
               (unsigned)((GRAVITY_PX_S2 * game_speed_pct()) / 100));
      snprintf(l2, sizeof(l2), "phys %lu.%luus rend %lu.%lums gap %lums",
               (unsigned long)(g->phys_us_per_substep_x10 / 10),
               (unsigned long)(g->phys_us_per_substep_x10 % 10),
               (unsigned long)(r->render_ms_x10 / 10), (unsigned long)(r->render_ms_x10 % 10),
               (unsigned long)s_tick_gap_max);
      snprintf(l3, sizeof(l3), "Up=Tempo %u%% Dn=%s 2x=Ton%s", (unsigned)game_speed_pct(),
               s_thumb_mode ? "Zange" : "Daumen", s_audio_load ? "aus" : "an");
      break;
    default:
      l1[0] = l2[0] = l3[0] = '\0';
      break;
  }
  (void)t;
  (void)a;
  (void)h;
  render_set_hud(l1, l2, l3);
}

// --------------------------------------------------------------------- Timer

static void prv_game_tick(void *data) {
  s_game_timer = app_timer_register(GAME_TICK_MS, prv_game_tick, NULL);
  uint32_t now = e1clock_now_ms();
  uint32_t dt = s_last_tick_ms ? now - s_last_tick_ms : GAME_TICK_MS;
  if (s_last_tick_ms && dt > s_tick_gap_max) {
    s_tick_gap_max = dt;
  }
  s_last_tick_ms = now;
  if (dt > 200) {
    dt = 200;
  }
  if (s_paused || !s_layer) {
    return;
  }
  if (s_back_hold_until != 0 && (int32_t)(now - s_back_hold_until) >= 0) {
    s_back_hold_until = 0;
    phys_set_flipper(&s_world, 0, false);
  }
  input_tick(now);
  nudge_tick(now);
  haptics_tick(now);
  if (s_mode == ModeSpiel || s_mode == ModeMagnet) {
    game_tick(now, dt);
  }
  uint32_t dur = e1clock_now_ms() - now;
  if (dur > s_game_ms_max) {
    s_game_ms_max = dur;
  }
}

static void prv_render_tick(void *data) {
  s_render_timer = app_timer_register(RENDER_TICK_MS, prv_render_tick, NULL);
  // Bild und HUD halten den App-Task auf: vorher den Tonvorlauf auffuellen.
  audio_top_up(audio_target_ms());
  if (s_paused || !s_layer) {
    return;
  }
  Overlay ov;
  game_fill_overlay(&ov);
  if (s_mode != ModeSpiel && s_mode != ModeMagnet) {
    ov.finger = false;
    ov.target_r = 0;
  }
  render_set_overlay(&ov);
  prv_update_hud();
  if (s_mode != ModePanel || !render_panel_stats()->active) {
    prv_mark_dirty();
  }
}

static void prv_log_tick(void *data) {
  s_log_timer = app_timer_register(LOG_TICK_MS, prv_log_tick, NULL);
  if (s_paused || s_quiet) {
    return;
  }
  audio_top_up(audio_target_ms());
  s_log_tick++;
  const GameStats *g = game_stats();
  const RenderStats *r = render_stats();
  const AudioStats *a = audio_stats();
  const TouchState *t = input_state();
  const HapStats *h = haptics_stats();
  const NudgeState *n = nudge_state();
  const PhysStats *p = &s_world.st;

  // APP_LOG schneidet bei rund 87 Zeichen ab, deshalb mehrere kurze Zeilen.
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[P1] %s fps=%lu.%lu rend=%lu.%lums gap=%lums spiel=%lums q=%lums ur=%lu",
          s_mode_names[s_mode], (unsigned long)(r->fps_x10 / 10), (unsigned long)(r->fps_x10 % 10),
          (unsigned long)(r->render_ms_x10 / 10), (unsigned long)(r->render_ms_x10 % 10),
          (unsigned long)s_tick_gap_max, (unsigned long)s_game_ms_max,
          (unsigned long)a->queue_ms_est, (unsigned long)a->underruns);
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[P1p] sub=%lu seg=%lu krs=%lu flp=%lu split=%lu esc=%lu vmax=%ld ball=%ld,%ld tempo=%u",
          (unsigned long)p->substeps, (unsigned long)p->contacts_seg,
          (unsigned long)p->contacts_circ, (unsigned long)p->contacts_flip,
          (unsigned long)p->splits_max,
          (unsigned long)p->escapes, (long)FX_TO_INT(p->speed_max),
          (long)FX_TO_INT(s_world.ball[0].p.x), (long)FX_TO_INT(s_world.ball[0].p.y),
          (unsigned)game_speed_pct());
  if ((s_log_tick % 5) != 1) {
    return;
  }
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[P1t] touch=%lu/s ivl=%lu..%lums stale=%lu mag=%lums verdeckt=%lums finger=%lums",
          (unsigned long)(t->ev_rate_x10 / 10), (unsigned long)t->min_interval_ms,
          (unsigned long)t->max_interval_ms, (unsigned long)t->stale_drops,
          (unsigned long)g->mag_ms, (unsigned long)g->occluded_ms, (unsigned long)g->finger_ms);
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[P1h] lra=%lu drop=%lu queue=%lu geiger=%lu mingap=%lums wait=%lums",
          (unsigned long)h->calls, (unsigned long)h->dropped, (unsigned long)h->queued,
          (unsigned long)h->geiger, (unsigned long)h->min_gap_ms, (unsigned long)h->busy_ms_max);
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[P1a] acc=%lu nudge=%lu tap=%lu mask=%lu/%lu/%lu peak=%dmg lean=%d/%d bob=%ld",
          (unsigned long)n->samples, (unsigned long)n->nudges,
          (unsigned long)n->taps, (unsigned long)n->masked_vib, (unsigned long)n->masked_flag,
          (unsigned long)n->masked_button, (int)n->peak_hp_mg, (int)n->lean_x_mg,
          (int)n->lean_y_mg, (long)n->bob);
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[P1+] baelle=%lu ab=%lu skill=%lu griff=%lu wurf=%lu heap=%lu stau=%lu",
          (unsigned long)g->balls, (unsigned long)g->drains, (unsigned long)g->skill_shots,
          (unsigned long)g->grabs, (unsigned long)g->throws,
          (unsigned long)heap_bytes_free(), (unsigned long)a->stalls);
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[P1b] back_rep=%lu raw: back=%lu/%lu down=%lu/%lu up=%lu/%lu select=%lu/%lu",
          (unsigned long)s_back_repeats,
          (unsigned long)s_raw_down_ev[BUTTON_ID_BACK], (unsigned long)s_raw_up_ev[BUTTON_ID_BACK],
          (unsigned long)s_raw_down_ev[BUTTON_ID_DOWN], (unsigned long)s_raw_up_ev[BUTTON_ID_DOWN],
          (unsigned long)s_raw_down_ev[BUTTON_ID_UP], (unsigned long)s_raw_up_ev[BUTTON_ID_UP],
          (unsigned long)s_raw_down_ev[BUTTON_ID_SELECT],
          (unsigned long)s_raw_up_ev[BUTTON_ID_SELECT]);
}

// --------------------------------------------------------------------- Fenster

static void prv_will_focus(bool in_focus) {
  if (!in_focus && !s_paused) {
    s_paused = true;
    haptics_stop();
    audio_stop();
    APP_LOG(APP_LOG_LEVEL_INFO, "[P1] Fokus verloren, pausiert");
  }
}

static void prv_did_focus(bool in_focus) {
  if (in_focus && s_paused) {
    s_paused = false;
    s_last_tick_ms = 0;
    if (s_audio_load) {
      audio_start();
    }
    APP_LOG(APP_LOG_LEVEL_INFO, "[P1] Fokus zurueck, weiter");
  }
  if (in_focus) {
    prv_mark_dirty();
  }
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_layer = layer_create(layer_get_bounds(root));
  layer_add_child(root, s_layer);
  render_init(s_layer);
  render_set_world(&s_world);
  input_init(window);
  prv_enter_mode(ModeSpiel);
}

static void prv_window_unload(Window *window) {
  if (s_game_timer) { app_timer_cancel(s_game_timer); s_game_timer = NULL; }
  if (s_render_timer) { app_timer_cancel(s_render_timer); s_render_timer = NULL; }
  if (s_log_timer) { app_timer_cancel(s_log_timer); s_log_timer = NULL; }
  haptics_stop();
  render_deinit();
  input_deinit();
  Layer *l = s_layer;
  s_layer = NULL;
  layer_destroy(l);
}

static void prv_init(void) {
  e1clock_init();
  phys_init(&s_world);
  game_init(&s_world);
  haptics_init();
  nudge_init();
  tone_init();
  audio_init();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, prv_click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  audio_set_source(AudioSourceTone);
  if (s_audio_load) {
    audio_start();
  }
  app_focus_service_subscribe_handlers((AppFocusHandlers) {
    .will_focus = prv_will_focus,
    .did_focus = prv_did_focus,
  });
  s_game_timer = app_timer_register(GAME_TICK_MS, prv_game_tick, NULL);
  s_render_timer = app_timer_register(RENDER_TICK_MS, prv_render_tick, NULL);
  s_log_timer = app_timer_register(LOG_TICK_MS, prv_log_tick, NULL);

  APP_LOG(APP_LOG_LEVEL_INFO,
          "[P1] Silberkugel P1 v%s: heap frei %lu, touch %d, stumm %d, welt %u B",
          P1_VERSION, (unsigned long)heap_bytes_free(), input_touch_available() ? 1 : 0,
          speaker_is_muted() ? 1 : 0, (unsigned)sizeof(World));
}

static void prv_deinit(void) {
  app_focus_service_unsubscribe();
  if (s_game_timer) app_timer_cancel(s_game_timer);
  if (s_render_timer) app_timer_cancel(s_render_timer);
  if (s_log_timer) app_timer_cancel(s_log_timer);
  audio_deinit();
  nudge_deinit();
  game_deinit();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
