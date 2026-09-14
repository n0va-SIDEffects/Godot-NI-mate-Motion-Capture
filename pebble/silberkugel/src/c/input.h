#pragma once
#include <pebble.h>
#include "fixed.h"

// Touch fuer den Flipper: Der Zeigefinger ist der Magnet, eine senkrechte
// Zieh-Geste in der Abschussbahn ist der Plunger. Rohevents, kein Recognizer:
// Das TouchEvent traegt keinen Zeitstempel, deshalb wird die Zeit beim
// Eingang selbst genommen (e1clock, nicht time_ms).

typedef struct {
  bool down;
  int16_t x;
  int16_t y;
  uint32_t events;
  uint32_t touchdowns;
  uint32_t liftoffs;
  uint32_t moves;
  uint32_t ev_rate_x10;       // Ereignisse pro Sekunde * 10
  uint32_t min_interval_ms;   // kuerzester Abstand zweier Positionsereignisse
  uint32_t max_interval_ms;   // groesster Abstand bei liegendem Finger
  uint32_t stale_drops;       // Finger ohne Liftoff verloren (Zeitablauf)
  int16_t vel_px_s_x;         // aus den letzten Positionen, fuer den Notwurf
  int16_t vel_px_s_y;
} TouchState;

void input_init(Window *window);
void input_deinit(void);
void input_tick(uint32_t now_ms);
bool input_touch_available(void);

// Magnetfinger
bool input_finger(int16_t *x, int16_t *y);
void input_finger_velocity(fix *vx, fix *vy);

// Plunger: senkrechtes Ziehen in der Abschussbahn
bool input_plunger_active(void);
int16_t input_plunger_pull_px(void);
uint8_t input_plunger_ticks(void);
uint8_t input_take_ratchet(void);         // neue Ratschenimpulse seit dem letzten Abruf
bool input_take_plunger_release(int16_t *pull_px, uint8_t *ticks);

const TouchState *input_state(void);
