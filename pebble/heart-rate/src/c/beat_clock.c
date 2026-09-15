#include "beat_clock.h"

void beat_clock_init(BeatClock *clock, uint32_t now_ms, uint32_t px_ms, uint32_t catchup_px,
                     uint32_t audible_ms, uint32_t min_gap_ms) {
  clock->px_ms = px_ms;
  clock->catchup_px = catchup_px;
  clock->audible_ms = audible_ms;
  clock->min_gap_ms = min_gap_ms;
  clock->interval_ms = 0;
  clock->last_px_ms = now_ms;
  // Backdate the last beat by the minimum gap: nothing has been played yet, so the first rate to
  // arrive must be free to beat straight away.
  clock->last_beat_ms = now_ms - min_gap_ms;
  clock->now_ms = now_ms;
}

void beat_clock_set_interval(BeatClock *clock, uint32_t interval_ms, uint32_t now_ms) {
  if (interval_ms == clock->interval_ms) {
    return;
  }
  const bool was_stopped = (clock->interval_ms == 0);
  clock->interval_ms = interval_ms;
  if (was_stopped && interval_ms > 0) {
    // Start beating at once, but never so soon after the last beat that its beep is cut off.
    const uint32_t earliest = clock->last_beat_ms + clock->min_gap_ms;
    const uint32_t at = ((int32_t)(earliest - now_ms) > 0) ? earliest : now_ms;
    clock->last_beat_ms = at - interval_ms;
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
  if ((uint32_t)(clock->last_px_ms - clock->last_beat_ms) < clock->min_gap_ms) {
    return false;   // too soon after the last beat: hold it back rather than clip its beep
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

#define SECOND_MS     1000
#define TOLERANCE_MS  40

uint32_t beat_clock_filter_time(uint32_t raw_ms, uint32_t previous_raw, uint32_t previous_out,
                                uint32_t step_ms) {
  const int32_t offset = (int32_t)(previous_out - previous_raw);
  const int32_t step = (int32_t)(raw_ms - previous_raw);
  const int32_t slack = (int32_t)step_ms + TOLERANCE_MS;

  int32_t correction = 0;
  if (step > SECOND_MS - slack && step < SECOND_MS + slack) {
    correction = -SECOND_MS;
  } else if (step < -(SECOND_MS - slack) && step > -(SECOND_MS + slack)) {
    correction = SECOND_MS;
  }
  return raw_ms + (uint32_t)(offset + correction);
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
