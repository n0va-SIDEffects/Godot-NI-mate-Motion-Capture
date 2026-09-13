#include "beat_clock.h"

void beat_clock_init(BeatClock *clock, uint32_t now_ms, uint32_t px_ms, uint32_t catchup_px,
                     uint32_t audible_ms) {
  clock->px_ms = px_ms;
  clock->catchup_px = catchup_px;
  clock->audible_ms = audible_ms;
  clock->interval_ms = 0;
  clock->last_px_ms = now_ms;
  clock->last_beat_ms = now_ms;
  clock->now_ms = now_ms;
}

void beat_clock_set_interval(BeatClock *clock, uint32_t interval_ms, uint32_t now_ms) {
  if (interval_ms == clock->interval_ms) {
    return;
  }
  const bool was_stopped = (clock->interval_ms == 0);
  clock->interval_ms = interval_ms;
  if (was_stopped && interval_ms > 0) {
    clock->last_beat_ms = now_ms - interval_ms;   // due immediately, so beating starts at once
  }
}

uint32_t beat_clock_begin(BeatClock *clock, uint32_t now_ms) {
  clock->now_ms = now_ms;
  const int32_t elapsed = (int32_t)(now_ms - clock->last_px_ms);
  if (elapsed <= 0) {
    return 0;
  }
  uint32_t steps = (uint32_t)elapsed / clock->px_ms;
  if (steps > clock->catchup_px) {
    // More than the caller can show is behind us: skip the stale timeline rather than replay it,
    // and carry the beat phase forward so the first step does not fire a pile-up of beats.
    steps = clock->catchup_px;
    clock->last_px_ms = now_ms - steps * clock->px_ms;
    if (clock->interval_ms > 0) {
      clock->last_beat_ms = clock->last_px_ms - clock->interval_ms;
    }
  }
  return steps;
}

bool beat_clock_step(BeatClock *clock, bool *audible) {
  clock->last_px_ms += clock->px_ms;
  if (clock->interval_ms == 0 ||
      (int32_t)(clock->last_px_ms - (clock->last_beat_ms + clock->interval_ms)) < 0) {
    return false;
  }
  clock->last_beat_ms = clock->last_px_ms;
  if (audible) {
    *audible = (uint32_t)(clock->now_ms - clock->last_px_ms) <= clock->audible_ms;
  }
  return true;
}

bool beat_clock_interval_plausible(uint32_t interval_ms, uint32_t reference_ms,
                                   uint32_t tolerance_pct) {
  if (reference_ms == 0 || tolerance_pct >= 100) {
    return true;
  }
  const uint32_t low = reference_ms * (100 - tolerance_pct) / 100;
  const uint32_t high = reference_ms * (100 + tolerance_pct) / 100;
  return interval_ms >= low && interval_ms <= high;
}

uint32_t beat_clock_median3(uint32_t a, uint32_t b, uint32_t c) {
  if (a > b) { const uint32_t t = a; a = b; b = t; }
  if (b > c) { b = c; }
  return a > b ? a : b;
}

BeatSource beat_clock_select(uint32_t measured_ms, uint32_t measured_age_ms,
                             uint32_t measured_timeout_ms, uint32_t rate_ms,
                             uint32_t *interval_ms) {
  BeatSource source = BeatSourceNone;
  uint32_t interval = 0;
  if (measured_ms > 0 && measured_age_ms <= measured_timeout_ms) {
    source = BeatSourceMeasured;
    interval = measured_ms;
  } else if (rate_ms > 0) {
    source = BeatSourceRate;
    interval = rate_ms;
  }
  if (interval_ms) {
    *interval_ms = interval;
  }
  return source;
}
