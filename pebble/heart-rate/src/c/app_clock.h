/*
 * The watch clock in milliseconds, with the firmware's one-second hiccup filtered out.
 * The filter itself lives in beat_clock.c, where it can be exercised on a host.
 */
#pragma once

#include <stdint.h>

//! Milliseconds since the epoch. Wraps every 49 days; differences stay valid.
uint32_t clock_ms(void);
