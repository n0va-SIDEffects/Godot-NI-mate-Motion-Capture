#pragma once
#include <pebble.h>
#include "config.h"
#include "control.h"

// Der Duell-Lauf: dieselbe Strecke, dieselbe Startlage, dieselbe Dauer, einmal
// je Steuerprofil. Er beantwortet nicht die Frage "was fuehlt sich besser an",
// sondern die konkrete Kritik der Jury: verdeckt der Finger beim Steigen den
// Bodenschatten, also die wichtigste Information des Spiels?
//
// Vier Zahlen je Lauf:
//   Sohle    Anteil der Zeit im Bodeneffekt-Fenster (2 bis 12 Zellen). Das ist
//            die Kernmechanik: wer sie nicht haelt, laedt keinen Boost.
//   Boden    Bodenkontakte. Die Kosten des Tiefflugs.
//   Hoehe    mittlere Hoehe ueber Grund. Zeigt, ob ein Profil mutiger macht.
//   Blind    Anteil der Zeit, in der der Schatten im Bild war, aber unter der
//            Hand lag. Bei den Tasten ist dieser Wert bauartbedingt null.
typedef struct {
  uint32_t seed;
  uint32_t dauer_ms;
  uint32_t sohle_ms;          // im Bodeneffekt-Fenster
  uint32_t schatten_ms;       // Schatten war im Bild (Bezug fuer blind_ms)
  uint32_t blind_ms;          // davon vom Finger verdeckt
  uint32_t agl_sum8;          // Summe der Hoehen ueber Grund (24.8) fuer den Mittelwert
  uint32_t proben;
  uint16_t kontakte;
  uint8_t profil;             // Profil aus setup.h
  uint8_t invert;             // Nicklage umgekehrt (nur die Finger- und Tilt-Profile)
  uint8_t schatten;           // Schattenlage, mit der der Lauf geflogen wurde
  bool gueltig;
} DuellLauf;

void duell_init(void);
void duell_start(void);                 // beginnt einen Lauf mit dem aktuellen Profil
void duell_abort(void);
bool duell_aktiv(void);
uint32_t duell_rest_ms(void);
// Einmal je Spiel-Tick waehrend eines Laufs. Liefert true, wenn der Lauf in
// diesem Tick zu Ende ging.
bool duell_tick(uint32_t dt_ms, int32_t agl8, bool kontakt, bool in_effekt,
                int schatten_zeile, int schatten_hw, const CtrlStats *touch);
const DuellLauf *duell_ergebnis(uint8_t profil);
