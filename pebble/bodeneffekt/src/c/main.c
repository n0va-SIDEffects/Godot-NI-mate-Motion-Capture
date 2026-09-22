#include <pebble.h>
#include "config.h"
#include "bclock.h"
#include "world.h"
#include "voxel.h"
#include "control.h"
#include "flight.h"
#include "duell.h"

// BODENEFFEKT, Phase 1. Zwei Bildschirme:
//   FLUG    - der Voxel-Tiefflug, beide Steuerprofile jederzeit umschaltbar
//   MESSUNG - Vollbildzeit, Strahlenzahl, Sichtweite, Ergebnisse
//   DUELL   - beide Steuerprofile auf derselben Strecke gegeneinander
//
// Tasten:
//   Back  kurz        Bildschirm wechseln (FLUG -> MESSUNG -> DUELL -> FLUG)
//   Back  Doppelklick Steuerprofil umschalten (Fingerstick <-> Tasten)
//   Back  lang 900 ms App beenden
//   FLUG:    Select halten = steigen (Tasten) bzw. Praezision (Finger)
//            Up/Down       = Roll (Tasten) bzw. Hoehentrimmung (Finger)
//   MESSUNG: Select = Test starten und Variante weiterschalten
//            Up     = Strahlenzahl 200 <-> 100 (2 px je Spalte)
//            Down   = Sichtweite 200 / 160 / 120 Zellen
//            Select Doppelklick = Nicklage der Fingersteuerung umkehren
//   DUELL:   Select = 60-Sekunden-Lauf mit dem aktuellen Profil starten

typedef enum { ScrFlug = 0, ScrMessung = 1, ScrDuell = 2 } Screen;

static Window *s_window;
static Layer *s_layer;
static AppTimer *s_game_timer;
static AppTimer *s_render_timer;
static AppTimer *s_log_timer;
static Screen s_screen = ScrFlug;

static uint32_t s_last_game_ms;
static uint32_t s_tick_gap_max;        // groesste Tick-Luecke seit dem letzten Log
static uint32_t s_tick_gap_max_ever;
static uint32_t s_render_gap_max;
static uint32_t s_last_render_ms;
static uint8_t s_panel_variant;
static uint32_t s_seed = 1;

static uint32_t s_sel_down_ms;
static uint32_t s_sel_last_click_ms;

static char s_l1[48];
static char s_l2[48];
static char s_t[VOX_TEXT_LINES][32];

// ---------------------------------------------------------------- Bildschirm
// Die Uhr rechnet die Empfehlung selbst aus, damit am Handgelenk nichts
// abgeschrieben und nachgeschlagen werden muss. Massgeblich ist die groessere
// von Vollbild- und Voxelzeit: die echte Szene ist der Lastfall, das leere
// Vollbild die Untergrenze der Uebertragung.
static const char *prv_ziel(uint32_t voll_x10, uint32_t voxel_x10) {
  const uint32_t t = voll_x10 > voxel_x10 ? voll_x10 : voxel_x10;
  if (t == 0) return "-";
  if (t < 280) return "30 fps (33ms)";
  if (t < 400) return "25 fps (40ms)";
  return "20 fps (50ms)";
}

static void prv_refresh_text(void) {
  const PanelStats *ps = voxel_panel_stats();
  const WorldInfo *wi = world_info();
  static const char *name[PANEL_VARIANTS] = { "Voll ", "10Z  ", "Voxel" };
  snprintf(s_t[0], sizeof(s_t[0]), "MESSUNG  %u St %u Z",
           (unsigned)voxel_rays(), (unsigned)voxel_sight_cells());
  snprintf(s_t[1], sizeof(s_t[1]), "        Bild / rast");
  for (int v = 0; v < PANEL_VARIANTS; v++) {
    snprintf(s_t[2 + v], sizeof(s_t[0]), "%s %lu.%lu / %lu.%lu ms", name[v],
             (unsigned long)(ps->result_x10[v] / 10), (unsigned long)(ps->result_x10[v] % 10),
             (unsigned long)(ps->result_rast_x10[v] / 10),
             (unsigned long)(ps->result_rast_x10[v] % 10));
  }
  snprintf(s_t[5], sizeof(s_t[0]), "Ziel %s",
           prv_ziel(ps->result_x10[0], ps->result_x10[2]));
  snprintf(s_t[6], sizeof(s_t[0]), "Welt %lums heap %luk",
           (unsigned long)wi->gen_ms, (unsigned long)(heap_bytes_free() / 1024));
  snprintf(s_t[7], sizeof(s_t[0]), "Sel=Test Up=St Dn=Sicht");
  for (int i = 0; i < VOX_TEXT_LINES; i++) voxel_set_text(i, s_t[i]);
}

static void prv_zeile_lauf(char *out, size_t n, const DuellLauf *l, const char *name) {
  if (!l->gueltig || l->dauer_ms == 0) {
    snprintf(out, n, "%s  -", name);
    return;
  }
  const uint32_t sohle = (l->sohle_ms * 100) / l->dauer_ms;
  const uint32_t blind = l->schatten_ms ? (l->blind_ms * 100) / l->schatten_ms : 0;
  const uint32_t agl = l->proben ? (l->agl_sum8 / l->proben) >> 8 : 0;
  snprintf(out, n, "%s %lu%% %uB %luh %lu%%", name, (unsigned long)sohle,
           (unsigned)l->kontakte, (unsigned long)agl, (unsigned long)blind);
}

static void prv_refresh_duell(void) {
  const DuellLauf *f = duell_ergebnis(CtrlFinger);
  const DuellLauf *t = duell_ergebnis(CtrlButton);
  snprintf(s_t[0], sizeof(s_t[0]), "DUELL  %s%s",
           control_profile() == CtrlFinger ? "FINGER" : "TASTEN",
           (control_profile() == CtrlFinger && control_pitch_invert()) ? " inv" : "");
  snprintf(s_t[1], sizeof(s_t[0]), "    Sohle Bod Hoeh Blind");
  prv_zeile_lauf(s_t[2], sizeof(s_t[0]), f, "Fing");
  prv_zeile_lauf(s_t[3], sizeof(s_t[0]), t, "Tast");
  // Der gespeicherte Lauf gilt nur fuer die Strecke, auf der er geflogen wurde.
  const uint32_t seed = world_info()->seed;
  const bool fremd = (f->gueltig && f->seed != seed) || (t->gueltig && t->seed != seed);
  snprintf(s_t[4], sizeof(s_t[0]), fremd ? "andere Strecke!" : "Strecke %lu",
           (unsigned long)seed);
  snprintf(s_t[5], sizeof(s_t[0]), "Sohle hoch, Blind tief");
  snprintf(s_t[6], sizeof(s_t[0]), "ist besser.");
  snprintf(s_t[7], sizeof(s_t[0]), "Sel=Lauf 60s BackBack=Prof");
  for (int i = 0; i < VOX_TEXT_LINES; i++) voxel_set_text(i, s_t[i]);
}

static void prv_refresh_hud(void) {
  const Flight *f = flight_state();
  const VoxelStats *vs = voxel_stats();
  snprintf(s_l1, sizeof(s_l1), "%s agl %ld.%ld%s trim%+ld",
           control_profile() == CtrlFinger ? "FINGER" : "TASTEN",
           (long)(f->agl8 >> 8), (long)(((f->agl8 & 255) * 10) >> 8),
           f->in_effect ? " *" : "", (long)(control_trim8() >> 8));
  if (duell_aktiv()) {
    snprintf(s_l2, sizeof(s_l2), "DUELL %lus  b%lu  sohle %lus",
             (unsigned long)((duell_rest_ms() + 999) / 1000), (unsigned long)f->contacts,
             (unsigned long)(f->effect_ms / 1000));
  } else {
    snprintf(s_l2, sizeof(s_l2), "%lu.%lu fps  %lu.%lu ms  b%lu",
             (unsigned long)(vs->fps_x10 / 10), (unsigned long)(vs->fps_x10 % 10),
             (unsigned long)(vs->render_ms_x10 / 10), (unsigned long)(vs->render_ms_x10 % 10),
             (unsigned long)f->contacts);
  }
  voxel_set_hud(s_l1, s_l2);
}

static void prv_set_screen(Screen s) {
  s_screen = s;
  voxel_panel_stop();
  if (s != ScrFlug) duell_abort();
  voxel_set_scene(s == ScrFlug);
  if (s == ScrMessung) prv_refresh_text();
  if (s == ScrDuell) prv_refresh_duell();
  layer_mark_dirty(s_layer);
}

// ---------------------------------------------------------------- Timer
static void prv_game_tick(void *data) {
  s_game_timer = app_timer_register(GAME_TICK_MS, prv_game_tick, NULL);
  const uint32_t now = bclock_now_ms();
  uint32_t dt = s_last_game_ms ? now - s_last_game_ms : GAME_TICK_MS;
  s_last_game_ms = now;
  // Tick-Luecke: alles ueber dem Raster ist Zeit, die der App-Task nicht hatte.
  if (dt > GAME_TICK_MS) {
    const uint32_t gap = dt - GAME_TICK_MS;
    if (gap > s_tick_gap_max) s_tick_gap_max = gap;
    if (gap > s_tick_gap_max_ever) s_tick_gap_max_ever = gap;
  }
  control_tick(now);
  if (s_screen == ScrFlug) {
    flight_step(dt);
    const Flight *f = flight_state();
    if (f->hit) voxel_set_flash(3);
    voxel_set_agl8(f->agl8);
    Camera cam;
    flight_fill_camera(&cam);
    voxel_set_camera(&cam);
    // Die Schattenzeile stammt aus dem zuletzt gezeichneten Bild, ist also
    // hoechstens ein Bild alt. Genauer geht es nicht, ohne die Projektion ein
    // zweites Mal zu rechnen, und fuer eine Sekundenstatistik reicht das.
    if (duell_tick(dt, f->agl8, f->hit, f->in_effect, voxel_shadow_row(),
                   control_stats())) {
      prv_set_screen(ScrDuell);          // Lauf zu Ende, Ergebnis zeigen
    }
  }
}

static void prv_render_tick(void *data) {
  s_render_timer = app_timer_register(RENDER_TICK_MS, prv_render_tick, NULL);
  const uint32_t now = bclock_now_ms();
  if (s_last_render_ms) {
    const uint32_t dt = now - s_last_render_ms;
    if (dt > RENDER_TICK_MS && (dt - RENDER_TICK_MS) > s_render_gap_max) {
      s_render_gap_max = dt - RENDER_TICK_MS;
    }
  }
  s_last_render_ms = now;
  if (voxel_panel_stats()->active) return;   // der Test taktet sich selbst
  if (s_screen == ScrFlug) {
    prv_refresh_hud();
  }
  layer_mark_dirty(s_layer);
}

static void prv_log_tick(void *data) {
  s_log_timer = app_timer_register(LOG_TICK_MS, prv_log_tick, NULL);
  if (voxel_panel_stats()->active) return;   // kein APP_LOG waehrend der Messung
  const VoxelStats *vs = voxel_stats();
  const PanelStats *ps = voxel_panel_stats();
  const Flight *f = flight_state();
  const CtrlStats *cs = control_stats();

  APP_LOG(APP_LOG_LEVEL_INFO,
          "[BE] %s fps=%lu.%lu rast=%lu.%lu/%lums voll=%lu.%lums heap=%lu",
          s_screen == ScrFlug ? (duell_aktiv() ? "DUEL" : "FLUG")
                              : (s_screen == ScrMessung ? "MESS" : "ERGB"),
          (unsigned long)(vs->fps_x10 / 10), (unsigned long)(vs->fps_x10 % 10),
          (unsigned long)(vs->render_ms_x10 / 10), (unsigned long)(vs->render_ms_x10 % 10),
          (unsigned long)vs->render_ms_max,
          (unsigned long)(ps->result_x10[0] / 10), (unsigned long)(ps->result_x10[0] % 10),
          (unsigned long)heap_bytes_free());
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[BEt] prof=%s inv=%d touch=%lu.%lu/s ivl=%lums dx=%d dy=%d rays=%u sicht=%u",
          control_profile() == CtrlFinger ? "F" : "T", control_pitch_invert() ? 1 : 0,
          (unsigned long)(cs->ev_rate_x10 / 10), (unsigned long)(cs->ev_rate_x10 % 10),
          (unsigned long)cs->min_interval_ms, (int)cs->dx, (int)cs->dy,
          (unsigned)voxel_rays(), (unsigned)voxel_sight_cells());
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[BE+] agl=%ld.%ld h=%ld yaw=%ld roll=%ld.%ld bod=%lu sohle=%lus lueck=%lu/%lums schritte=%lu",
          (long)(f->agl8 >> 8), (long)(((f->agl8 & 255) * 10) >> 8),
          (long)(f->h8 >> 8), (long)((f->yaw * 360) / TRIG_MAX_ANGLE),
          (long)(f->roll_deg8 >> 8), (long)(((f->roll_deg8 < 0 ? -f->roll_deg8 : f->roll_deg8) & 255) * 10 >> 8),
          (unsigned long)f->contacts, (unsigned long)(f->effect_ms / 1000),
          (unsigned long)s_tick_gap_max, (unsigned long)s_render_gap_max,
          (unsigned long)vs->steps_last);
  if (bclock_glitches() || bclock_resyncs() || bclock_long_gaps()) {
    APP_LOG(APP_LOG_LEVEL_INFO, "[BEu] glitch=%lu rs=%lu gaps=%lu",
            (unsigned long)bclock_glitches(), (unsigned long)bclock_resyncs(),
            (unsigned long)bclock_long_gaps());
  }
  s_tick_gap_max = 0;
  s_render_gap_max = 0;
  if (s_screen == ScrMessung) prv_refresh_text();
  if (s_screen == ScrDuell) prv_refresh_duell();
}

// ---------------------------------------------------------------- Tasten
static void prv_back_click(ClickRecognizerRef r, void *ctx) {
  prv_set_screen((Screen)((s_screen + 1) % 3));
}

static void prv_back_double(ClickRecognizerRef r, void *ctx) {
  control_toggle_profile();
  APP_LOG(APP_LOG_LEVEL_INFO, "[BE] Steuerprofil -> %s",
          control_profile() == CtrlFinger ? "Fingerstick" : "Tasten");
  if (s_screen == ScrMessung) prv_refresh_text();
  if (s_screen == ScrDuell) prv_refresh_duell();
}

static void prv_back_long(ClickRecognizerRef r, void *ctx) {
  window_stack_pop_all(true);
}

static void prv_select_down(ClickRecognizerRef r, void *ctx) {
  s_sel_down_ms = bclock_now_ms();
  control_button_select(true);
}

static void prv_select_up(ClickRecognizerRef r, void *ctx) {
  control_button_select(false);
  const uint32_t now = bclock_now_ms();
  if ((now - s_sel_down_ms) > 400) return;         // gehalten, kein Klick
  if (s_screen == ScrDuell) {
    // Ein Lauf beginnt immer an derselben Stelle, sonst vergleicht er nichts.
    flight_reset(64 << 16, 8 << 16);
    duell_start();
    prv_set_screen(ScrFlug);
    return;
  }
  if (s_screen != ScrMessung) return;
  if ((now - s_sel_last_click_ms) < 350) {
    // Doppelklick: Nicklage der Fingersteuerung umkehren
    control_set_pitch_invert(!control_pitch_invert());
    voxel_panel_stop();
    APP_LOG(APP_LOG_LEVEL_INFO, "[BE] Nicklage invertiert = %d", control_pitch_invert() ? 1 : 0);
    s_sel_last_click_ms = 0;
    prv_refresh_text();
    return;
  }
  s_sel_last_click_ms = now;
  voxel_panel_start(s_panel_variant);
  s_panel_variant = (uint8_t)((s_panel_variant + 1) % PANEL_VARIANTS);
}

static void prv_up_down(ClickRecognizerRef r, void *ctx) {
  if (s_screen == ScrMessung) {
    voxel_set_rays(voxel_rays() == RAYS_FULL ? RAYS_HALF : RAYS_FULL);
    prv_refresh_text();
    layer_mark_dirty(s_layer);
    return;
  }
  control_button_up(true);
  if (control_profile() == CtrlFinger) control_trim(TRIM_STEP);
}

static void prv_up_up(ClickRecognizerRef r, void *ctx) {
  control_button_up(false);
}

static void prv_down_down(ClickRecognizerRef r, void *ctx) {
  if (s_screen == ScrMessung) {
    voxel_set_sight((uint8_t)(voxel_sight() + 1));
    prv_refresh_text();
    layer_mark_dirty(s_layer);
    return;
  }
  control_button_down(true);
  if (control_profile() == CtrlFinger) control_trim(-TRIM_STEP);
}

static void prv_down_up(ClickRecognizerRef r, void *ctx) {
  control_button_down(false);
}

static void prv_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click);
  window_multi_click_subscribe(BUTTON_ID_BACK, 2, 2, 300, true, prv_back_double);
  window_long_click_subscribe(BUTTON_ID_BACK, 900, prv_back_long, NULL);
  window_raw_click_subscribe(BUTTON_ID_SELECT, prv_select_down, prv_select_up, NULL);
  window_raw_click_subscribe(BUTTON_ID_UP, prv_up_down, prv_up_up, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, prv_down_down, prv_down_up, NULL);
}

// ---------------------------------------------------------------- Fenster
static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect b = layer_get_bounds(root);
  s_layer = layer_create(b);
  layer_add_child(root, s_layer);
  voxel_init(s_layer);
  control_init(window);

  // Seed aus dem Datum: dieselbe Strecke am selben Tag.
  const time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  s_seed = (uint32_t)((lt->tm_year + 1900) * 10000 + (lt->tm_mon + 1) * 100 + lt->tm_mday);
  // Sonnenazimut aus der Uhrzeit: 6 Uhr Ost, 12 Uhr Sued, 18 Uhr West.
  const int32_t minutes = lt->tm_hour * 60 + lt->tm_min;
  const int32_t azim = (int32_t)(((int64_t)(minutes - 360) * TRIG_MAX_ANGLE) / (2 * 720));
  world_generate(s_seed, azim & (TRIG_MAX_ANGLE - 1));
  APP_LOG(APP_LOG_LEVEL_INFO, "[BE] Welt Seed=%lu in %lums, h %u..%u, heap=%lu",
          (unsigned long)s_seed, (unsigned long)world_info()->gen_ms,
          (unsigned)world_info()->h_min, (unsigned)world_info()->h_max,
          (unsigned long)heap_bytes_free());

  flight_reset(64 << 16, 8 << 16);
  Camera cam;
  flight_fill_camera(&cam);
  voxel_set_camera(&cam);
  prv_set_screen(ScrFlug);
}

static void prv_window_unload(Window *window) {
  voxel_deinit();
  control_deinit();
  layer_destroy(s_layer);
  s_layer = NULL;
}

static void prv_focus(bool in_focus) {
  // Beim Fokusverlust (Benachrichtigung) darf der Flug nicht weiterlaufen und
  // die Tick-Luecke nicht als Messwert zaehlen.
  if (in_focus) {
    s_last_game_ms = 0;
    s_last_render_ms = 0;
  }
}

static void prv_init(void) {
  bclock_init();
  duell_init();
  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_set_background_color(s_window, GColorBlack);
  app_focus_service_subscribe(prv_focus);
  window_stack_push(s_window, true);
  s_game_timer = app_timer_register(GAME_TICK_MS, prv_game_tick, NULL);
  s_render_timer = app_timer_register(RENDER_TICK_MS, prv_render_tick, NULL);
  s_log_timer = app_timer_register(LOG_TICK_MS, prv_log_tick, NULL);
}

static void prv_deinit(void) {
  if (s_game_timer) app_timer_cancel(s_game_timer);
  if (s_render_timer) app_timer_cancel(s_render_timer);
  if (s_log_timer) app_timer_cancel(s_log_timer);
  s_game_timer = s_render_timer = s_log_timer = NULL;
  app_focus_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
  return 0;
}
