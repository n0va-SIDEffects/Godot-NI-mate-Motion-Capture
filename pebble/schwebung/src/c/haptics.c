#include "haptics.h"
#include "config.h"
#include "e1clock.h"
#include "audio.h"

typedef enum {
  HapOff = 0,
  HapSlow,     // unter 0,5 Hz: weicher Zweierschlag alle 2 s
  HapBeat,     // 0,5 bis 5 Hz: zaehlbare Einzelimpulse
  HapBuzz,     // 5 bis 6 Hz: Rauheit
  HapWarn,     // Bruchwarnung
  HapTest,     // LRA-Test
} HapMode;

static HapMode s_want_mode = HapOff;
static uint32_t s_want_period_ms = 1000;
static HapMode s_run_mode = HapOff;
static AppTimer *s_next_timer;
static AppTimer *s_event_timer;
static uint32_t s_pulse_start_ms;
static uint32_t s_pulse_len_ms;
static uint32_t s_pat[3];
static uint32_t s_calls;
static bool s_log_calls;
static bool s_test_active;
static uint8_t s_pending_event;   // 0 keins, 1 Einrasten, 2 Bruch
static uint32_t s_next_due_ms;    // Solltermin des naechsten Impulses (0 = Raster neu ankern)

static const char *s_names[] = { "aus", "langsam", "puls", "rauh", "warn", "test" };

static void prv_on_next(void *data);
static void prv_on_event_timer(void *data);

static bool prv_motor_busy(uint32_t now) {
  return s_pulse_len_ms != 0 && (now - s_pulse_start_ms) < (s_pulse_len_ms + 20);
}

static uint32_t prv_busy_left(uint32_t now) {
  uint32_t el = now - s_pulse_start_ms;
  uint32_t total = s_pulse_len_ms + 20;
  return el < total ? total - el : 0;
}

static void prv_schedule_next(uint32_t delay_ms) {
  if (s_next_timer) {
    app_timer_cancel(s_next_timer);
  }
  s_next_timer = app_timer_register(delay_ms, prv_on_next, NULL);
}

// Plant den naechsten Impuls auf ein festes Raster statt "Periode ab jetzt":
// Timer-Latenz und Aufrufdauer summieren sich sonst zu einer Drift von einigen
// Millisekunden pro Impuls (im Emulator gemessen: 125 ms Soll, 134 ms Ist).
static void prv_schedule_period(uint32_t now, uint32_t period_ms) {
  if (s_next_due_ms == 0 || (int32_t)(now - s_next_due_ms) > (int32_t)period_ms) {
    s_next_due_ms = now;   // erster Impuls oder weit hinterher: neu ankern
  }
  s_next_due_ms += period_ms;
  int32_t delay = (int32_t)(s_next_due_ms - now);
  int32_t min_delay = (int32_t)(s_pulse_len_ms + 20);   // Motor muss frei sein
  if (delay < min_delay) {
    delay = min_delay;
    s_next_due_ms = now + (uint32_t)delay;
  }
  prv_schedule_next((uint32_t)delay);
}

static void prv_enqueue(uint32_t n, uint32_t len_ms, HapMode mode, uint32_t now) {
  VibePattern pat = { .durations = s_pat, .num_segments = n };
  vibes_enqueue_custom_pattern(pat);
  s_calls++;
  s_pulse_start_ms = now;
  s_pulse_len_ms = len_ms;
  s_run_mode = mode;
  if (s_log_calls) {
    APP_LOG(APP_LOG_LEVEL_INFO, "[E1][LRA] t=%lu pulse mode=%s period=%lu ms len=%lu ms",
            (unsigned long)now, s_names[mode], (unsigned long)s_want_period_ms,
            (unsigned long)len_ms);
  }
}

// Reiht den Impuls des gewuenschten Modus ein und plant den naechsten.
static void prv_fire(uint32_t now) {
  switch (s_want_mode) {
    case HapSlow:
      s_pat[0] = 25;
      s_pat[1] = 100;
      s_pat[2] = 25;
      prv_enqueue(3, 150, HapSlow, now);
      prv_schedule_period(now, 2000);
      break;
    case HapBuzz:
      s_pat[0] = LRA_BUZZ_ON_MS;
      prv_enqueue(1, LRA_BUZZ_ON_MS, HapBuzz, now);
      prv_schedule_period(now, 250);
      break;
    case HapBeat:
    case HapWarn:
    case HapTest: {
      uint32_t p = s_want_period_ms;
      if (p < LRA_PULSE_MS + 20) {
        p = LRA_PULSE_MS + 20;
      }
      s_pat[0] = LRA_PULSE_MS;
      prv_enqueue(1, LRA_PULSE_MS, s_want_mode, now);
      prv_schedule_period(now, p);
      break;
    }
    default:
      s_run_mode = HapOff;
      break;
  }
}

static void prv_fire_event(uint32_t now) {
  uint8_t ev = s_pending_event;
  s_pending_event = 0;
  if (ev == 1) {
    vibes_double_pulse();
    s_pulse_len_ms = 400;
  } else {
    vibes_long_pulse();
    s_pulse_len_ms = 500;
  }
  s_calls++;
  s_pulse_start_ms = now;
  s_run_mode = HapOff;
  s_next_due_ms = 0;
  if (s_want_mode != HapOff) {
    prv_schedule_next(s_pulse_len_ms + 50);
  }
}

static void prv_on_next(void *data) {
  s_next_timer = NULL;
  uint32_t now = e1clock_now_ms();
  if (s_pending_event) {
    prv_fire_event(now);
    return;
  }
  if (s_want_mode == HapOff) {
    s_run_mode = HapOff;
    return;
  }
  if (prv_motor_busy(now)) {
    prv_schedule_next(prv_busy_left(now));
    return;
  }
  prv_fire(now);
}

static void prv_on_event_timer(void *data) {
  s_event_timer = NULL;
  if (s_pending_event) {
    prv_fire_event(e1clock_now_ms());
  }
}

static void prv_request_event(uint8_t ev) {
  uint32_t now = e1clock_now_ms();
  s_pending_event = ev;
  if (s_next_timer) {
    app_timer_cancel(s_next_timer);
    s_next_timer = NULL;
  }
  if (prv_motor_busy(now)) {
    if (s_event_timer) {
      app_timer_cancel(s_event_timer);
    }
    s_event_timer = app_timer_register(prv_busy_left(now), prv_on_event_timer, NULL);
  } else {
    prv_fire_event(now);
  }
}

static void prv_request(HapMode mode, uint32_t period_ms) {
  if (mode != s_want_mode || period_ms != s_want_period_ms) {
    s_next_due_ms = 0;   // neues Raster
  }
  s_want_mode = mode;
  s_want_period_ms = period_ms;
  if (mode == HapOff) {
    return;   // kein Cancel: laufender Impuls klingt aus, kein naechster
  }
  if (s_next_timer || s_event_timer || s_pending_event) {
    return;   // Rhythmus laeuft, der naechste Impuls nimmt die neue Rate
  }
  uint32_t now = e1clock_now_ms();
  if (prv_motor_busy(now)) {
    prv_schedule_next(prv_busy_left(now));
    return;
  }
  prv_fire(now);
}

void haptics_init(void) {
  s_want_mode = HapOff;
  s_run_mode = HapOff;
  s_calls = 0;
  s_log_calls = false;
  s_test_active = false;
  s_pending_event = 0;
  s_pulse_len_ms = 0;
}

void haptics_deinit(void) {
  haptics_stop();
}

void haptics_set_beat(int32_t beat_chz, bool active) {
  if (s_test_active) {
    return;
  }
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
  if (s_test_active) {
    return;
  }
  if (ms_left > WARN_MS) {
    ms_left = WARN_MS;
  }
  uint32_t gap = 80 + (120 * ms_left) / WARN_MS;
  prv_request(HapWarn, LRA_PULSE_MS + gap);
}

void haptics_lock(void) {
  if (s_test_active) {
    return;
  }
  s_want_mode = HapOff;
  prv_request_event(1);
}

void haptics_break(void) {
  if (s_test_active) {
    return;
  }
  s_want_mode = HapOff;
  prv_request_event(2);
}

bool haptics_marker(void) {
  uint32_t now = e1clock_now_ms();
  if (prv_motor_busy(now) || s_pending_event) {
    return false;
  }
  s_pat[0] = 40;
  prv_enqueue(1, 40, HapOff, now);
  return true;
}

void haptics_test_rate(uint8_t hz) {
  if (hz < 1) {
    hz = 1;
  }
  s_test_active = true;
  s_log_calls = true;
  s_pending_event = 0;
  if (s_event_timer) {
    app_timer_cancel(s_event_timer);
    s_event_timer = NULL;
  }
  if (s_next_timer) {
    app_timer_cancel(s_next_timer);
    s_next_timer = NULL;
  }
  s_want_mode = HapTest;
  s_want_period_ms = 1000 / hz;
  s_next_due_ms = 0;
  uint32_t now = e1clock_now_ms();
  if (prv_motor_busy(now)) {
    prv_schedule_next(prv_busy_left(now));
  } else {
    prv_fire(now);
  }
}

void haptics_stop(void) {
  if (s_next_timer) {
    app_timer_cancel(s_next_timer);
    s_next_timer = NULL;
  }
  if (s_event_timer) {
    app_timer_cancel(s_event_timer);
    s_event_timer = NULL;
  }
  s_pending_event = 0;
  s_want_mode = HapOff;
  s_run_mode = HapOff;
  s_next_due_ms = 0;
  s_test_active = false;
  s_log_calls = false;
  uint32_t now = e1clock_now_ms();
  if (prv_motor_busy(now)) {
    audio_top_up(AUDIO_TOPUP_MS);   // vibes_cancel kann blockieren: vorher Audio auffuellen
    vibes_cancel();
    s_pulse_len_ms = 0;
  }
}

uint32_t haptics_call_count(void) {
  return s_calls;
}

const char *haptics_mode_name(void) {
  return s_names[s_want_mode];
}
