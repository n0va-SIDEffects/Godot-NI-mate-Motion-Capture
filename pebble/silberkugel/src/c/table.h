#pragma once
#include <pebble.h>
#include "fixed.h"

// Grauer Testtisch: Wandsegmente, Kreise, zwei Flipperdrehpunkte, Plunger-Bahn,
// zwei Outlanes. Die Geometrie steht als int16-Pixelliste im Flash und wird
// beim Start einmal in Fixed-Point uebersetzt; im Konzept kommt sie spaeter aus
// demselben Blender-File wie die Grafik.

typedef enum {
  SegWall = 0,      // normale Bande
  SegSling,         // Slingshot-Gummi: mehr Abprall plus fester Stoss
  SegGate,          // Einwegtor: sperrt nur von einer Seite
} SegKind;

typedef struct {
  Vec a;
  Vec b;
  Vec n;            // Einheitsnormale, zeigt zur Spielfeldseite
  fix len;
  Vec dir;          // Einheitsvektor von a nach b
  uint8_t kind;
  uint8_t rest_pct;
} Segment;

typedef enum {
  CircBumper = 0,
  CircPost,
} CircleKind;

typedef struct {
  Vec c;
  fix r;
  uint8_t kind;
  uint8_t rest_pct;
  int16_t cx_px;
  int16_t cy_px;
  int16_t r_px;
} Circle;

typedef struct {
  Vec pivot;
  fix len;
  fix radius;
  int32_t angle_rest;     // Pebble-Trigonometrieeinheiten
  int32_t angle_active;
  int32_t angle;          // aktueller Winkel
  int32_t angle_prev;     // Winkel vor dem letzten Substep (fuer die Kontaktgeschwindigkeit)
  bool up;                // Taste gedrueckt
  bool left;              // linker Flipper (nur fuer Anzeige und Log)
  int16_t pivot_x_px;
  int16_t pivot_y_px;
} Flipper;

#define TABLE_MAX_SEGMENTS 40
#define TABLE_MAX_CIRCLES 5

typedef struct {
  Segment seg[TABLE_MAX_SEGMENTS];
  uint8_t seg_count;
  Circle circ[TABLE_MAX_CIRCLES];
  uint8_t circ_count;
  Flipper flip[2];        // 0 = links, 1 = rechts
  int16_t height_px;      // Tischlaenge: 228 ohne Streckung, mehr mit
  int16_t drain_y;        // ab hier ist die Kugel weg
  int16_t mag_dead_y;     // unterhalb wirkt der Magnet nicht
  int16_t plunger_y;      // Ruhelage der Kugel in der Abschussbahn
  int16_t stretch;        // eingefuegtes gerades Stueck
} Table;

// Baut den Tisch. stretch_px schiebt alles unterhalb der Pfosten um diesen
// Betrag nach unten und verlaengert dabei die Seitenwaende: So entsteht aus
// derselben Geometrie ein laengerer Tisch, den die Kamera abfahren muss, ohne
// dass eine zweite Geometrie gepflegt werden will.
void table_build(Table *t, int16_t stretch_px);

// Startpunkt der Kugel in der Plunger-Bahn
Vec table_plunger_pos(const Table *t);

// Kleinste Abschussgeschwindigkeit, mit der die Kugel oben aus der Bahn
// kommt, in px/s. Sie folgt aus sqrt(2*g*h) mit der tatsaechlichen Bahnhoehe,
// plus Reserve: Ein fester Wert waere bei jeder Aenderung der Tischlaenge oder
// des Grundtempos wieder falsch, und der Spieler saesse fest.
int16_t table_plunger_min_speed(const Table *t, int32_t gravity_px_s2);
