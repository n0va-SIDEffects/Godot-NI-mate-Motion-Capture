#pragma once
#include <pebble.h>

// Monotone Millisekunden-Uhr auf Basis von time_ms(), mit Filter gegen die
// beobachteten +-1000-ms-Spruenge an Sekundengrenzen.
void e1clock_init(void);
uint32_t e1clock_now_ms(void);
uint32_t e1clock_glitches(void);
