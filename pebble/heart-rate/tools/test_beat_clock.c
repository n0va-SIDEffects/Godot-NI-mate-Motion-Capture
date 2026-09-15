/*
 * Host test for the beat clock. Build and run:
 *
 *     cc -Wall -Wextra -o /tmp/test_beat_clock tools/test_beat_clock.c src/c/beat_clock.c
 *     /tmp/test_beat_clock
 *
 * The cases cover the two timing faults seen on real watches: a rate whose beats drifted or were
 * dropped when frames ran late, and a second beat fired by every measured-interval event on top
 * of the beat the clock had already placed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/c/beat_clock.h"

#define PX_MS       20
#define CATCHUP_PX  144
#define AUDIBLE_MS  120
#define MIN_GAP_MS  260
#define MAX_BEATS   4096

static int s_failures;

static void check(bool ok, const char *what, const char *detail) {
  printf("%-4s %s%s%s\n", ok ? "ok" : "FAIL", what, detail ? ": " : "", detail ? detail : "");
  if (!ok) {
    s_failures++;
  }
}

static void check_eq(long got, long want, const char *what) {
  char detail[128];
  snprintf(detail, sizeof(detail), "got %ld, want %ld", got, want);
  check(got == want, what, detail);
}

typedef struct {
  uint32_t times[MAX_BEATS];
  bool audible[MAX_BEATS];
  int count;
} Beats;

//! Run the clock for a stretch of time. Every frame_ms the caller catches up to the clock, just
//! like the app's render loop. interval_every_ms simulates the watch delivering a freshly
//! measured interval at that period; 0 means the rate is set once at the start.
static void run(Beats *beats, uint32_t interval_ms, uint32_t frame_ms, uint32_t duration_ms,
                uint32_t interval_every_ms) {
  BeatClock clock;
  const uint32_t start = 1000000;   // arbitrary, well away from zero
  beat_clock_init(&clock, start, PX_MS, CATCHUP_PX, AUDIBLE_MS, MIN_GAP_MS);
  beat_clock_set_interval(&clock, interval_ms, start);
  memset(beats, 0, sizeof(*beats));

  uint32_t next_update = start + interval_every_ms;
  for (uint32_t now = start; now - start <= duration_ms; now += frame_ms) {
    if (interval_every_ms && (int32_t)(now - next_update) >= 0) {
      beat_clock_set_interval(&clock, interval_ms, now);   // same value, delivered again
      next_update += interval_every_ms;
    }
    uint32_t steps = beat_clock_begin(&clock, now);
    for (uint32_t i = 0; i < steps; i++) {
      bool audible = false;
      if (beat_clock_step(&clock, &audible) && beats->count < MAX_BEATS) {
        beats->audible[beats->count] = audible;
        beats->times[beats->count++] = clock.last_px_ms - start;
      }
    }
  }
}

static void report_spacing(const Beats *b, uint32_t want_ms, uint32_t tolerance_ms,
                           const char *what) {
  bool ok = b->count > 1;
  long worst = 0;
  for (int i = 1; i < b->count; i++) {
    long gap = (long)b->times[i] - (long)b->times[i - 1];
    long err = labs(gap - (long)want_ms);
    if (err > worst) {
      worst = err;
    }
    if (err > (long)tolerance_ms) {
      ok = false;
    }
  }
  char detail[128];
  snprintf(detail, sizeof(detail), "%d beats, worst deviation %ld ms (allowed %u)",
           b->count, worst, tolerance_ms);
  check(ok, what, detail);
}

int main(void) {
  Beats b;

  // A steady rate over a minute: one beat per interval, evenly spaced within one pixel.
  run(&b, 600, 33, 60000, 0);
  report_spacing(&b, 600, PX_MS, "steady 100 bpm keeps its spacing");
  check_eq(b.count, 100, "steady 100 bpm beats once per interval");

  // The watch reports the same measured interval once a second, as it does in live mode. This
  // must not add beats of its own: 100 bpm stays 100 bpm, not 160.
  run(&b, 600, 33, 60000, 1000);
  report_spacing(&b, 600, PX_MS, "repeated interval updates do not add beats");
  check_eq(b.count, 100, "one beat per interval while updates arrive");

  // Updates landing just after a beat used to be the worst case for a double beat.
  run(&b, 606, 33, 30000, 997);
  report_spacing(&b, 606, PX_MS, "updates out of phase with the beat do not add beats");

  // Frames arriving far too late must not stretch the rhythm; the clock catches up instead.
  run(&b, 600, 500, 60000, 1000);
  report_spacing(&b, 600, PX_MS, "late frames neither stretch nor drop beats");
  check_eq(b.count, 100, "late frames keep the beat count");

  // A long stall: the stale timeline is skipped, and the beats replayed on the way back are
  // drawn but silent so the watch cannot fire a burst of clicks.
  {
    BeatClock clock;
    const uint32_t start = 1000000;
    beat_clock_init(&clock, start, PX_MS, CATCHUP_PX, AUDIBLE_MS, MIN_GAP_MS);
    beat_clock_set_interval(&clock, 600, start);
    beat_clock_begin(&clock, start);            // settle
    uint32_t steps = beat_clock_begin(&clock, start + 30000);   // 30 s gone missing
    check(steps <= CATCHUP_PX, "a long stall replays at most one screen", NULL);
    int silent = 0, loud = 0;
    for (uint32_t i = 0; i < steps; i++) {
      bool audible = false;
      if (beat_clock_step(&clock, &audible)) {
        audible ? loud++ : silent++;
      }
    }
    check(loud <= 1, "a long stall sounds at most the current beat", NULL);
    check(silent >= 1, "a long stall still draws the beats it catches up on", NULL);
  }

  // A rate change applies from the last beat rather than after the old interval has run out.
  // At 1000 ms the beats fall at 20 and 1020; switching to 400 ms at 1200 must put the next one
  // at 1420, one new interval after the last beat, not at 2020.
  {
    BeatClock clock;
    const uint32_t start = 1000000;
    beat_clock_init(&clock, start, PX_MS, CATCHUP_PX, AUDIBLE_MS, MIN_GAP_MS);
    beat_clock_set_interval(&clock, 1000, start);
    long after_change = -1;
    for (uint32_t now = start; now - start <= 4000; now += 20) {
      if (now - start == 1200) {
        beat_clock_set_interval(&clock, 400, now);
      }
      uint32_t steps = beat_clock_begin(&clock, now);
      for (uint32_t i = 0; i < steps; i++) {
        if (beat_clock_step(&clock, NULL)) {
          const long at = (long)(clock.last_px_ms - start);
          if (at > 1200 && after_change < 0) {
            after_change = at;
          }
        }
      }
    }
    check_eq(after_change, 1420, "a faster rate applies from the last beat");
  }

  // Timestamps that wrap around must keep working.
  {
    BeatClock clock;
    const uint32_t start = 0xFFFFF000;
    beat_clock_init(&clock, start, PX_MS, CATCHUP_PX, AUDIBLE_MS, MIN_GAP_MS);
    beat_clock_set_interval(&clock, 600, start);
    int count = 0;
    for (uint32_t i = 0; i <= 60000 / 33; i++) {
      uint32_t now = start + i * 33;
      uint32_t steps = beat_clock_begin(&clock, now);
      for (uint32_t k = 0; k < steps; k++) {
        if (beat_clock_step(&clock, NULL)) {
          count++;
        }
      }
    }
    check_eq(count, 100, "the clock survives a timestamp wrap");
  }

  // A measured interval is only believed when it agrees with the averaged rate. The numbers are
  // taken from a recording of the watch: at a displayed 105 bpm, one interval per heartbeat is
  // about 571 ms, and the sensor started reporting half of that, which doubled the pulse.
  {
    const uint32_t rate = 571;   // 105 bpm
    check(beat_clock_interval_plausible(600, rate, 25), "a normal interval is believed", NULL);
    check(beat_clock_interval_plausible(540, rate, 25), "beat-to-beat variation is believed", NULL);
    check(!beat_clock_interval_plausible(296, rate, 25), "a halved interval is rejected", NULL);
    check(!beat_clock_interval_plausible(1142, rate, 25), "a doubled interval is rejected", NULL);
    check(!beat_clock_interval_plausible(406, rate, 25), "the 406 ms outlier is rejected", NULL);
    check(beat_clock_interval_plausible(296, 0, 25), "without a rate every interval is believed",
          NULL);
  }

  // End to end, replaying what the watch did on the wrist: the rate held at 105 bpm while the
  // sensor first reported one interval per heartbeat and then, from four seconds in, half of it.
  // Following those readings doubled the pulse; the beat must stay with the displayed rate.
  {
    const uint32_t rate_ms = 60000 / 105;
    const uint32_t measured[] = {600, 623, 603, 601, 603, 576,   // one per heartbeat
                                 406, 295, 304, 268, 358, 451,   // the sensor misreading
                                 290, 300, 285, 295, 310, 288};
    const uint32_t measured_count = sizeof(measured) / sizeof(measured[0]);

    BeatClock clock;
    const uint32_t start = 1000000;
    beat_clock_init(&clock, start, PX_MS, CATCHUP_PX, AUDIBLE_MS, MIN_GAP_MS);
    uint32_t believed_ms = 0, believed_at = 0, next_reading = start, index = 0;
    uint32_t recent[3] = {0, 0, 0};
    unsigned have = 0;
    int beats = 0;
    long shortest = 100000, previous_beat = -1;

    for (uint32_t now = start; now - start <= 18000; now += 33) {
      // The watch delivers a measured interval about once a second.
      if (index < measured_count && (int32_t)(now - next_reading) >= 0) {
        const uint32_t reading = measured[index++];
        if (beat_clock_interval_plausible(reading, rate_ms, 25)) {
          recent[2] = recent[1];
          recent[1] = recent[0];
          recent[0] = reading;
          if (have < 3) {
            have++;
          }
          believed_ms = (have == 3) ? beat_clock_median3(recent[0], recent[1], recent[2]) : reading;
          believed_at = now;
        }
        next_reading += 1000;
      }
      uint32_t interval = 0;
      beat_clock_select(believed_ms, now - believed_at, 5000, rate_ms, &interval);
      beat_clock_set_interval(&clock, interval, now);

      uint32_t steps = beat_clock_begin(&clock, now);
      for (uint32_t i = 0; i < steps; i++) {
        if (beat_clock_step(&clock, NULL)) {
          beats++;
          const long at = (long)(clock.last_px_ms - start);
          if (previous_beat >= 0 && at - previous_beat < shortest) {
            shortest = at - previous_beat;
          }
          previous_beat = at;
        }
      }
    }
    char detail[128];
    snprintf(detail, sizeof(detail), "%d beats in 18 s, %.0f bpm, closest pair %ld ms",
             beats, beats * 60000.0 / 18000.0, shortest);
    check(beats >= 29 && beats <= 32 && shortest >= 540, "misread intervals cannot rush the pulse",
          detail);
  }

  // The middle of three readings, so a single odd one cannot set the tempo.
  {
    check_eq(beat_clock_median3(600, 451, 580), 580, "an outlier is outvoted");
    check_eq(beat_clock_median3(451, 600, 580), 580, "the order does not matter");
    check_eq(beat_clock_median3(590, 600, 610), 600, "three similar readings keep the middle");
  }

  // A rate arriving after a pause must not beat on top of the beat just played. In a recording
  // from the watch two beeps landed 94 ms apart, the first one cut short and heard as a click.
  {
    BeatClock clock;
    const uint32_t start = 1000000;
    beat_clock_init(&clock, start, PX_MS, CATCHUP_PX, AUDIBLE_MS, MIN_GAP_MS);
    beat_clock_set_interval(&clock, 600, start);
    long last = -1, shortest = 100000;
    int beats = 0;
    for (uint32_t now = start; now - start <= 12000; now += 33) {
      // The rate keeps dropping out and coming back, as it does when the sensor loses the pulse.
      const uint32_t t = now - start;
      if (t % 1000 < 100) {
        beat_clock_set_interval(&clock, 0, now);
      } else if (t % 1000 < 200) {
        beat_clock_set_interval(&clock, 600, now);
      }
      uint32_t steps = beat_clock_begin(&clock, now);
      for (uint32_t i = 0; i < steps; i++) {
        if (beat_clock_step(&clock, NULL)) {
          beats++;
          const long at = (long)(clock.last_px_ms - start);
          if (last >= 0 && at - last < shortest) {
            shortest = at - last;
          }
          last = at;
        }
      }
    }
    char detail[128];
    snprintf(detail, sizeof(detail), "%d beats, closest pair %ld ms", beats, shortest);
    check(shortest >= MIN_GAP_MS, "a rate returning after a pause cannot clip the last beep",
          detail);
  }

  // The watch clock sometimes reports a whole second beside the truth around second boundaries.
  {
    const uint32_t base = 1000000;
    // A normal step passes through untouched.
    check_eq((long)beat_clock_filter_time(base + 33, base, base, 40) - (long)base, 33,
             "a normal step is left alone");
    // A reading a second ahead is pulled back to where it belongs.
    check_eq((long)beat_clock_filter_time(base + 1033, base, base, 40) - (long)base, 33,
             "a reading a second early is corrected");
    // And one a second behind.
    check_eq((long)beat_clock_filter_time(base - 967, base, base, 40) - (long)base, 33,
             "a reading a second late is corrected");
    // The correction carries forward, so time keeps running smoothly after a hiccup.
    const uint32_t corrected = beat_clock_filter_time(base + 1033, base, base, 40);
    check_eq((long)beat_clock_filter_time(base + 1066, base + 1033, corrected, 40) - (long)base, 66,
             "time keeps running after a corrected hiccup");
    // A real stall of several seconds is a stall, not a hiccup, and must be seen as one.
    check_eq((long)beat_clock_filter_time(base + 5000, base, base, 40) - (long)base, 5000,
             "a long stall is not mistaken for a hiccup");
  }

  printf("\n%s\n", s_failures ? "FAILURES" : "all checks passed");
  return s_failures ? 1 : 0;
}
