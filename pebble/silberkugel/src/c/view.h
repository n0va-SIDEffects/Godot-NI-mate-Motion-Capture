#pragma once
#include <pebble.h>
#include "fixed.h"
#include "table.h"

// Die Ansicht sitzt zwischen Tisch und Bildschirm und macht zwei Dinge:
//
//   Drehung: Im Hochformat laeuft die Tischachse senkrecht ueber den
//   Bildschirm, im Querformat liegt sie waagerecht. Gedreht gehalten wird die
//   Uhr zum kleinen Automaten, den man in beide Haende nimmt; der Tisch ist
//   dann 228 statt 200 Pixel breit, dafuer sieht man weniger von seiner Laenge.
//
//   Kamera: Ist der Tisch laenger als der Ausschnitt, folgt sie der untersten
//   Kugel, aber nur wenn diese ein Totband verlaesst, und nur in ganzen
//   Pixeln. Ohne beides zittert das Bild bei jeder kleinen Bewegung.
//
// Alles, was Tischkoordinaten in Bildschirmkoordinaten uebersetzt, geht hier
// durch: der Renderer, der Finger auf dem Glas und die Achsen des
// Beschleunigungssensors.

typedef struct {
  bool rot90;          // Tisch quer statt hoch
  bool camera;         // Kamera darf scrollen
  int16_t cam;         // Versatz entlang der Tischlaenge, in ganzen Pixeln
  int16_t cam_max;     // groesster zulaessiger Versatz
  int16_t view_len;    // sichtbare Tischlaenge in Pixeln
  int16_t view_wid;    // sichtbare Tischbreite in Pixeln
  int16_t wid_off;     // Rand, wenn die Tischbreite schmaler ist als der Platz
} View;

void view_init(View *v, const Table *t);
void view_set_rot(View *v, bool rot90, const Table *t);
void view_set_camera(View *v, bool on, const Table *t);
// Kamera nachfuehren. ball_y ist die Tischhoehe der untersten lebenden Kugel;
// bei -1 bleibt die Kamera stehen.
void view_track(View *v, int16_t ball_y);
// Kamera sofort auf die Kugel setzen, ohne Schwenk. Fuer den Ballwechsel:
// Dort ist ein Sprung richtig, weil die Szene wechselt; im Spiel waere er ein
// Ruckeln.
void view_snap(View *v, int16_t ball_y);

// Tisch nach Bildschirm und zurueck. Beide arbeiten in ganzen Pixeln, weil das
// Ziel ohnehin ein Pixelraster ist.
void view_to_screen(const View *v, int16_t tx, int16_t ty, int16_t *sx, int16_t *sy);
void view_to_table(const View *v, int16_t sx, int16_t sy, int16_t *tx, int16_t *ty);
// Dreht eine Richtung mit (Stoss aus dem Beschleunigungssensor, Fingerwurf).
void view_dir_to_table(const View *v, fix dx, fix dy, fix *tx, fix *ty);
// Liegt der Punkt im sichtbaren Ausschnitt?
bool view_visible(const View *v, int16_t tx, int16_t ty);
