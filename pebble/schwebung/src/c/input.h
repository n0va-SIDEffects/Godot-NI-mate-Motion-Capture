#pragma once
#include <pebble.h>

// Touch als Trackpad: vertikales Ziehen aendert die Tonhoehe relativ,
// Dead Zone gegen Ruhe-Rauschen, Kupplung bei stillem Finger, Tastenmaske,
// dazu Statistik fuer Ereignisrate und Jitter (Etappe-1-Messung).
typedef struct {
  uint32_t events;
  uint32_t touchdowns;
  uint32_t moves;
  uint32_t ev_rate_x10;      // Ereignisse pro Sekunde * 10
  uint32_t jitter_x10_px;    // mittlere Ruheabweichung * 10
  uint16_t jitter_max_px;
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
