#pragma once
#include "ptz.h"

// Einstellungen: teils von der Konfigurationsseite (Clay), teils an der Uhr
// gesetzt. Beides wird lokal gespeichert, damit die App auch ohne Telefon
// sofort brauchbar startet.
typedef struct {
  uint8_t speed;          // 1..5, Anteil der maximalen Kamerageschwindigkeit
  uint8_t gyro_sens;      // 1..10, wie stark die Neigung wirkt
  uint8_t gyro_dead;      // Totzone in Milli-g (20..200)
  bool    invert_pan;
  bool    invert_tilt;
  bool    vibrate;        // Rueckmeldung bei Fehler und Preset
  uint8_t preset_count;   // 1..PTZ_MAX_PRESETS
  uint8_t gyro_axes;      // Startwert fuer die Neigungssteuerung
  uint8_t active_cam;     // zuletzt gewaehlte Kamera
  uint8_t active_axis;    // zuletzt gewaehlte Achse der Tastenansicht
} PtzSettings;

void settings_load(void);
void settings_save(void);
PtzSettings *settings_get(void);

// Nimmt die Werte der Konfigurationsseite aus einer AppMessage entgegen.
// Gibt true zurueck, wenn mindestens ein Wert enthalten war.
bool settings_apply_message(DictionaryIterator *iter);

// Maximalausschlag in Panasonic-Schritten fuer die eingestellte Stufe.
uint8_t settings_speed_limit(void);
