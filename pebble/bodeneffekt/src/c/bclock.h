#pragma once
#include <pebble.h>

// Monotone Millisekunden-Uhr auf Basis von time_ms(), mit Filter gegen die
// beobachteten +-1000-ms-Spruenge an Sekundengrenzen.
void bclock_init(void);
uint32_t bclock_now_ms(void);
uint32_t bclock_glitches(void);
// Zahl der Neusynchronisationen (Versatz hielt > 1,5 s an, z. B. Pause oder Zeitsync).
uint32_t bclock_resyncs(void);
// Zahl der Aufrufpausen ueber 1,4 s (App-Task stand); ohne Sonderbehandlung
// wuerde die Modulo-Rechnung dabei ganze Sekunden verlieren.
uint32_t bclock_long_gaps(void);
