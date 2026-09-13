#include "input.h"
#include "config.h"
#include "e1clock.h"

static TouchStats s_st;
static int32_t s_fork_chz = FORK_START_CHZ;
static bool s_down;
static bool s_clutch;
static int16_t s_last_y;
static uint32_t s_last_t;
static uint32_t s_last_move_t;
static int32_t s_vel_px_s;
static uint32_t s_freeze_until;
static uint32_t s_ev_times[64];
static uint8_t s_ev_head;
static uint32_t s_jit_sum_x10;
static uint32_t s_jit_n;

static void prv_clamp(void) {
  if (s_fork_chz < FORK_MIN_CHZ) {
    s_fork_chz = FORK_MIN_CHZ;
  } else if (s_fork_chz > FORK_MAX_CHZ) {
    s_fork_chz = FORK_MAX_CHZ;
  }
}

static void prv_touch(const TouchEvent *e, void *ctx) {
  uint32_t now = e1clock_now_ms();
  s_st.events++;
  s_ev_times[s_ev_head++ & 63] = now;
  s_st.x = e->x;
  s_st.y = e->y;
  switch (e->type) {
    case TouchEvent_Touchdown:
      s_down = true;
      s_clutch = false;
      s_last_y = e->y;
      s_last_t = now;
      s_last_move_t = now;
      s_vel_px_s = 0;
      s_st.touchdowns++;
      break;
    case TouchEvent_Liftoff:
      s_down = false;
      s_clutch = false;
      break;
    case TouchEvent_PositionUpdate: {
      if (!s_down) {
        s_down = true;
        s_last_y = e->y;
        s_last_t = now;
        s_last_move_t = now;
      }
      int32_t dy = (int32_t)e->y - (int32_t)s_last_y;
      int32_t ady = dy < 0 ? -dy : dy;
      uint32_t dt = now - s_last_t;
      if (dt == 0) {
        dt = 1;
      }
      if (ady <= PAN_DEAD_ZONE_PX) {
        // Finger liegt: Rauschen messen, Tonhoehe nicht bewegen
        s_jit_sum_x10 += (uint32_t)ady * 10;
        s_jit_n++;
        if (ady > (int32_t)s_st.jitter_max_px) {
          s_st.jitter_max_px = (uint16_t)ady;
        }
        break;
      }
      if ((int32_t)(now - s_freeze_until) < 0) {
        // Tastenmaske: Delta verwerfen
        s_last_y = e->y;
        s_last_t = now;
        break;
      }
      if (s_clutch && ady < 2 * PAN_DEAD_ZONE_PX) {
        break;
      }
      s_clutch = false;
      int32_t v = (ady * 1000) / (int32_t)dt;
      s_vel_px_s = (s_vel_px_s * 3 + v) / 4;
      int32_t vv = s_vel_px_s > PAN_FAST_AT_PX_S ? PAN_FAST_AT_PX_S : s_vel_px_s;
      int32_t gain = PAN_SLOW_CHZ_PER_PX +
                     ((PAN_FAST_CHZ_PER_PX - PAN_SLOW_CHZ_PER_PX) * vv) / PAN_FAST_AT_PX_S;
      s_fork_chz -= dy * gain;   // nach oben ziehen = hoeher
      prv_clamp();
      s_last_y = e->y;
      s_last_t = now;
      s_last_move_t = now;
      s_st.moves++;
      break;
    }
  }
}

void input_init(Window *window) {
  s_st = (TouchStats){ 0 };
  s_fork_chz = FORK_START_CHZ;
  s_down = false;
  s_clutch = false;
  s_jit_sum_x10 = 0;
  s_jit_n = 0;
  window_set_touch_bridge_disabled(window, true);
  touch_service_subscribe(prv_touch, NULL);
}

void input_deinit(void) {
  touch_service_unsubscribe();
}

void input_tick(uint32_t now) {
  if (s_down && !s_clutch && (now - s_last_move_t) > CLUTCH_MS) {
    s_clutch = true;
  }
  uint32_t n = 0;
  for (int i = 0; i < 64; i++) {
    uint32_t t = s_ev_times[i];
    if (t != 0 && (now - t) <= 1000) {
      n++;
    }
  }
  s_st.ev_rate_x10 = n * 10;
  s_st.jitter_x10_px = s_jit_n ? s_jit_sum_x10 / s_jit_n : 0;
}

bool input_finger_down(void) {
  return s_down;
}

bool input_clutch_engaged(void) {
  return s_clutch;
}

bool input_touch_available(void) {
  return touch_service_is_enabled();
}

int32_t input_fork_chz(void) {
  return s_fork_chz;
}

void input_nudge_chz(int32_t delta_chz) {
  s_fork_chz += delta_chz;
  prv_clamp();
}

void input_button_pressed(void) {
  s_freeze_until = e1clock_now_ms() + BUTTON_FREEZE_MS;
}

const TouchStats *input_stats(void) {
  return &s_st;
}
