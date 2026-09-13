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
