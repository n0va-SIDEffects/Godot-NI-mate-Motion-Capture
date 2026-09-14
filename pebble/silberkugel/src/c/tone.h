#pragma once
#include <pebble.h>

// Phase 1 macht keine Spielmusik. Dieser Generator liefert nur einen Ton als
// Last, damit die Messung ehrlich ist: Der Tonnachschub ist laut Hardware-
// Befund das knappste Gut der Uhr, und die Frage lautet, ob Physik, Rendern
// und Ton zusammen durchhalten. Phasenakkumulator plus Sinustabelle, wie in
// pebble/schwebung.
void tone_init(void);
void tone_set_hz(uint16_t hz);
void tone_render(int16_t *out, uint32_t num_samples);
void tone_render_silence(int16_t *out, uint32_t num_samples);
