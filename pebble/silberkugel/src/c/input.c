#include "input.h"
#include "config.h"
#include "e1clock.h"

#define VEL_HIST 3

static TouchState s_st;
static uint32_t s_last_ev_ms;
static uint32_t s_rate_t0;
static uint32_t s_rate_n0;

static int16_t s_hist_x[VEL_HIST];
static int16_t s_hist_y[VEL_HIST];
static uint32_t s_hist_t[VEL_HIST];
static uint8_t s_hist_n;

// Plunger
static bool s_pl_active;
static int16_t s_pl_start_y;
static int16_t s_pl_pull;
static uint8_t s_pl_ticks;
static uint8_t s_pl_new_ticks;
static bool s_pl_released;
static int16_t s_pl_rel_pull;
static uint8_t s_pl_rel_ticks;

static void prv_hist_push(int16_t x, int16_t y, uint32_t t) {
  for (uint8_t i = VEL_HIST - 1; i > 0; i--) {
    s_hist_x[i] = s_hist_x[i - 1];
    s_hist_y[i] = s_hist_y[i - 1];
    s_hist_t[i] = s_hist_t[i - 1];
  }
  s_hist_x[0] = x;
  s_hist_y[0] = y;
  s_hist_t[0] = t;
  if (s_hist_n < VEL_HIST) {
    s_hist_n++;
  }
}

static void prv_update_velocity(void) {
  if (s_hist_n < 2) {
    s_st.vel_px_s_x = 0;
    s_st.vel_px_s_y = 0;
    return;
  }
  uint8_t last = (uint8_t)(s_hist_n - 1);
  int32_t dt = (int32_t)(s_hist_t[0] - s_hist_t[last]);
  if (dt <= 0) {
    return;
  }
  s_st.vel_px_s_x = (int16_t)(((int32_t)(s_hist_x[0] - s_hist_x[last]) * 1000) / dt);
  s_st.vel_px_s_y = (int16_t)(((int32_t)(s_hist_y[0] - s_hist_y[last]) * 1000) / dt);
}

static void prv_plunger_begin(int16_t x, int16_t y) {
  if (x >= PLUNGER_LANE_X0 && y >= PLUNGER_LANE_Y0) {
    s_pl_active = true;
    s_pl_start_y = y;
    s_pl_pull = 0;
    s_pl_ticks = 0;
  }
}

static void prv_plunger_update(int16_t y) {
  if (!s_pl_active) {
    return;
  }
  int32_t pull = y - s_pl_start_y;      // nach unten ziehen = positiv
  if (pull < 0) {
    pull = 0;
  }
  if (pull > PLUNGER_PULL_MAX_PX) {
    pull = PLUNGER_PULL_MAX_PX;
  }
  s_pl_pull = (int16_t)pull;
  uint8_t ticks = (uint8_t)(pull / PLUNGER_TICK_PX);
  if (ticks > PLUNGER_TICKS_MAX) {
    ticks = PLUNGER_TICKS_MAX;
  }
  if (ticks > s_pl_ticks) {
    // Die Ratsche zaehlt nur vorwaerts: Beim Zurueckwandern des Fingers soll
    // sie nicht doppelt ticken, sonst ist der Skill-Shot nicht zaehlbar.
    s_pl_new_ticks = (uint8_t)(s_pl_new_ticks + (ticks - s_pl_ticks));
    s_pl_ticks = ticks;
  }
}

static void prv_plunger_end(void) {
  if (!s_pl_active) {
    return;
  }
  s_pl_active = false;
  if (s_pl_pull > 0) {
    s_pl_released = true;
    s_pl_rel_pull = s_pl_pull;
    s_pl_rel_ticks = s_pl_ticks;
  }
  s_pl_pull = 0;
  s_pl_ticks = 0;
}

static void prv_touch(const TouchEvent *e, void *ctx) {
  uint32_t now = e1clock_now_ms();
  s_st.events++;
  s_st.x = e->x;
  s_st.y = e->y;
  switch (e->type) {
    case TouchEvent_Touchdown:
      s_st.down = true;
      s_st.touchdowns++;
      s_hist_n = 0;
      prv_hist_push(e->x, e->y, now);
      prv_plunger_begin(e->x, e->y);
      break;
    case TouchEvent_Liftoff:
      s_st.down = false;
      s_st.liftoffs++;
      prv_plunger_end();
      break;
    case TouchEvent_PositionUpdate: {
      if (!s_st.down) {
        // Ohne vorheriges Touchdown: als Auflegen werten, sonst haengt der
        // Magnet an einer alten Position fest.
        s_st.down = true;
        s_hist_n = 0;
        prv_plunger_begin(e->x, e->y);
      }
      uint32_t dt = now - s_last_ev_ms;
      if (s_last_ev_ms != 0 && dt > 0) {
        if (s_st.min_interval_ms == 0 || dt < s_st.min_interval_ms) {
          s_st.min_interval_ms = dt;
        }
        if (dt > s_st.max_interval_ms && dt < 2000) {
          s_st.max_interval_ms = dt;
        }
      }
      s_st.moves++;
      prv_hist_push(e->x, e->y, now);
      prv_update_velocity();
      prv_plunger_update(e->y);
      break;
    }
  }
  s_last_ev_ms = now;
}

void input_init(Window *window) {
  memset(&s_st, 0, sizeof(s_st));
  s_last_ev_ms = 0;
  s_hist_n = 0;
  s_pl_active = false;
  s_pl_released = false;
  s_rate_t0 = e1clock_now_ms();
  s_rate_n0 = 0;
  // Die System-Touch-Bruecke wuerde Wischen als Tastendruck ausliefern; der
  // Flipper braucht die Rohereignisse.
  window_set_touch_bridge_disabled(window, true);
  touch_service_subscribe(prv_touch, NULL);
}

void input_deinit(void) {
  touch_service_unsubscribe();
}

void input_tick(uint32_t now) {
  // Ein verschlucktes Liftoff wuerde den Magneten dauerhaft anlassen. Bleibt
  // ein liegender Finger laenger als TOUCH_STALE_MS still, gilt er als
  // abgehoben; der Zaehler zeigt, wie oft das passiert.
  if (s_st.down && s_last_ev_ms != 0 && (now - s_last_ev_ms) > TOUCH_STALE_MS) {
    s_st.down = false;
    s_st.stale_drops++;
    prv_plunger_end();
  }
  uint32_t el = now - s_rate_t0;
  if (el >= 1000) {
    s_st.ev_rate_x10 = ((s_st.events - s_rate_n0) * 10000) / el;
    s_rate_n0 = s_st.events;
    s_rate_t0 = now;
  }
}

bool input_touch_available(void) {
  return touch_service_is_enabled();
}

bool input_finger(int16_t *x, int16_t *y) {
  if (!s_st.down) {
    return false;
  }
  *x = s_st.x;
  *y = s_st.y;
  return true;
}

void input_finger_velocity(fix *vx, fix *vy) {
  *vx = FX_FROM_INT(s_st.vel_px_s_x);
  *vy = FX_FROM_INT(s_st.vel_px_s_y);
}

bool input_plunger_active(void) {
  return s_pl_active;
}

int16_t input_plunger_pull_px(void) {
  return s_pl_pull;
}

uint8_t input_plunger_ticks(void) {
  return s_pl_ticks;
}

uint8_t input_take_ratchet(void) {
  uint8_t n = s_pl_new_ticks;
  s_pl_new_ticks = 0;
  return n;
}

bool input_take_plunger_release(int16_t *pull_px, uint8_t *ticks) {
  if (!s_pl_released) {
    return false;
  }
  s_pl_released = false;
  *pull_px = s_pl_rel_pull;
  *ticks = s_pl_rel_ticks;
  return true;
}

const TouchState *input_state(void) {
  return &s_st;
}
