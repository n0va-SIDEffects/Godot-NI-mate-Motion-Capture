#include "input.h"
#include "config.h"
#include "e1clock.h"

static TouchStats s_st;
static int32_t s_fork_chz = FORK_START_CHZ;
static bool s_down;
static bool s_clutch;
static int16_t s_last_y;
static int16_t s_anchor_x;
static int16_t s_anchor_y;
static uint32_t s_last_t;
static uint32_t s_last_move_t;
static int32_t s_vel_px_s;
static uint32_t s_freeze_until;
static uint32_t s_rate_t0;
static uint32_t s_rate_n0;
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
      uint32_t dt = now - s_last_t;
      if (dt > 0 && (s_st.min_interval_ms == 0 || dt < s_st.min_interval_ms)) {
        s_st.min_interval_ms = dt;
      }
      if (dt == 0) {
        dt = 1;
      }
      int32_t dy = (int32_t)e->y - (int32_t)s_last_y;
      int32_t ady = dy < 0 ? -dy : dy;
      if (s_clutch) {
        // Rohes Ruhe-Rauschen relativ zum Ankerpunkt, ungedeckelt
        int32_t rx = (int32_t)e->x - (int32_t)s_anchor_x;
        int32_t ry = (int32_t)e->y - (int32_t)s_anchor_y;
        if (rx < 0) rx = -rx;
        if (ry < 0) ry = -ry;
        int32_t r = rx > ry ? rx : ry;
        if (r > (int32_t)s_st.still_max_px) {
          s_st.still_max_px = (uint16_t)r;
        }
        if (ady > PAN_DEAD_ZONE_PX) {
          s_st.dz_exceed++;
        }
      }
      if (ady <= PAN_DEAD_ZONE_PX) {
        s_jit_sum_x10 += (uint32_t)ady * 10;
        s_jit_n++;
        s_last_t = now;
        break;
      }
      if ((int32_t)(now - s_freeze_until) < 0) {
        // Tastenmaske: Delta verwerfen
        s_last_y = e->y;
        s_last_t = now;
        break;
      }
      if (s_clutch && ady < 2 * PAN_DEAD_ZONE_PX) {
        s_last_t = now;
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
  s_rate_t0 = e1clock_now_ms();
  s_rate_n0 = 0;
  window_set_touch_bridge_disabled(window, true);
  touch_service_subscribe(prv_touch, NULL);
}

void input_deinit(void) {
  touch_service_unsubscribe();
}

void input_tick(uint32_t now) {
  if (s_down && !s_clutch && (now - s_last_move_t) > CLUTCH_MS) {
    s_clutch = true;
    s_anchor_x = s_st.x;
    s_anchor_y = s_st.y;
  }
  uint32_t el = now - s_rate_t0;
  if (el >= 1000) {
    s_st.ev_rate_x10 = ((s_st.events - s_rate_n0) * 10000) / el;
    s_rate_n0 = s_st.events;
    s_rate_t0 = now;
  }
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
