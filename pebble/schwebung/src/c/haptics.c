#include "haptics.h"
#include "config.h"
#include "e1clock.h"

typedef enum {
  HapOff = 0,
  HapSlow,     // unter 0,5 Hz: weicher Zweierschlag alle 2 s
  HapBeat,     // 0,5 bis 5 Hz: zaehlbare Einzelimpulse
  HapBuzz,     // 5 bis 6 Hz: Rauheit
  HapWarn,     // Bruchwarnung
  HapTest,     // LRA-Test
} HapMode;

static HapMode s_mode = HapOff;
static HapMode s_want_mode = HapOff;
static uint32_t s_period_ms;
static uint32_t s_want_period_ms;
static uint32_t s_pat[64];
static uint32_t s_grid_total_ms;   // Musterlaenge inklusive letzter Pause
static uint32_t s_pat_start_ms;
static AppTimer *s_end_timer;
static AppTimer *s_boundary_timer;
static uint32_t s_block_until_ms;
static uint32_t s_calls;
static bool s_log_calls;

static const char *s_names[] = { "aus", "langsam", "puls", "rauh", "warn", "test" };

static void prv_cancel_timers(void) {
  if (s_end_timer) {
    app_timer_cancel(s_end_timer);
    s_end_timer = NULL;
  }
  if (s_boundary_timer) {
    app_timer_cancel(s_boundary_timer);
    s_boundary_timer = NULL;
  }
}

static void prv_on_end(void *data);

static void prv_start(HapMode mode, uint32_t period_ms) {
  uint32_t n = 0;
  uint32_t grid = 0;
  switch (mode) {
    case HapSlow:
      s_pat[n++] = 25;
      s_pat[n++] = 100;
      s_pat[n++] = 25;
      period_ms = 2000;
      grid = 2000;
      break;
    case HapBuzz:
      s_pat[n++] = 250;
      period_ms = 250;
      grid = 250;
      break;
    case HapBeat:
    case HapWarn:
    case HapTest: {
      const uint32_t on = LRA_PULSE_MS;
      if (period_ms < on + 20) {
        period_ms = on + 20;
      }
      uint32_t pulses = 1000 / period_ms;
      if (pulses < 1) {
        pulses = 1;
      }
      if (pulses > 31) {
        pulses = 31;
      }
      for (uint32_t i = 0; i < pulses; i++) {
        s_pat[n++] = on;
        if (i + 1 < pulses) {
          s_pat[n++] = period_ms - on;
        }
      }
      grid = pulses * period_ms;
      break;
    }
    default:
      return;
  }
  VibePattern pat = { .durations = s_pat, .num_segments = n };
  vibes_enqueue_custom_pattern(pat);
  s_calls++;
  s_mode = mode;
  s_period_ms = period_ms;
  s_grid_total_ms = grid;
  s_pat_start_ms = e1clock_now_ms();
  if (s_log_calls) {
    APP_LOG(APP_LOG_LEVEL_INFO, "[E1][LRA] t=%lu enqueue mode=%s period=%lu ms segs=%lu",
            (unsigned long)s_pat_start_ms, s_names[mode], (unsigned long)period_ms,
            (unsigned long)n);
  }
  s_end_timer = app_timer_register(grid, prv_on_end, NULL);
}

static void prv_on_end(void *data) {
  s_end_timer = NULL;
  s_mode = HapOff;
  uint32_t now = e1clock_now_ms();
  if (s_want_mode != HapOff && (int32_t)(now - s_block_until_ms) >= 0) {
    prv_start(s_want_mode, s_want_period_ms);
  }
}

static void prv_on_boundary(void *data) {
  s_boundary_timer = NULL;
  vibes_cancel();
  prv_cancel_timers();
  s_mode = HapOff;
  if (s_want_mode != HapOff) {
    prv_start(s_want_mode, s_want_period_ms);
  }
}

static void prv_request(HapMode mode, uint32_t period_ms) {
  s_want_mode = mode;
  s_want_period_ms = period_ms;
  uint32_t now = e1clock_now_ms();
  if ((int32_t)(now - s_block_until_ms) < 0) {
    return;   // Ereignispuls laeuft, danach uebernimmt der naechste Aufruf
  }
  if (s_mode == HapOff) {
    if (mode != HapOff && !s_end_timer) {
      prv_start(mode, period_ms);
    }
    return;
  }
  if (mode == s_mode) {
    uint32_t a = s_period_ms;
    uint32_t b = period_ms;
    uint32_t diff = a > b ? a - b : b - a;
    if (diff * 100 < a * LRA_RATE_HYST_PCT) {
      return;   // Hysterese: laufendes Muster behalten, Ende uebernimmt neue Rate
    }
  }
  if (s_boundary_timer) {
    return;
  }
  // Wechsel an der naechsten Impulsgrenze
  uint32_t el = now - s_pat_start_ms;
  uint32_t k = el / s_period_ms + 1;
  uint32_t tb = k * s_period_ms;
  if (tb >= s_grid_total_ms) {
    return;   // Musterende kommt ohnehin gleich
  }
  s_boundary_timer = app_timer_register(tb - el, prv_on_boundary, NULL);
}

void haptics_init(void) {
  s_mode = HapOff;
  s_want_mode = HapOff;
  s_calls = 0;
  s_log_calls = false;
  s_block_until_ms = 0;
}

void haptics_deinit(void) {
  haptics_stop();
}

void haptics_set_beat(int32_t beat_chz, bool active) {
  if (!active) {
    prv_request(HapOff, 0);
  } else if (beat_chz < LOCK_CHZ) {
    prv_request(HapSlow, 2000);
  } else if (beat_chz <= LRA_COUNTABLE_MAX_CHZ) {
    prv_request(HapBeat, (uint32_t)(100000 / beat_chz));
  } else if (beat_chz <= LRA_BUZZ_MAX_CHZ) {
    prv_request(HapBuzz, 250);
  } else {
    prv_request(HapOff, 0);
  }
}

void haptics_warn(uint32_t ms_left) {
  if (ms_left > WARN_MS) {
    ms_left = WARN_MS;
  }
  uint32_t gap = 80 + (120 * ms_left) / WARN_MS;
  prv_request(HapWarn, LRA_PULSE_MS + gap);
}

void haptics_lock(void) {
  prv_cancel_timers();
  vibes_cancel();
  vibes_double_pulse();
  s_calls++;
  s_mode = HapOff;
  s_want_mode = HapOff;
  s_block_until_ms = e1clock_now_ms() + 500;
}

void haptics_break(void) {
  prv_cancel_timers();
  vibes_cancel();
  vibes_long_pulse();
  s_calls++;
  s_mode = HapOff;
  s_want_mode = HapOff;
  s_block_until_ms = e1clock_now_ms() + 900;
}

void haptics_test_rate(uint8_t hz) {
  s_log_calls = true;
  prv_cancel_timers();
  vibes_cancel();
  s_mode = HapOff;
  s_block_until_ms = 0;
  if (hz < 1) {
    hz = 1;
  }
  prv_request(HapTest, 1000 / hz);
}

void haptics_stop(void) {
  prv_cancel_timers();
  vibes_cancel();
  s_mode = HapOff;
  s_want_mode = HapOff;
  s_log_calls = false;
}

uint32_t haptics_call_count(void) {
  return s_calls;
}

const char *haptics_mode_name(void) {
  return s_names[s_mode];
}
