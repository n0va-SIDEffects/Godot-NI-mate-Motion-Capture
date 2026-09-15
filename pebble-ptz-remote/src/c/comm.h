#pragma once
#include "ptz.h"

// Verbindung zur Telefon-Komponente. Die Uhr schickt immer den *gewuenschten
// Zustand* aller Achsen, nicht einzelne Tastendruecke. Geht ein Paket
// verloren, korrigiert das naechste den Zustand wieder - fuer eine
// Fernsteuerung, bei der ein verschlucktes "Stopp" teuer waere, ist das die
// sichere Variante.
void comm_init(void);
void comm_deinit(void);

// Sollgeschwindigkeit setzen, jeweils -49..+49 (0 = Stillstand).
void comm_set_move(int8_t pan, int8_t tilt, int8_t zoom);
void comm_set_focus(int8_t focus);

// Alle Achsen anhalten. Wird sofort und doppelt gesendet.
void comm_stop_all(void);

// Einzelbefehl mit Zahlwert (Preset, Kamerawahl, Autofokus, ...).
void comm_send_command(PtzCommand cmd, int32_t value);

PtzStatus   comm_get_status(void);
const char *comm_get_message(void);
uint8_t     comm_get_cam_count(void);
const char *comm_get_cam_name(uint8_t index);
bool        comm_is_moving(void);

// Wird aufgerufen, wenn sich Status, Meldung oder Kameraliste geaendert haben.
typedef void (*CommUpdateHandler)(void);
void comm_set_update_handler(CommUpdateHandler handler);
