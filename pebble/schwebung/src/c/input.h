#pragma once
#include <pebble.h>

// Touch als Trackpad: vertikales Ziehen aendert die Tonhoehe relativ,
// Dead Zone gegen Ruhe-Rauschen, Kupplung bei stillem Finger, Tastenmaske,
// dazu Statistik fuer Ereignisrate, kuerzesten Ereignisabstand und rohes
// Ruhe-Rauschen (Etappe-1-Messung).
typedef struct {
  uint32_t events;
  uint32_t touchdowns;
  uint32_t moves;
  uint32_t ev_rate_x10;      // Ereignisse pro Sekunde * 10, aus dem Gesamtzaehler
  uint32_t min_interval_ms;  // kuerzester Abstand zweier Positionsereignisse
  uint32_t jitter_x10_px;    // mittlere Abweichung innerhalb der Dead Zone * 10
  uint16_t still_max_px;     // groesste rohe Abweichung vom Ankerpunkt bei eingekuppeltem Finger
  uint32_t dz_exceed;        // Ereignisse ueber der Dead Zone bei eingekuppeltem Finger
  int16_t x;
  int16_t y;
} TouchStats;

void input_init(Window *window);
void input_deinit(void);
void input_tick(uint32_t now_ms);
bool input_finger_down(void);
bool input_clutch_engaged(void);
bool input_touch_available(void);
int32_t input_fork_chz(void);
void input_nudge_chz(int32_t delta_chz);
void input_button_pressed(void);
const TouchStats *input_stats(void);
