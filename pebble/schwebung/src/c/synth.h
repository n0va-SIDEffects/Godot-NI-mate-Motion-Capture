#pragma once
#include <pebble.h>

// Echtzeit-Synth: Stimmgabel (Sinus) plus eine Glasblume (Sinus), Glasklang
// beim Einrasten, Splitterrauschen beim Bruch. Die Schwebung entsteht
// physikalisch aus der Summe beider Sinus, nicht aus einem Naeherungsmesser.
void synth_init(void);
void synth_set_fork(int32_t chz, bool gate);          // gate: Finger liegt
void synth_set_flower(int32_t chz, uint8_t weight_pct); // Resonanzgewicht 0..100
void synth_set_octave_jump(bool on);
void synth_set_force_gate(bool on);                    // Latenztest: Gabel singt ohne Finger
void synth_set_flat(bool on);                          // Klick-Diagnose: nackter Sinus, keine Huellkurve
void synth_render_loop_block(int16_t *out);            // Klick-Diagnose: 500 Hz, blockgenau periodisch
void synth_ping(int32_t chz);                          // Glasklang (3 unharmonische Teiltoene)
void synth_crack(void);                                // Zersplittern
void synth_render(int16_t *out, uint32_t num_samples);
