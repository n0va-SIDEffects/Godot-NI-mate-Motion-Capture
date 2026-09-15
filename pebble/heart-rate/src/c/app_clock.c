#include <pebble.h>

#include "app_clock.h"
#include "beat_clock.h"

//! How far apart the app reads the clock at worst: the period of its slowest regular timer.
#define STEP_MS 40

static uint32_t s_previous_raw;
static uint32_t s_previous_out;
static bool s_started;

uint32_t clock_ms(void) {
  time_t seconds;
  uint16_t millis;
  time_ms(&seconds, &millis);
  const uint32_t raw = (uint32_t)seconds * 1000u + millis;
  if (!s_started) {
    s_started = true;
    s_previous_raw = raw;
    s_previous_out = raw;
    return raw;
  }
  const uint32_t out = beat_clock_filter_time(raw, s_previous_raw, s_previous_out, STEP_MS);
  s_previous_raw = raw;
  s_previous_out = out;
  return out;
}
