#pragma once
#include <pebble.h>
#include "fixed.h"

// Beschleunigungssensor in drei Rollen, aus einem Datenstrom (50 Hz, Batch 2):
//   Hochpass   = Stoss gegen das Gehaeuse (Nudge)
//   Tiefpass   = Neigung des Tisches, verschiebt die Schwerkraft
//   Tap-Dienst = Klaps auf die Gehaeuseseite, kraeftiger gerichteter Stoss
// Dazu die Vibrationsmaske: Jeder eigene LRA-Impuls sieht im Sensor aus wie
// ein Stoss. Ohne Maske wuerde der Flipper sich selbst anstossen.

typedef struct {
  uint32_t samples;
  uint32_t nudges;
  uint32_t taps;
  uint32_t masked_vib;      // wegen eigener Vibration verworfen
  uint32_t masked_flag;     // wegen did_vibrate der Firmware verworfen
  uint32_t masked_button;   // kurz nach einem Tastendruck verworfen
  int16_t peak_hp_mg;       // groesster Hochpassbetrag seit dem Start
  int16_t hp_x_mg;
  int16_t hp_y_mg;
  int16_t lean_x_mg;
  int16_t lean_y_mg;
  int32_t bob;              // Tilt-Bob
  bool warn;
  bool tilted;
  uint32_t batches;
  uint32_t batch_max;       // groesste gelieferte Stapelgroesse
} NudgeState;

void nudge_init(void);
void nudge_deinit(void);
void nudge_calibrate(void);          // Neutrallage neu mitteln (nach dem Abschuss)
void nudge_button_mask(void);        // Tastendruck: kurz keine Stossauswertung
void nudge_tick(uint32_t now_ms);    // Tilt-Bob abklingen lassen
void nudge_reset_tilt(void);

// Aufgelaufenen Stoss abholen (px/s, Tischkoordinaten) und Zaehler nullen.
bool nudge_take_impulse(fix *dvx, fix *dvy);
// Schwerkraftverschiebung durch Neigung (px/s^2, Tischkoordinaten)
void nudge_gravity_offset(fix *gx, fix *gy);
bool nudge_is_tilted(void);
const NudgeState *nudge_state(void);
