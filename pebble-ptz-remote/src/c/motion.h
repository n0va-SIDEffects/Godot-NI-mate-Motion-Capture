#pragma once
#include "ptz.h"

// Bewegungssteuerung ueber den Lagesensor der Uhr.
//
// Gemessen wird die Neigung gegen die Schwerkraft, nicht die Drehrate. Das
// ist fuer eine Kamerafahrt das passende Signal: die Haltung des Handgelenks
// bestimmt die Geschwindigkeit, und wer die Hand still haelt, haelt auch die
// Kamera still. Eine reine Drehratenmessung wuerde mit der Zeit wegdriften.
//
// Der Nullpunkt ist immer die Haltung beim Beginn einer Fahrt, nicht eine
// feste Achse im Raum. Damit laesst sich die Kamera aus jeder bequemen
// Armhaltung heraus fahren.

void motion_start(void);
void motion_stop(void);

// Aktuelle Haltung als Nullpunkt uebernehmen.
void motion_set_reference(void);

// Sensor auslesen und glaetten. Gehoert in den Anzeigetakt.
void motion_update(void);

// Geschwindigkeiten aus der Neigung, jeweils -49..+49.
void motion_get_speeds(GyroAxes mode, uint8_t limit,
                       int8_t *pan, int8_t *tilt, int8_t *zoom);

// Ablenkung fuer die Anzeige, -100..+100 bezogen auf den Vollausschlag.
void motion_get_deflection(int8_t *x, int8_t *y);
