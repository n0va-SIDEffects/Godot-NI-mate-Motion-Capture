#include "motion.h"
#include "settings.h"

static int16_t s_smooth_x, s_smooth_y;
static int16_t s_ref_x, s_ref_y;
static bool s_running;
static bool s_primed;   // erster Messwert liegt vor

// Ab welcher Neigung der volle Ausschlag erreicht ist, in Milli-g.
// Stufe 1 verlangt eine deutliche Bewegung (rund 40 Grad), Stufe 10 reagiert
// schon auf ein kurzes Kippen (rund 12 Grad).
static uint16_t full_scale(void) {
  uint8_t sens = settings_get()->gyro_sens;
  if (sens < 1) sens = 1;
  if (sens > 10) sens = 10;
  return (uint16_t)(700 - sens * 50);
}

void motion_start(void) {
  if (s_running) return;
  accel_data_service_subscribe(0, NULL);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
  s_running = true;
  s_primed = false;
}

void motion_stop(void) {
  if (!s_running) return;
  accel_data_service_unsubscribe();
  s_running = false;
}

void motion_update(void) {
  if (!s_running) return;

  AccelData d;
  if (accel_service_peek(&d) != 0) {
    return;
  }
  if (!s_primed) {
    s_smooth_x = d.x;
    s_smooth_y = d.y;
    s_primed = true;
    return;
  }
  // Gleitender Mittelwert. Ein Viertel je Schritt daempft das Zittern der
  // Hand, ohne die Steuerung traege wirken zu lassen.
  s_smooth_x += (d.x - s_smooth_x) / 4;
  s_smooth_y += (d.y - s_smooth_y) / 4;
}

void motion_set_reference(void) {
  if (!s_primed) {
    motion_update();
  }
  s_ref_x = s_smooth_x;
  s_ref_y = s_smooth_y;
}

// Ablenkung in Milli-g auf -100..+100 abbilden.
static int16_t normalize(int16_t delta) {
  uint16_t scale = full_scale();
  int32_t v = (int32_t)delta * 100 / scale;
  if (v >  100) v =  100;
  if (v < -100) v = -100;
  return (int16_t)v;
}

// Aus der Ablenkung eine Kamerageschwindigkeit machen.
static int8_t to_speed(int16_t delta, uint8_t limit, bool invert) {
  uint8_t dead = settings_get()->gyro_dead;
  uint16_t scale = full_scale();
  if (scale <= dead + 20) {
    scale = dead + 20;   // ein nutzbarer Bereich muss uebrig bleiben
  }

  int32_t a = delta < 0 ? -delta : delta;
  if (a <= dead) {
    return 0;
  }
  a -= dead;
  int32_t span = scale - dead;
  if (a > span) a = span;

  // Erst auf 0..100 bringen, dann quadrieren: kleine Neigungen ergeben
  // sehr langsame Fahrten, das Feingefuehl liegt dort, wo man es braucht.
  int32_t v = a * 100 / span;
  v = (v * v) / 100;

  int32_t speed = v * limit / 100;
  if (speed < 1) speed = 1;
  if (speed > limit) speed = limit;
  if (delta < 0) speed = -speed;
  if (invert) speed = -speed;
  return (int8_t)speed;
}

void motion_get_speeds(GyroAxes mode, uint8_t limit,
                       int8_t *pan, int8_t *tilt, int8_t *zoom) {
  PtzSettings *cfg = settings_get();
  int16_t dx = s_smooth_x - s_ref_x;   // Uhr seitlich gekippt
  int16_t dy = s_smooth_y - s_ref_y;   // Uhr nach vorn oder hinten gekippt

  int8_t p = 0, t = 0, z = 0;

  switch (mode) {
    case GYRO_AXES_PAN_TILT:
      p = to_speed(dx, limit, cfg->invert_pan);
      t = to_speed(dy, limit, cfg->invert_tilt);
      break;
    case GYRO_AXES_PAN_ZOOM:
      p = to_speed(dx, limit, cfg->invert_pan);
      z = to_speed(dy, limit, cfg->invert_tilt);
      break;
    case GYRO_AXES_TILT_ONLY:
      t = to_speed(dy, limit, cfg->invert_tilt);
      break;
    default:
      break;
  }

  if (pan)  *pan  = p;
  if (tilt) *tilt = t;
  if (zoom) *zoom = z;
}

void motion_get_deflection(int8_t *x, int8_t *y) {
  if (x) *x = (int8_t)normalize(s_smooth_x - s_ref_x);
  if (y) *y = (int8_t)normalize(s_smooth_y - s_ref_y);
}
