#pragma once
#include <pebble.h>
#include "physics.h"
#include "render.h"
#include "view.h"

typedef enum {
  BallIdle = 0,     // keine Kugel im Spiel
  BallLane,         // liegt in der Abschussbahn
  BallPlay,         // rollt
  BallDrained,      // gerade abgeflossen
} BallState;

typedef struct {
  uint32_t balls;
  uint32_t drains;
  uint32_t bumper;
  uint32_t sling;
  uint32_t flipper;
  uint32_t wall;
  uint32_t gate;
  uint32_t skill_shots;
  uint32_t grabs;
  uint32_t throws;
  uint32_t plunges;

  uint32_t play_ms;         // Zeit mit rollender Kugel
  uint32_t mag_ms;          // davon mit wirkendem Magneten
  uint32_t occluded_ms;     // davon lag die Kugel unter der Fingerkuppe
  uint32_t finger_ms;       // Zeit mit liegendem Finger

  // Magnet-Uebung: die Antwort auf die Hauptkritik der Jury in Zahlen
  uint32_t hold_attempts;
  uint32_t hold_best_ms;
  uint32_t hold_total_ms;
  uint32_t hold_cur_ms;
  uint32_t hold_targets;    // erreichte Ziele

  uint8_t charge;
  uint32_t phys_us_per_substep_x10;   // aus dem Stresstest
} GameStats;

void game_init(World *w, View *v);
void game_deinit(void);
void game_set_magnet_drill(bool on);   // Uebungsmodus statt Flipperspiel
bool game_magnet_drill(void);
void game_set_grab(bool pressed);      // Select gehalten = Magnetgriff
void game_new_ball(void);
void game_plunge_button(bool held);    // Tasten-Ersatz fuer die Zieh-Geste
void game_tick(uint32_t now_ms, uint32_t dt_ms);
void game_fill_overlay(Overlay *ov);
BallState game_ball_state(void);
const GameStats *game_stats(void);
void game_reset_stats(void);
// Stresstest: viele Substeps am Stueck, misst die reine Physikzeit.
void game_bench_physics(void);
// Tempo: Stufe 0 bis SPEED_STEPS-1, skaliert Schwerkraft, Neigung und
// Magnetkraft gemeinsam.
void game_speed_next(void);
void game_set_speed_idx(uint8_t idx);
uint8_t game_speed_idx(void);
uint16_t game_speed_pct(void);
