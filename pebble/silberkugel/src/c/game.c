#include "game.h"
#include "config.h"
#include "e1clock.h"
#include "input.h"
#include "haptics.h"
#include "nudge.h"

static World *s_w;
static GameStats s_st;
static BallState s_state;
static uint32_t s_state_since;
static bool s_grab_btn;
static bool s_plunge_btn;
static uint32_t s_plunge_btn_since;
static uint8_t s_plunge_btn_ticks;
static int32_t s_charge_x100;       // Ladung mit Nachkommastellen, damit 25/s sauber laufen
static uint32_t s_phys_accum_ms;
static bool s_drill;
static int16_t s_target_x, s_target_y;
static bool s_target_hit;
static uint32_t s_seed = 0x5C1BE12;

static uint32_t prv_rand(void) {
  // Deterministisch und billig; der Tagestisch des Konzepts wuerde spaeter den
  // Startwert aus dem Datum ziehen.
  s_seed = s_seed * 1664525u + 1013904223u;
  return s_seed >> 8;
}

static void prv_new_target(void) {
  // Ziele nur im mittleren Band: oben sitzt das HUD, unten die Flipperzone,
  // in der der Magnet ohnehin nicht wirkt.
  s_target_x = (int16_t)(40 + (prv_rand() % 100));
  s_target_y = (int16_t)(60 + (prv_rand() % 90));
  s_target_hit = false;
  s_st.hold_cur_ms = 0;
  s_st.hold_attempts++;
}

void game_init(World *w) {
  s_w = w;
  memset(&s_st, 0, sizeof(s_st));
  s_state = BallIdle;
  s_state_since = 0;
  s_grab_btn = false;
  s_plunge_btn = false;
  s_charge_x100 = MAG_CHARGE_MAX * 100;
  s_phys_accum_ms = 0;
  s_drill = false;
  prv_new_target();
}

void game_deinit(void) {
}

void game_reset_stats(void) {
  uint32_t bench = s_st.phys_us_per_substep_x10;
  memset(&s_st, 0, sizeof(s_st));
  s_st.phys_us_per_substep_x10 = bench;
  phys_reset_stats(s_w);
  haptics_reset_stats();
}

void game_set_magnet_drill(bool on) {
  if (s_drill == on) {
    return;
  }
  s_drill = on;
  phys_clear_balls(s_w);
  s_state = BallIdle;
  if (on) {
    // Uebung: Die Kugel startet frei im Feld, damit man sofort ueben kann.
    Ball *b = phys_spawn_lane(s_w);
    if (b) {
      b->p = vec_make(FX_FROM_INT(91), FX_FROM_INT(40));
      b->v = vec_make(0, 0);
      b->in_lane = false;
      for (uint8_t k = 0; k < BALL_TRAIL; k++) {
        b->trail[k] = b->p;
      }
    }
    s_state = BallPlay;
    s_charge_x100 = MAG_CHARGE_MAX * 100;
    prv_new_target();
  }
}

bool game_magnet_drill(void) {
  return s_drill;
}

void game_set_grab(bool pressed) {
  s_grab_btn = pressed;
}

void game_new_ball(void) {
  phys_clear_balls(s_w);
  Ball *b = phys_spawn_lane(s_w);
  if (!b) {
    return;
  }
  s_state = BallLane;
  s_state_since = e1clock_now_ms();
  s_st.balls++;
  s_charge_x100 = MAG_CHARGE_MAX * 100;
  nudge_reset_tilt();
  nudge_calibrate();
}

static void prv_launch(int16_t pull_px, uint8_t ticks) {
  Ball *b = NULL;
  for (uint8_t i = 0; i < s_w->ball_count; i++) {
    if (s_w->ball[i].alive && s_w->ball[i].in_lane) {
      b = &s_w->ball[i];
      break;
    }
  }
  if (!b) {
    return;
  }
  int32_t span = PLUNGER_MAX_PX_S - PLUNGER_MIN_PX_S;
  int32_t speed = PLUNGER_MIN_PX_S + (span * pull_px) / PLUNGER_PULL_MAX_PX;
  b->v = vec_make(0, -FX_FROM_INT(speed));
  s_state = BallPlay;
  s_state_since = e1clock_now_ms();
  s_st.plunges++;
  if (ticks == PLUNGER_SKILL_TICKS) {
    s_st.skill_shots++;
    s_charge_x100 = MAG_CHARGE_MAX * 100;
  }
  haptics_pulse(LRA_FLIPPER_MS, HapEvent);
  nudge_calibrate();
  APP_LOG(APP_LOG_LEVEL_INFO, "[P1][PLUNGER] Zug %d px, %u Ticks, %ld px/s%s",
          (int)pull_px, (unsigned)ticks, (long)speed,
          ticks == PLUNGER_SKILL_TICKS ? ", Skill-Shot" : "");
}

// Magnetfinger: Position und Staerke fuer die Physik setzen, Ladung abbuchen,
// Naehe-Geiger stellen, Verdeckung mitzaehlen.
static void prv_magnet(uint32_t now, uint32_t dt_ms) {
  int16_t fx, fy;
  bool finger = input_finger(&fx, &fy);
  s_w->mag_on = false;
  if (finger) {
    s_st.finger_ms += dt_ms;
  }
  if (!finger || input_plunger_active()) {
    haptics_geiger(0);
    if (s_charge_x100 < MAG_CHARGE_MAX * 100) {
      s_charge_x100 += (int32_t)((MAG_RECHARGE_PER_S * dt_ms) / 10);
      if (s_charge_x100 > MAG_CHARGE_MAX * 100) {
        s_charge_x100 = MAG_CHARGE_MAX * 100;
      }
    }
    return;
  }

  Vec fpos = vec_make(FX_FROM_INT(fx), FX_FROM_INT(fy));
  // Naechste Kugel suchen: Sie bestimmt Geiger-Rate und Griffmoeglichkeit.
  Ball *near = NULL;
  int32_t near_px = 1 << 20;
  for (uint8_t i = 0; i < s_w->ball_count; i++) {
    Ball *b = &s_w->ball[i];
    if (!b->alive) {
      continue;
    }
    fix d = vec_len(vec_sub(b->p, fpos));
    int32_t dpx = FX_TO_INT(d);
    if (dpx < near_px) {
      near_px = dpx;
      near = b;
    }
  }

  bool charged = s_charge_x100 > 0;
  if (charged) {
    s_w->mag_on = true;
    s_w->mag_pos = fpos;
    // Volle Ladung zieht mit voller Kraft, eine fast leere nur noch schwach.
    int32_t pct = s_charge_x100 / (MAG_CHARGE_MAX);   // 0..100
    if (pct > 100) {
      pct = 100;
    }
    s_w->mag_accel = (fix)(((int64_t)FX_FROM_INT(MAG_ACCEL_MAX_PX_S2) * (40 + (pct * 60) / 100)) / 100);
  }

  if (near) {
    if (near_px <= FINGER_TIP_R_PX && s_state == BallPlay) {
      s_st.occluded_ms += dt_ms;     // die Kugel liegt unter der Kuppe: unsichtbar
    }
    if (near_px <= MAG_RADIUS_PX) {
      // Geigerzaehler: Tickabstand kodiert die Entfernung, von 400 ms am
      // Ringrand bis 60 ms direkt unter der Kuppe.
      int32_t span = GEIGER_SLOW_MS - GEIGER_FAST_MS;
      int32_t period = GEIGER_FAST_MS + (span * near_px) / MAG_RADIUS_PX;
      haptics_geiger((uint32_t)period);
      if (charged && near->p.y < FX_FROM_INT(MAG_DEAD_Y) && !near->held) {
        s_st.mag_ms += dt_ms;
        // In der Uebung kostet der Magnet nichts: Gemessen werden soll, ob
        // sich die Kugel unter der verdeckenden Fingerkuppe fuehren laesst,
        // nicht wie lange die Ladung reicht. Im Spiel zieht der Verbrauch.
        if (!s_drill) {
          s_charge_x100 -= (int32_t)((MAG_DRAIN_PER_S * dt_ms) / 10);
          if (s_charge_x100 < 0) {
            s_charge_x100 = 0;
          }
        }
      }
    } else {
      haptics_geiger(0);
    }

    // Griff: Select gehalten, Finger ueber der Kugel, genug Ladung.
    if (near->held) {
      if (!s_grab_btn || !charged) {
        // Notwurf: Die Kugel faellt mit der Geschwindigkeit des Fingers aus den
        // letzten Positionsereignissen. Das TouchEvent traegt keine Zeit, die
        // Zeitstempel kommen aus e1clock beim Eingang.
        fix vx, vy;
        input_finger_velocity(&vx, &vy);
        near->held = false;
        near->v = vec_make(vx, vy);
        s_charge_x100 = 0;
        s_st.throws++;
        haptics_pulse(LRA_FLIPPER_MS, HapEvent);
        APP_LOG(APP_LOG_LEVEL_INFO, "[P1][WURF] v = %ld / %ld px/s",
                (long)FX_TO_INT(vx), (long)FX_TO_INT(vy));
      } else {
        near->p = fpos;
        near->v = vec_make(0, 0);
        if (!s_drill) {
          s_charge_x100 -= (int32_t)((MAG_DRAIN_PER_S * 2 * dt_ms) / 10);
          if (s_charge_x100 <= 0) {
            s_charge_x100 = 0;
          }
        }
      }
    } else if (s_grab_btn && near_px <= MAG_GRAB_DIST_PX &&
               s_charge_x100 >= 30 * 100 && near->p.y < FX_FROM_INT(MAG_DEAD_Y)) {
      near->held = true;
      s_st.grabs++;
      haptics_pulse(LRA_BUMPER_MS, HapEvent);
    }
  } else {
    haptics_geiger(0);
  }
}

static void prv_drill(uint32_t now, uint32_t dt_ms) {
  Ball *b = NULL;
  for (uint8_t i = 0; i < s_w->ball_count; i++) {
    if (s_w->ball[i].alive) {
      b = &s_w->ball[i];
      break;
    }
  }
  if (!b) {
    // Kugel abgeflossen: neue setzen, der Versuch zaehlt als beendet.
    b = phys_spawn_lane(s_w);
    if (b) {
      b->p = vec_make(FX_FROM_INT(91), FX_FROM_INT(40));
      b->v = vec_make(0, 0);
      b->in_lane = false;
      for (uint8_t k = 0; k < BALL_TRAIL; k++) {
        b->trail[k] = b->p;
      }
    }
    prv_new_target();
    return;
  }
  int32_t dx = FX_TO_INT(b->p.x) - s_target_x;
  int32_t dy = FX_TO_INT(b->p.y) - s_target_y;
  bool inside = (dx * dx + dy * dy) <= (14 * 14);
  s_target_hit = inside;
  if (inside) {
    s_st.hold_cur_ms += dt_ms;
    s_st.hold_total_ms += dt_ms;
    if (s_st.hold_cur_ms > s_st.hold_best_ms) {
      s_st.hold_best_ms = s_st.hold_cur_ms;
    }
    if (s_st.hold_cur_ms >= 2000) {
      s_st.hold_targets++;
      haptics_pulse(LRA_FLIPPER_MS, HapEvent);
      APP_LOG(APP_LOG_LEVEL_INFO, "[P1][DRILL] Ziel %lu erreicht, Versuch %lu, bester Halt %lu ms",
              (unsigned long)s_st.hold_targets, (unsigned long)s_st.hold_attempts,
              (unsigned long)s_st.hold_best_ms);
      prv_new_target();
    }
  } else {
    s_st.hold_cur_ms = 0;
  }
  // Die Uebung soll nicht am Abfluss enden, sondern am Koennen: Wer die Kugel
  // verliert, bekommt sofort eine neue oben.
  if (b->p.y > FX_FROM_INT(MAG_DEAD_Y + 30)) {
    b->p = vec_make(FX_FROM_INT(91), FX_FROM_INT(40));
    b->v = vec_make(0, 0);
    prv_new_target();
  }
}

static void prv_events(void) {
  PhysEvents *e = &s_w->ev;
  if (e->drain) {
    s_st.drains += e->drain;
    haptics_pulse(LRA_DRAIN_MS, HapDrain);
    s_state = BallDrained;
    s_state_since = e1clock_now_ms();
    APP_LOG(APP_LOG_LEVEL_INFO, "[P1][BALL] Abfluss nach %lu ms Spielzeit", (unsigned long)s_st.play_ms);
  }
  if (e->bumper) {
    s_st.bumper += e->bumper;
    haptics_pulse(LRA_BUMPER_MS, HapEvent);
  }
  if (e->sling) {
    s_st.sling += e->sling;
    haptics_pulse(LRA_SLING_MS, HapTick);
  }
  if (e->flipper) {
    s_st.flipper += e->flipper;
  }
  s_st.wall += e->wall;
  s_st.gate += e->gate;
  memset(e, 0, sizeof(*e));
}

void game_plunge_button(bool held) {
  uint32_t now = e1clock_now_ms();
  if (held && !s_plunge_btn) {
    s_plunge_btn = true;
    s_plunge_btn_since = now;
    s_plunge_btn_ticks = 0;
  } else if (!held && s_plunge_btn) {
    s_plunge_btn = false;
    // Gleiche Ratsche wie bei der Geste: der Zugweg ergibt sich aus der
    // Haltedauer, damit die Fertigkeit uebertragbar bleibt.
    uint32_t held_ms = now - s_plunge_btn_since;
    int32_t pull = (int32_t)((held_ms * PLUNGER_PULL_MAX_PX) / 700);
    if (pull > PLUNGER_PULL_MAX_PX) {
      pull = PLUNGER_PULL_MAX_PX;
    }
    if (s_state == BallLane) {
      prv_launch((int16_t)pull, (uint8_t)(pull / PLUNGER_TICK_PX));
    }
  }
}

void game_tick(uint32_t now, uint32_t dt_ms) {
  // Ratsche der Zieh-Geste in Haptik uebersetzen
  uint8_t ticks = input_take_ratchet();
  for (uint8_t i = 0; i < ticks; i++) {
    haptics_pulse(LRA_RATCHET_MS, HapTick);
  }
  // Tasten-Plunger: Ratsche waehrend des Haltens
  if (s_plunge_btn) {
    uint32_t held_ms = now - s_plunge_btn_since;
    uint8_t want = (uint8_t)((held_ms * PLUNGER_TICKS_MAX) / 700);
    if (want > PLUNGER_TICKS_MAX) {
      want = PLUNGER_TICKS_MAX;
    }
    while (s_plunge_btn_ticks < want) {
      s_plunge_btn_ticks++;
      haptics_pulse(LRA_RATCHET_MS, HapTick);
    }
  }
  int16_t pull;
  uint8_t rel_ticks;
  if (input_take_plunger_release(&pull, &rel_ticks) && s_state == BallLane) {
    prv_launch(pull, rel_ticks);
  }

  // Neigung in die Schwerkraft, Stoesse auf die Kugeln
  fix gx, gy;
  nudge_gravity_offset(&gx, &gy);
  s_w->gravity = vec_make(gx, FX_FROM_INT(GRAVITY_PX_S2) + gy);
  fix dvx, dvy;
  if (nudge_take_impulse(&dvx, &dvy) && !nudge_is_tilted()) {
    phys_nudge(s_w, dvx, dvy);
  }
  if (nudge_is_tilted()) {
    // TILT: Flipper tot. Im Prototyp reicht das, die Anzeige uebernimmt das HUD.
    phys_set_flipper(s_w, 0, false);
    phys_set_flipper(s_w, 1, false);
  }

  prv_magnet(now, dt_ms);

  // Fester Zeitschritt mit Nachholen: Die Spielgeschwindigkeit haengt nicht an
  // der Bildrate. Mehr als PHYS_MAX_SUBSTEPS wird verworfen, sonst holt die
  // App nach einer langen Pause in Zeitlupe auf.
  s_phys_accum_ms += dt_ms;
  uint32_t steps = s_phys_accum_ms / PHYS_DT_MS;
  if (steps > PHYS_MAX_SUBSTEPS) {
    steps = PHYS_MAX_SUBSTEPS;
    s_phys_accum_ms = 0;
  } else {
    s_phys_accum_ms -= steps * PHYS_DT_MS;
  }
  for (uint32_t i = 0; i < steps; i++) {
    phys_substep(s_w);
  }
  phys_push_trail(s_w);
  prv_events();

  if (s_state == BallPlay) {
    s_st.play_ms += dt_ms;
  }
  if (s_drill) {
    prv_drill(now, dt_ms);
  } else if (s_state == BallDrained && (now - s_state_since) > 1200) {
    game_new_ball();
  }
  s_st.charge = (uint8_t)(s_charge_x100 / 100);
}

void game_fill_overlay(Overlay *ov) {
  memset(ov, 0, sizeof(*ov));
  int16_t fx, fy;
  ov->finger = input_finger(&fx, &fy);
  ov->fx = fx;
  ov->fy = fy;
  ov->charge_pct = s_st.charge;
  ov->grab = s_grab_btn;
  ov->show_tip = true;
  ov->show_deadline = true;
  ov->plunger_pull = input_plunger_active() ? input_plunger_pull_px() : 0;
  ov->plunger_ticks = input_plunger_ticks();
  const NudgeState *n = nudge_state();
  ov->tilt_warn = n->warn;
  ov->tilted = n->tilted;
  if (s_drill) {
    ov->target_x = s_target_x;
    ov->target_y = s_target_y;
    ov->target_r = 14;
    ov->target_hit = s_target_hit;
  }
}

BallState game_ball_state(void) {
  return s_state;
}

const GameStats *game_stats(void) {
  return &s_st;
}

// Stresstest: eine Kugel mit hohem Tempo quer durch den Tisch, viele Substeps
// am Stueck. Das misst die reine Physikzeit ohne Rendern und ohne Timer, in
// einer Aufloesung, die eine Millisekundenuhr sonst nicht hergibt.
void game_bench_physics(void) {
  World bench;
  phys_init(&bench);
  for (uint8_t i = 0; i < MAX_BALLS; i++) {
    Ball *b = phys_spawn_lane(&bench);
    if (!b) {
      break;
    }
    b->p = vec_make(FX_FROM_INT(40 + i * 40), FX_FROM_INT(40 + i * 20));
    b->v = vec_make(FX_FROM_INT(700 - i * 200), FX_FROM_INT(500 + i * 150));
    b->in_lane = false;
  }
  bench.table.flip[0].up = true;    // beide Flipper in Bewegung
  bench.table.flip[1].up = true;
  uint32_t t0 = e1clock_now_ms();
  for (uint32_t i = 0; i < PHYS_BENCH_STEPS; i++) {
    if ((i & 63) == 0) {
      bench.table.flip[0].up = !bench.table.flip[0].up;
      bench.table.flip[1].up = !bench.table.flip[1].up;
    }
    for (uint8_t k = 0; k < MAX_BALLS; k++) {
      if (!bench.ball[k].alive) {
        bench.ball[k].alive = true;
        bench.ball[k].p = vec_make(FX_FROM_INT(91), FX_FROM_INT(40));
        bench.ball[k].v = vec_make(FX_FROM_INT(600), FX_FROM_INT(400));
      }
    }
    phys_substep(&bench);
  }
  uint32_t t1 = e1clock_now_ms();
  uint32_t ms = t1 - t0;
  s_st.phys_us_per_substep_x10 = (ms * 10000) / PHYS_BENCH_STEPS;
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[P1][BENCH] %d Substeps, %lu Kugeln, %lu ms = %lu.%lu us/Substep",
          PHYS_BENCH_STEPS, (unsigned long)MAX_BALLS, (unsigned long)ms,
          (unsigned long)(s_st.phys_us_per_substep_x10 / 10),
          (unsigned long)(s_st.phys_us_per_substep_x10 % 10));
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[P1][BENCH] seg=%lu krs=%lu flp=%lu tests=%lu split=%lu esc=%lu",
          (unsigned long)bench.st.contacts_seg, (unsigned long)bench.st.contacts_circ,
          (unsigned long)bench.st.contacts_flip, (unsigned long)bench.st.narrow_tests,
          (unsigned long)bench.st.splits_max, (unsigned long)bench.st.escapes);
}
