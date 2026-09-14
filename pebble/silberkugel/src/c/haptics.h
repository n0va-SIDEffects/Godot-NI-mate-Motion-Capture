#pragma once
#include <pebble.h>

// LRA-Vokabular fuer den Flipper. Zwei Regeln aus dem Hardware-Befund:
// kein vibes_cancel (blockiert den App-Task 10 bis 80 ms), und waehrend ein
// Muster laeuft, nimmt das System kein zweites an. Deshalb ist jeder Impuls
// ein eigener kurzer Aufruf, und was nicht passt, wird gezaehlt statt
// erzwungen. Genau diese Zahl beantwortet die offene Frage, ob der Motor die
// kurzen Impulse des Konzepts (Flipper 15 ms, Bumper 10 ms, Ratsche 8 ms) in
// schneller Folge ueberhaupt trennt.

typedef enum {
  HapGeiger = 0,   // Naehe-Ticks unter der Fingerkuppe, darf jederzeit entfallen
  HapTick,         // Ratsche, Slingshot
  HapEvent,        // Flipper, Bumper
  HapDrain,        // Abfluss: wartet, bis der Motor frei ist
} HapPrio;

typedef struct {
  uint32_t calls;       // tatsaechlich abgesetzte Muster
  uint32_t dropped;     // verworfen, weil der Motor belegt war
  uint32_t queued;      // nachgeholt, weil sie warten durften
  uint32_t geiger;      // Ticks des Naehe-Geigers
  uint32_t min_gap_ms;  // kuerzester Abstand zweier abgesetzter Impulse
  uint32_t busy_ms_max; // laengste Wartezeit eines nachgeholten Impulses
} HapStats;

void haptics_init(void);
void haptics_reset_stats(void);
// Einzelimpuls. Gibt true zurueck, wenn er abgesetzt oder eingereiht wurde.
bool haptics_pulse(uint32_t len_ms, HapPrio prio);
// Naehe-Geiger: Abstand in ms, 0 schaltet ihn ab.
void haptics_geiger(uint32_t period_ms);
void haptics_tick(uint32_t now_ms);
void haptics_stop(void);
// Zeitpunkt des letzten Vibrationsaufrufs: die Nudge-Erkennung maskiert damit
// die eigenen Impulse.
uint32_t haptics_last_call_ms(void);
const HapStats *haptics_stats(void);
