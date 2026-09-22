#pragma once
#include <pebble.h>
#include "config.h"

// Zwei Steuerprofile, die sich jederzeit gegeneinander tauschen lassen.
// Das ist Absicht: die Jury haelt den Fingerstick fuer schwaecher als den
// Tastenmodus, und diese Frage soll sich am Handgelenk entscheiden lassen,
// nicht am Schreibtisch.
//
// Profil A "Fingerstick": Finger irgendwo im Cockpitband aufsetzen, der
//   Versatz zum Aufsetzpunkt ist die Eingabe (x = Roll, y = Hoehe).
//   Dead Zone 6 px, Saettigung bei 40 px, quadratische Kurve.
//   Select gehalten = Praezisionsmodus, Up/Down trimmen die Hoehe.
// Profil B "Tasten" mit Flappy-Hoehenmodell: Up/Down = Roll, Select gehalten
//   = Steigen, losgelassen = Sinken.
typedef enum {
  CtrlFinger = 0,
  CtrlButton = 1,
} CtrlProfile;

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

void control_set_profile(CtrlProfile p);
CtrlProfile control_profile(void);
void control_toggle_profile(void);

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
const CtrlStats *control_stats(void);
