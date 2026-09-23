#pragma once
#include <pebble.h>
#include "config.h"

// Alles, was das Duell gegeneinander stellen soll, an einer Stelle und im
// Flash. Die Messung hat gezeigt, dass der Fingerstick im unteren Bilddrittel
// den Bodenschatten zu 77 Prozent der Zeit verdeckt; statt einen Nachfolger zu
// raten, sind hier alle Kandidaten waehlbar und einzeln messbar.

typedef enum {
  ProfFingerUnten = 0,   // Cockpitband im unteren Drittel (der durchgefallene Stand)
  ProfFingerRand  = 1,   // schmaler Streifen am Bildrand, Mitte bleibt frei
  ProfTilt        = 2,   // Neigungssensor, gar keine Hand auf dem Glas
  ProfTasten      = 3,   // Up/Down Roll, Select halten steigen (Flappy)
  // Aus der Messung entstanden: nicht die Verdeckung trennt die Profile,
  // sondern das Hoehenmodell. Dieses hier nimmt das Flappy-Modell der Tasten
  // und legt es auf den Finger - und weil dabei die Hochachse des Fingers
  // nichts mehr steuert, darf er ganz unten liegen bleiben, wo er nichts
  // verdeckt. Neue Werte gehoeren ans Ende, sonst verschieben sich die
  // gespeicherten Laeufe.
  ProfFingerFlappy = 4,  // Finger liegt = steigen, Versatz nach x = Roll
  ProfAnzahl      = 5,
} Profil;

typedef enum {
  SchattenNormal = 0,    // Kamera 32 Zellen zurueck, Schatten in Zeile 160..210
  SchattenHoch   = 1,    // Kamera weiter zurueck, Schatten ueber dem Cockpitband
  SchattenAnzahl = 2,
} Schattenlage;

typedef struct {
  uint8_t profil;        // Profil
  uint8_t schatten;      // Schattenlage
  uint8_t invert;        // Nicklage umgekehrt (nur die Finger-Profile)
  uint8_t rand_rechts;   // Randstreifen rechts statt links (nur ProfFingerRand)
  uint8_t licht;         // Backlight dauerhaft an
  // Kippt der Horizont mit der Kurve oder gegen sie? Die erste Fassung kippte
  // gegen die Kurve, was geometrisch falsch ist. Nach der Korrektur ist die
  // Sohlenzeit des Tastenprofils von 39/43 auf 11/3 Prozent gefallen - das
  // kann Umgewoehnung sein oder ein echter Nachteil. Solange das offen ist,
  // gehoert es umschaltbar und in die Reihe geschrieben, statt erraten.
  uint8_t horizont_alt;
} Setup;

void setup_init(void);
void setup_save(void);
const Setup *setup_get(void);

// Eine Zeile des Einstellungsbildschirms weiterschalten (0..SETUP_ZEILEN-1).
#define SETUP_ZEILEN 6
void setup_naechster_wert(int zeile);
// Backlight nach der Einstellung schalten. Getrennt von setup_naechster_wert,
// damit der Aufruf auch beim Start und beim Zurueckkommen aus dem Hintergrund
// passieren kann.
void setup_licht_anwenden(void);
void setup_text(int zeile, char *out, size_t n, bool markiert);

const char *setup_profil_name(uint8_t p);      // kurz, fuer die Duell-Tabelle
bool setup_profil_ist_touch(uint8_t p);
// Flappy heisst: die Hoehe kommt aus Halten und Loslassen, nicht aus einer
// Auslenkung. Genau das trennt in der Messung 43 Prozent Sohlenzeit von 4.
bool setup_profil_ist_flappy(uint8_t p);

// Aus der Schattenlage abgeleitete Geometrie. Beide Werte brauchen Renderer
// und Flugmodell, und sie muessen zusammenpassen, sonst steht der Schatten
// nicht mehr unter dem Gleiter.
int setup_cam_back_cells(void);
int setup_glider_row(void);
