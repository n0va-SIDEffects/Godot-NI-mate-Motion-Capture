#pragma once
#include <pebble.h>

// LRA-Vokabular ohne vibes_cancel im Normalbetrieb: Jeder Impuls wird einzeln
// eingereiht, der naechste per Timer geplant. Ratenwechsel greifen beim
// naechsten Impuls, ein Stopp laesst den laufenden Impuls einfach ausklingen.
// Grund: vibes_cancel kann den App-Task 10 bis 80 ms blockieren und damit den
// Audio-Vorlauf leerlaufen lassen. Muster koennen nicht eingereiht werden,
// solange eines laeuft, und es gibt keinen Fertig-Callback.
void haptics_init(void);
void haptics_deinit(void);
void haptics_set_beat(int32_t beat_chz, bool active);  // Schwebungsrate (nur Spielbetrieb)
void haptics_warn(uint32_t ms_left);                    // Bruchwarnung, Pausen schrumpfen
void haptics_lock(void);                                // Doppelpuls, ggf. verzoegert bis der Motor frei ist
void haptics_break(void);                               // langer Puls, ebenso
bool haptics_marker(void);                              // Einzelimpuls fuer den Latenztest, false wenn Motor belegt
void haptics_test_rate(uint8_t hz);                     // LRA-Test: exakte Rate, jeder Impuls geloggt, Spiel gesperrt
void haptics_stop(void);                                // alles aus, hebt den Testmodus auf
uint32_t haptics_call_count(void);
const char *haptics_mode_name(void);
