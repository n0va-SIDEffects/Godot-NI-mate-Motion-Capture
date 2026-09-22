#pragma once
#include <pebble.h>
#include "config.h"
#include "setup.h"

// Vier Steuerprofile, jederzeit tauschbar, alle vom DUELL messbar. Welches
// gewinnt, entscheidet die Messung und nicht der Geschmack: der Fingerstick im
// unteren Bilddrittel hat auf der Uhr 77 Prozent Verdeckung des Bodenschattens
// erreicht, und die Umkehr der Nicklage hat daran nichts geaendert.
//
//   ProfFingerUnten  Cockpitband unten, Versatz zum Aufsetzpunkt ist die
//                    Eingabe (x = Roll, y = Hoehe). Der gemessene Stand.
//   ProfFingerRand   dasselbe, aber nur ein schmaler Streifen am Bildrand
//                    nimmt den Finger an; die Bildmitte bleibt frei.
//   ProfTilt         Neigungssensor, gar keine Hand auf dem Glas.
//   ProfTasten       Up/Down Roll, Select halten steigen (Flappy-Hoehenmodell).
//
// Das Profil steht in setup.h und wird dort auch im Flash gehalten.
typedef struct {
  int32_t roll_cmd;          // -256..256, Rollkommando
  int32_t climb_cmd;         // -256..256, Steigkommando (Profil A)
  bool climb_held;           // Select liegt (Profil B)
  bool precision;            // Select liegt (Profil A)
  int32_t trim8;             // Hoehentrimmung in 24.8 Zellen
} CtrlOut;

typedef struct {
  uint32_t events;
  uint32_t touchdowns;
  uint32_t ev_rate_x10;
  uint32_t min_interval_ms;
  int16_t dx, dy;            // aktueller Versatz zum Aufsetzpunkt
  int16_t abs_x, abs_y;      // absolute Fingerposition, fuer die Verdeckungsmessung
  bool down;
  bool available;
} CtrlStats;

void control_init(Window *window);
void control_deinit(void);
void control_tick(uint32_t now_ms);

void control_set_profile(uint8_t p);
uint8_t control_profile(void);
void control_toggle_profile(void);
// Neutrallage des Neigungssensors auf die aktuelle Haltung setzen. Wird beim
// Betreten des Flugs und beim Start eines Duell-Laufs gerufen, damit niemand
// eine Kalibriertaste suchen muss.
void control_tilt_kalibrieren(void);

// Die Kritik der Jury: beim Steigen zieht die Fingerkuppe aus dem Cockpitband
// bis an die Horizontlinie und verdeckt den Bodenschatten. Umgekehrtes
// Vorzeichen zieht den Finger zum Steigen nach unten, wie ein echter Knueppel.
void control_set_pitch_invert(bool on);
bool control_pitch_invert(void);

void control_button_up(bool pressed);
void control_button_down(bool pressed);
void control_button_select(bool pressed);
void control_trim(int32_t delta8);
int32_t control_trim8(void);

const CtrlOut *control_out(void);
bool control_touch_in_zone(int16_t x);   // liegt x in der aktiven Touch-Zone?
const CtrlStats *control_stats(void);
