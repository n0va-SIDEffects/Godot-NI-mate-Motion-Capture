#include "motion.h"

#define SAMPLE_RATE      ACCEL_SAMPLING_50HZ
#define SAMPLES_PER_CB   5           /* 50 Hz / 5 = update every 100 ms */
#define FILTER_SHIFT     2           /* EMA alpha = 1/4 */

static struct {
  bool active;
  bool calibrate_pending;
  MotionUpdateHandler handler;
  int32_t fx, fy, fz;     /* low-pass filtered acceleration (milli-g) */
  int32_t nx, ny;         /* neutral orientation */
  bool have_filter;
} s_motion;

/* Full deflection (100) is reached at this many milli-g of tilt away
 * from neutral. 1000 mg ~ 90 degrees. Sensitivity 1 -> ~45 deg,
 * sensitivity 10 -> ~9 deg. */
static int32_t prv_full_scale_mg(void) {
  uint8_t s = g_app.settings.sensitivity;
  if (s < 1) s = 1;
  if (s > 10) s = 10;
  return 780 - (int32_t)s * 62;
}

static int8_t prv_map(int32_t delta_mg) {
  int32_t full = prv_full_scale_mg();
  int32_t v = delta_mg * 100 / full;           /* linear, -inf..inf */
  int32_t sign = v < 0 ? -1 : 1;
  int32_t mag = v < 0 ? -v : v;
  if (mag > 100) mag = 100;

  /* dead zone, then rescale so full deflection is still 100 */
  int32_t dz = g_app.settings.deadzone;
  if (mag <= dz) return 0;
  mag = (mag - dz) * 100 / (100 - dz);

  /* mild expo curve: fine control near the centre, full speed at the edge */
  mag = (mag + (mag * mag) / 100) / 2;
  if (mag > 100) mag = 100;
  return (int8_t)(sign * mag);
}

static void prv_accel_handler(AccelData *data, uint32_t num_samples) {
  int32_t sx = 0, sy = 0, sz = 0;
  uint32_t used = 0;
  for (uint32_t i = 0; i < num_samples; i++) {
    if (data[i].did_vibrate) continue;   /* vibration corrupts the reading */
    sx += data[i].x;
    sy += data[i].y;
    sz += data[i].z;
    used++;
  }
  if (used == 0) return;
  sx /= (int32_t)used;
  sy /= (int32_t)used;
  sz /= (int32_t)used;

  if (!s_motion.have_filter) {
    s_motion.fx = sx;
    s_motion.fy = sy;
    s_motion.fz = sz;
    s_motion.have_filter = true;
  } else {
    s_motion.fx += (sx - s_motion.fx) >> FILTER_SHIFT;
    s_motion.fy += (sy - s_motion.fy) >> FILTER_SHIFT;
    s_motion.fz += (sz - s_motion.fz) >> FILTER_SHIFT;
  }

  if (s_motion.calibrate_pending) {
    s_motion.nx = s_motion.fx;
    s_motion.ny = s_motion.fy;
    s_motion.calibrate_pending = false;
  }

  if (s_motion.handler) {
    int8_t x = prv_map(s_motion.fx - s_motion.nx);
    int8_t y = prv_map(s_motion.fy - s_motion.ny);
    s_motion.handler(x, y);
  }
}

void motion_init(void) {
  memset(&s_motion, 0, sizeof(s_motion));
}

void motion_deinit(void) {
  motion_set_active(false, NULL);
}

void motion_set_active(bool active, MotionUpdateHandler handler) {
  if (active && !s_motion.active) {
    s_motion.handler = handler;
    s_motion.have_filter = false;
    s_motion.calibrate_pending = true;
    accel_data_service_subscribe(SAMPLES_PER_CB, prv_accel_handler);
    accel_service_set_sampling_rate(SAMPLE_RATE);
    s_motion.active = true;
  } else if (!active && s_motion.active) {
    accel_data_service_unsubscribe();
    s_motion.active = false;
    s_motion.handler = NULL;
  } else if (active) {
    s_motion.handler = handler;
  }
}

bool motion_is_active(void) {
  return s_motion.active;
}

void motion_calibrate(void) {
  if (s_motion.have_filter) {
    s_motion.nx = s_motion.fx;
    s_motion.ny = s_motion.fy;
  } else {
    s_motion.calibrate_pending = true;
  }
}
