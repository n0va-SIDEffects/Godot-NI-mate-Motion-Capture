#include "e1clock.h"

// time_ms() liefert Sekunden und Millisekunden getrennt; an Sekundengrenzen
// passen die beiden Teile gelegentlich nicht zusammen (Sprung um +-1000 ms).
// Deshalb zaehlt diese Uhr nur die Millisekunden-Differenzen (modulo 1000)
// zusammen. Sie ist damit streng monoton. Mit dem Rohwert gleicht sie sich
// nur ab, wenn eine Abweichung laenger als 1,5 s anhaelt (echte Pause oder
// Zeitsynchronisation), und dann nur vorwaerts: bei negativer Abweichung wird
// die Rohbasis verschoben, nie die eigene Zeit zurueckgesetzt.
// Voraussetzung: Aufrufe kommen oefter als einmal pro Sekunde (Audio-Tick 8 ms).

static bool s_init;
static uint32_t s_acc_ms;
static uint16_t s_prev_ms;
static uint64_t s_raw_base;
static uint32_t s_glitches;
static uint32_t s_resyncs;
static bool s_off;
static uint32_t s_off_since;
static int64_t s_last_diff;

void e1clock_init(void) {
  time_t secs = 0;
  uint16_t ms = 0;
  time_ms(&secs, &ms);
  s_prev_ms = ms;
  s_acc_ms = 0;
  s_raw_base = (uint64_t)secs * 1000ULL + ms;
  s_glitches = 0;
  s_resyncs = 0;
  s_off = false;
  s_last_diff = 0;
  s_init = true;
}

uint32_t e1clock_now_ms(void) {
  if (!s_init) {
    e1clock_init();
    return 0;
  }
  time_t secs = 0;
  uint16_t ms = 0;
  time_ms(&secs, &ms);
  uint32_t d = ((uint32_t)ms + 1000u - (uint32_t)s_prev_ms) % 1000u;
  s_prev_ms = ms;
  s_acc_ms += d;

  uint64_t raw_now = (uint64_t)secs * 1000ULL + ms;
  int64_t raw_el = (int64_t)raw_now - (int64_t)s_raw_base;
  int64_t diff = raw_el - (int64_t)s_acc_ms;
  int64_t jitter = diff - s_last_diff;
  if (jitter < 0) {
    jitter = -jitter;
  }
  s_last_diff = diff;

  if (diff > 700 || diff < -700) {
    if (!s_off) {
      s_off = true;
      s_off_since = s_acc_ms;
      s_glitches++;
    } else if ((s_acc_ms - s_off_since) > 1500 && jitter < 50) {
      // Abweichung haelt an und ist stabil: kein Glitch, sondern Pause oder Zeitsync
      if (diff > 0) {
        s_acc_ms = (uint32_t)raw_el;          // vorwaerts nachziehen
      } else {
        s_raw_base = raw_now - s_acc_ms;      // Rohbasis verschieben, Uhr bleibt monoton
      }
      s_off = false;
      s_resyncs++;
    }
  } else {
    s_off = false;
  }
  return s_acc_ms;
}

uint32_t e1clock_glitches(void) {
  return s_glitches;
}

uint32_t e1clock_resyncs(void) {
  return s_resyncs;
}
