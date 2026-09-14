#pragma once
#include <pebble.h>
#include "fixed.h"
#include "table.h"
#include "config.h"

#define BALL_TRAIL 3

typedef struct {
  Vec p;
  Vec v;
  bool alive;
  bool held;                  // vom Magnetfinger gefangen: folgt dem Finger statt der Physik
  bool in_lane;               // liegt noch in der Plunger-Bahn
  uint32_t bumper_cd_ms;
  Vec trail[BALL_TRAIL];      // letzte Positionen, fuer die Bewegungsspur
  uint8_t trail_n;
} Ball;

// Was in einem Tick passiert ist. Das Spiel liest es aus und macht Haptik,
// Licht und Punkte daraus; die Physik selbst kennt weder LRA noch Score.
typedef struct {
  uint8_t bumper;
  uint8_t sling;
  uint8_t flipper;
  uint8_t wall;
  uint8_t drain;
  uint8_t gate;
} PhysEvents;

typedef struct {
  uint32_t substeps;
  uint32_t contacts_seg;
  uint32_t contacts_circ;
  uint32_t contacts_flip;
  uint32_t splits_max;        // meiste Teilschritte innerhalb eines Substeps
  uint32_t escapes;           // Kugel ausserhalb des Tisches gefunden: Tunneling-Verdacht
  uint32_t narrow_tests;      // Kollisionstests gesamt, fuer die Kostenrechnung
  fix speed_max;
} PhysStats;

typedef struct {
  Table table;
  Ball ball[MAX_BALLS];
  uint8_t ball_count;

  Vec gravity;                // wirkende Schwerkraft in px/s^2, inklusive Tischneigung
  bool mag_on;                // Magnetfinger aktiv
  Vec mag_pos;
  fix mag_accel;              // Spitzenbeschleunigung, aus der Ladung abgeleitet

  PhysEvents ev;
  PhysStats st;
} World;

void phys_init(World *w);
void phys_reset_stats(World *w);

// Fester Zeitschritt. Der Aufrufer ruft das so oft, wie Zeit vergangen ist
// (Catch-up), die Physik selbst kennt keine variable Schrittweite.
void phys_substep(World *w);

// Kugelverwaltung
Ball *phys_spawn_lane(World *w);     // neue Kugel in die Plunger-Bahn, NULL wenn voll
void phys_clear_balls(World *w);
uint8_t phys_alive_count(const World *w);

// Flipperzustand setzen (Taste gedrueckt / losgelassen)
void phys_set_flipper(World *w, uint8_t idx, bool up);

// Stoss auf alle Kugeln (Nudge, Klaps)
void phys_nudge(World *w, fix dvx, fix dvy);

// Spur der Kugeln fortschreiben (einmal pro Bild)
void phys_push_trail(World *w);

// Spitze des Flippers, fuer den Renderer
Vec phys_flipper_tip(const Flipper *f);
