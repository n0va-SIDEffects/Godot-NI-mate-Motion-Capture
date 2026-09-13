/*
 * Beat clock: turns a measured beat interval into beats placed on a wall clock.
 *
 * The trace advances one pixel per fixed slice of time, and a beat can only start on such a
 * pixel. Callers drive it from their render loop:
 *
 *     uint32_t steps = beat_clock_begin(&clock, now_ms());
 *     for (uint32_t i = 0; i < steps; i++) {
 *       bool audible;
 *       if (beat_clock_step(&clock, &audible)) { ... start a beat ... }
 *       ... advance the trace by one pixel ...
 *     }
 *
 * Everything is integer arithmetic on millisecond timestamps that may wrap, and there are no
 * Pebble dependencies, so the logic can be exercised by tools/test_beat_clock.c on a host.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t px_ms;         // milliseconds per pixel, and therefore per step
  uint32_t catchup_px;    // most pixels replayed in one pass; older timeline is skipped
  uint32_t audible_ms;    // a beat older than this is reported as inaudible
  uint32_t interval_ms;   // current beat interval, 0 while no rate is known
  uint32_t last_px_ms;    // wall clock the caller has drawn up to
  uint32_t last_beat_ms;  // wall clock of the last beat
  uint32_t now_ms;        // wall clock captured by the current begin()
} BeatClock;

//! Start the clock at the given time. px_ms and catchup_px must be non-zero.
void beat_clock_init(BeatClock *clock, uint32_t now_ms, uint32_t px_ms, uint32_t catchup_px,
                     uint32_t audible_ms);

//! Adopt a new beat interval (0 stops the beats). The phase stays with the clock: the next beat
//! falls one new interval after the last one, so a changed rate takes effect immediately without
//! inserting an extra beat. Starting from stopped beats on the next step.
void beat_clock_set_interval(BeatClock *clock, uint32_t interval_ms, uint32_t now_ms);

//! Begin a pass and return how many steps to take to reach now.
uint32_t beat_clock_begin(BeatClock *clock, uint32_t now_ms);

//! Take one step. Returns true if a beat starts on this pixel, and then sets *audible to false
//! for a beat the caller is only catching up on, which should be drawn but not sounded.
bool beat_clock_step(BeatClock *clock, bool *audible);
