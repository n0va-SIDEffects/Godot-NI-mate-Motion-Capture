#include "e1clock.h"

// time_ms() liefert Sekunden und Millisekunden getrennt; an Sekundengrenzen
// passen die beiden Teile gelegentlich nicht zusammen (Sprung um +-1000 ms).
// Deshalb zaehlt diese Uhr nur die Millisekunden-Differenzen (modulo 1000)
// zusammen und gleicht sich nur bei einer laenger anhaltenden Abweichung
// (echte Pause) wieder mit dem Rohwert ab. Voraussetzung: Aufrufe kommen
// oefter als einmal pro Sekunde (der Audio-Tick ruft alle 8 ms).

static bool s_init;
static uint32_t s_acc_ms;
static uint16_t s_prev_ms;
static uint64_t s_raw_base;
static uint32_t s_glitches;
static uint32_t s_resyncs;
static bool s_off;
static uint32_t s_off_since;

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

  int64_t raw_el = (int64_t)((uint64_t)secs * 1000ULL + ms) - (int64_t)s_raw_base;
  int64_t diff = raw_el - (int64_t)s_acc_ms;
  if (diff > 700 || diff < -700) {
    if (!s_off) {
      s_off = true;
      s_off_since = s_acc_ms;
      s_glitches++;
    } else if ((s_acc_ms - s_off_since) > 1500 && raw_el > 0) {
      // Abweichung haelt an: echte Pause, auf Rohwert nachziehen
      s_acc_ms = (uint32_t)raw_el;
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
