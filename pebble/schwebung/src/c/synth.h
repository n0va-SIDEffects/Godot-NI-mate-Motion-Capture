#pragma once
#include <pebble.h>

// Echtzeit-Synth: Stimmgabel (Sinus) plus eine Glasblume (Sinus), Glasklang
// beim Einrasten, Splitterrauschen beim Bruch. Die Schwebung entsteht
// physikalisch aus der Summe beider Sinus, nicht aus einem Naeherungsmesser.
void synth_init(void);
void synth_set_fork(int32_t chz, bool gate);          // gate: Finger liegt
void synth_set_flower(int32_t chz, uint8_t weight_pct); // Resonanzgewicht 0..100
void synth_set_octave_jump(bool on);                   // Latenztest: Gabel eine Oktave hoch
void synth_ping(int32_t chz);                          // Glasklang (3 unharmonische Teiltoene)
void synth_crack(void);                                // Zersplittern
void synth_render(int16_t *out, uint32_t num_samples);
