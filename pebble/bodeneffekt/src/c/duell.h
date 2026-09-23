#pragma once
#include <pebble.h>
#include "config.h"
#include "control.h"

// Der Duell-Lauf: dieselbe Strecke, dieselbe Startlage, dieselbe Dauer, einmal
// je Steuerprofil. Er beantwortet nicht die Frage "was fuehlt sich besser an",
// sondern die konkrete Kritik der Jury: verdeckt der Finger beim Steigen den
// Bodenschatten, also die wichtigste Information des Spiels?
//
// Vier Zahlen, gemittelt ueber alle Laeufe eines Profils:
//   Sohle    Anteil der Zeit im Bodeneffekt-Fenster (2 bis 12 Zellen). Das ist
//            die Kernmechanik: wer sie nicht haelt, laedt keinen Boost.
//   Boden    Bodenkontakte je Lauf. Die Kosten des Tiefflugs.
//   Hoehe    mittlere Hoehe ueber Grund. Zeigt, ob ein Profil mutiger macht.
//   Blind    Anteil der Zeit, in der der Schatten im Bild war, aber unter der
//            Hand lag. Bei den tastengesteuerten Profilen ohne Aussage.
//
// Gesammelt wird ueber MEHRERE Laeufe, und das ist keine Bequemlichkeit: beim
// selben Profil auf derselben Strecke sind 11, 39 und 43 Prozent Sohlenzeit
// gemessen worden. Die Streuung eines Profils ist groesser als der Abstand
// zwischen den Profilen, ein einzelner Lauf traegt also keine Aussage. Neben
// dem Mittelwert steht deshalb die Zahl der Laeufe und die Spanne.
typedef struct {
  uint32_t seed;
  uint32_t dauer_ms;          // Summen ueber alle Laeufe der Reihe
  uint32_t sohle_ms;
  uint32_t schatten_ms;
  uint32_t blind_ms;
  uint32_t agl_sum8;
  uint32_t proben;
  uint32_t kontakte;
  uint8_t laeufe;             // Zahl der gewerteten Laeufe
  uint8_t sohle_min;          // Prozent, kleinster und groesster Einzellauf
  uint8_t sohle_max;
  uint8_t profil;
  uint8_t invert;
  uint8_t schatten;
  uint8_t horizont_alt;
} DuellReihe;

// Ein Lauf hat drei Phasen: Vorlauf (Countdown, es wird nichts gewertet und
// nicht geflogen), Wertung, Ende.
typedef enum {
  DuellAus = 0,
  DuellVorlauf,
  DuellWertung,
  DuellZuende,        // nur im Tick des Uebergangs
  DuellStart,         // nur im Tick, in dem die Wertung beginnt
} DuellPhase;

void duell_init(void);
void duell_start(void);                 // beginnt einen Lauf mit dem aktuellen Profil
void duell_abort(void);
bool duell_aktiv(void);
uint32_t duell_rest_ms(void);
uint32_t duell_vorlauf_ms(void);
DuellPhase duell_tick(uint32_t dt_ms, int32_t agl8, bool kontakt, bool in_effekt,
                      int schatten_zeile, int schatten_hw, const CtrlStats *touch);
const DuellReihe *duell_ergebnis(uint8_t profil);
// Die Reihe eines Profils verwerfen und neu beginnen.
void duell_reihe_loeschen(uint8_t profil);
