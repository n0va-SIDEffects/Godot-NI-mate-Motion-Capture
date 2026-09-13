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
  uint32_t min_gap_ms;    // no two beats may fall closer together than this
  uint32_t interval_ms;   // current beat interval, 0 while no rate is known
  uint32_t last_px_ms;    // wall clock the caller has drawn up to
  uint32_t last_beat_ms;  // wall clock of the last beat
  uint32_t now_ms;        // wall clock captured by the current begin()
} BeatClock;

//! Start the clock at the given time. px_ms and catchup_px must be non-zero.
//!
//! min_gap_ms is a floor on the distance between two beats. A heart cannot beat twice within it,
//! and neither can a beep be sounded twice, so a beat that would fall sooner waits. Without it a
//! rate arriving after a pause beat at once however recently the last beat had been, which cut the
//! previous beep short and was heard as a click.
void beat_clock_init(BeatClock *clock, uint32_t now_ms, uint32_t px_ms, uint32_t catchup_px,
                     uint32_t audible_ms, uint32_t min_gap_ms);

//! Adopt a new beat interval (0 stops the beats). The phase stays with the clock: the next beat
//! falls one new interval after the last one, so a changed rate takes effect immediately without
//! inserting an extra beat. Starting from stopped beats on the next step.
void beat_clock_set_interval(BeatClock *clock, uint32_t interval_ms, uint32_t now_ms);

//! Begin a pass and return how many steps to take to reach now.
uint32_t beat_clock_begin(BeatClock *clock, uint32_t now_ms);

//! Take one step. Returns true if a beat starts on this pixel, and then sets *audible to false
//! for a beat the caller is only catching up on, which should be drawn but not sounded.
bool beat_clock_step(BeatClock *clock, bool *audible);

//! Does a freshly measured interval agree with a reference interval, give or take a percentage?
//!
//! An optical sensor can lock onto the wrong peak of the pulse wave and report half the true
//! interval, which would double the pulse. Measuring it against the averaged rate, which is
//! smoothed and does not make that mistake, rejects such a reading. A reference of 0 means there
//! is nothing to compare against yet, and the interval is accepted.
bool beat_clock_interval_plausible(uint32_t interval_ms, uint32_t reference_ms,
                                   uint32_t tolerance_pct);

//! Middle of three values. Used on consecutive measured intervals so that a single reading which
//! passed the plausibility check but still sits well off the others cannot pull the rhythm along.
uint32_t beat_clock_median3(uint32_t a, uint32_t b, uint32_t c);

typedef enum {
  BeatSourceNone,      //! no usable rate at all
  BeatSourceMeasured,  //! a measured interval, fresh enough to be trusted
  BeatSourceRate,      //! the averaged rate, because no fresh measured interval is left
} BeatSource;

//! Choose what drives the beats and write the interval to *interval_ms.
//!
//! Measured intervals win while they keep arriving. They stop arriving whenever the sensor loses
//! the pulse, and they are dropped by the caller whenever they disagree with the rate, so the
//! averaged rate has to take over on its own after measured_timeout_ms.
BeatSource beat_clock_select(uint32_t measured_ms, uint32_t measured_age_ms,
                             uint32_t measured_timeout_ms, uint32_t rate_ms,
                             uint32_t *interval_ms);
