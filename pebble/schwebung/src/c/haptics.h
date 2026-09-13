#pragma once
#include <pebble.h>

// LRA-Vokabular. Es gibt keinen Fertig-Callback fuer Vibrationen und kein
// Einreihen waehrend ein Muster laeuft. Deshalb: ein Muster pro Rate, eigener
// Timer fuer das Musterende, Wechsel nur an Impulsgrenzen per vibes_cancel.
void haptics_init(void);
void haptics_deinit(void);
void haptics_set_beat(int32_t beat_chz, bool active);  // Schwebungsrate
void haptics_warn(uint32_t ms_left);                    // Bruchwarnung, Pausen schrumpfen
void haptics_lock(void);                                // Doppelpuls
void haptics_break(void);                               // langer Puls
void haptics_test_rate(uint8_t hz);                     // LRA-Test: exakte Rate, jeder Aufruf geloggt
void haptics_stop(void);
uint32_t haptics_call_count(void);
const char *haptics_mode_name(void);
