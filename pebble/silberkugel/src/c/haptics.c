#include "haptics.h"
#include "config.h"
#include "e1clock.h"

static HapStats s_st;
static uint32_t s_last_ms;        // Beginn des letzten Impulses
static uint32_t s_last_len;       // seine Laenge
static uint32_t s_pat[1];
static uint32_t s_geiger_ms;      // gewuenschter Abstand, 0 = aus
static uint32_t s_geiger_next;    // Solltermin des naechsten Ticks
static uint32_t s_pending_len;    // wartender Impuls hoher Prioritaet
static uint32_t s_pending_since;

// Der Motor gilt als belegt, solange das Muster laeuft, plus ein kurzer
// Nachlauf: der LRA schwingt aus, und ein sofort folgender Aufruf wuerde
// entweder abgelehnt oder als ein einziger langer Impuls gefuehlt.
static bool prv_busy(uint32_t now) {
  if (s_last_len == 0) {
    return false;
  }
  uint32_t el = now - s_last_ms;
  if (el < s_last_len + LRA_GUARD_MS) {
    return true;
  }
  return el < LRA_MIN_GAP_MS;
}

static void prv_fire(uint32_t now, uint32_t len_ms) {
  s_pat[0] = len_ms;
  VibePattern pat = { .durations = s_pat, .num_segments = 1 };
  vibes_enqueue_custom_pattern(pat);
  if (s_st.calls > 0) {
    uint32_t gap = now - s_last_ms;
    if (s_st.min_gap_ms == 0 || gap < s_st.min_gap_ms) {
      s_st.min_gap_ms = gap;
    }
  }
  s_last_ms = now;
  s_last_len = len_ms;
  s_st.calls++;
}

void haptics_init(void) {
  memset(&s_st, 0, sizeof(s_st));
  s_last_ms = 0;
  s_last_len = 0;
  s_geiger_ms = 0;
  s_geiger_next = 0;
  s_pending_len = 0;
}

void haptics_reset_stats(void) {
  memset(&s_st, 0, sizeof(s_st));
}

bool haptics_pulse(uint32_t len_ms, HapPrio prio) {
  uint32_t now = e1clock_now_ms();
  if (!prv_busy(now)) {
    prv_fire(now, len_ms);
    if (prio == HapGeiger) {
      s_st.geiger++;
    }
    return true;
  }
  if (prio >= HapEvent) {
    // Flipper, Bumper und Abfluss duerfen warten; der Tick holt sie nach,
    // sobald der Motor frei ist. Ein zweiter wartender Impuls ersetzt den
    // ersten: im Flipper zaehlt das Neueste, nicht das Aelteste.
    s_pending_len = len_ms;
    s_pending_since = now;
    s_st.queued++;
    return true;
  }
  s_st.dropped++;
  return false;
}

void haptics_geiger(uint32_t period_ms) {
  if (period_ms != s_geiger_ms) {
    s_geiger_ms = period_ms;
    if (period_ms == 0) {
      s_geiger_next = 0;
    } else {
      // Neues Raster ab jetzt, aber nie frueher als der bisherige Termin:
      // sonst wird der Geiger beim Annaehern zum Dauerbrummen.
      uint32_t now = e1clock_now_ms();
      uint32_t due = now + period_ms;
      if (s_geiger_next == 0 || due < s_geiger_next) {
        s_geiger_next = due;
      }
    }
  }
}

void haptics_tick(uint32_t now) {
  if (s_pending_len > 0 && !prv_busy(now)) {
    uint32_t waited = now - s_pending_since;
    if (waited > s_st.busy_ms_max) {
      s_st.busy_ms_max = waited;
    }
    uint32_t len = s_pending_len;
    s_pending_len = 0;
    prv_fire(now, len);
    return;   // in diesem Tick nicht noch einen Geiger-Tick hinterherschicken
  }
  if (s_geiger_ms == 0) {
    return;
  }
  if (s_geiger_next == 0) {
    s_geiger_next = now + s_geiger_ms;
    return;
  }
  if ((int32_t)(now - s_geiger_next) < 0) {
    return;
  }
  // Auf festes Raster planen, nicht "ab jetzt": sonst summiert sich die
  // Timer-Latenz zu einer hoerbaren Drift, und der Geiger soll die Entfernung
  // kodieren, nicht die Systemlast.
  s_geiger_next += s_geiger_ms;
  if ((int32_t)(now - s_geiger_next) > (int32_t)s_geiger_ms) {
    s_geiger_next = now + s_geiger_ms;   // weit hinterher: neu ankern
  }
  if (prv_busy(now)) {
    s_st.dropped++;
    return;
  }
  prv_fire(now, GEIGER_PULSE_MS);
  s_st.geiger++;
}

void haptics_stop(void) {
  s_geiger_ms = 0;
  s_geiger_next = 0;
  s_pending_len = 0;
  // Bewusst kein vibes_cancel: ein laufender Impuls ist nach spaetestens
  // 400 ms zu Ende, das Blockieren des App-Tasks waere teurer.
}

uint32_t haptics_last_call_ms(void) {
  return s_last_ms;
}

const HapStats *haptics_stats(void) {
  return &s_st;
}
