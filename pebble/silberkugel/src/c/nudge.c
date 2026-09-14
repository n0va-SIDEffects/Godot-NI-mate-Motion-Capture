#include "nudge.h"
#include "config.h"
#include "e1clock.h"
#include "haptics.h"

static NudgeState s_st;
static int32_t s_avg_x, s_avg_y;      // gleitender Mittelwert (Tiefpass), mg * 16
static int32_t s_neutral_x, s_neutral_y;
static bool s_have_avg;
static uint32_t s_cal_until;
static int32_t s_cal_sum_x, s_cal_sum_y, s_cal_n;
static uint32_t s_button_mask_until;
static uint32_t s_last_nudge_ms;
static fix s_pend_dvx, s_pend_dvy;
static uint32_t s_last_tick_ms;

// Achsen der Uhr: +X nach rechts, +Y zum oberen Bildschirmrand, +Z aus dem
// Glas heraus. Der Tisch rechnet mit y nach unten, deshalb wird Y gespiegelt.
// Z bleibt unbenutzt: auf dem Glas liegt beim Spielen der Zeigefinger.

// Rechnet den Hochpassbetrag in einen Stoss auf die Kugeln um: Bei der
// doppelten Nudge-Schwelle ist es genau NUDGE_IMPULSE_PX_S, darueber linear
// weiter bis zum Deckel. Beides in Pixeln pro Sekunde.
static fix prv_mg_to_dv(int32_t mg) {
  int32_t sign = mg < 0 ? -1 : 1;
  int32_t a = mg < 0 ? -mg : mg;
  int32_t px_s = (a * NUDGE_IMPULSE_PX_S) / (2 * NUDGE_THRESH_MG);
  if (px_s > NUDGE_IMPULSE_MAX_PX_S) {
    px_s = NUDGE_IMPULSE_MAX_PX_S;
  }
  return (fix)(sign * FX_FROM_INT(px_s));
}

static void prv_apply_impulse(int32_t hp_x, int32_t hp_y) {
  // Traegheit: Wer den Automaten nach rechts stoesst, schiebt die Kugel
  // relativ nach links. Deshalb das Minus.
  int32_t mag = (hp_x < 0 ? -hp_x : hp_x) + (hp_y < 0 ? -hp_y : hp_y);
  s_pend_dvx -= prv_mg_to_dv(hp_x);
  s_pend_dvy += prv_mg_to_dv(hp_y);
  s_st.nudges++;
  int32_t bob = mag / TILT_BOB_PER_NUDGE_DIV;
  if (bob > TILT_BOB_NUDGE_MAX) {
    bob = TILT_BOB_NUDGE_MAX;
  }
  s_st.bob += bob;
  if (s_st.bob > TILT_BOB_MAX) {
    s_st.bob = TILT_BOB_MAX;
  }
}

static void prv_accel(AccelData *data, uint32_t num) {
  uint32_t now = e1clock_now_ms();
  s_st.batches++;
  if (num > s_st.batch_max) {
    s_st.batch_max = num;
  }
  bool vib_mask = (now - haptics_last_call_ms()) < VIB_MASK_MS;
  bool btn_mask = (int32_t)(now - s_button_mask_until) < 0;
  for (uint32_t i = 0; i < num; i++) {
    s_st.samples++;
    int32_t x = data[i].x;
    int32_t y = data[i].y;
    // Tiefpass laeuft immer mit, auch waehrend der Maske: Die Lage der Uhr
    // aendert sich durch eine Vibration nicht, nur die Spitzen tun das.
    if (!s_have_avg) {
      s_avg_x = x * 16;
      s_avg_y = y * 16;
      s_have_avg = true;
    } else {
      s_avg_x += (x * 16 - s_avg_x) / 16;   // Zeitkonstante rund 320 ms bei 50 Hz
      s_avg_y += (y * 16 - s_avg_y) / 16;
    }
    if (s_cal_n >= 0 && (int32_t)(now - s_cal_until) < 0) {
      s_cal_sum_x += x;
      s_cal_sum_y += y;
      s_cal_n++;
      continue;
    }
    if (s_cal_n > 0) {
      s_neutral_x = s_cal_sum_x / s_cal_n;
      s_neutral_y = s_cal_sum_y / s_cal_n;
      s_cal_n = 0;
    }
    if (data[i].did_vibrate) {
      s_st.masked_flag++;
      continue;
    }
    if (vib_mask) {
      s_st.masked_vib++;
      continue;
    }
    if (btn_mask) {
      s_st.masked_button++;
      continue;
    }
    int32_t hp_x = x - s_avg_x / 16;
    int32_t hp_y = y - s_avg_y / 16;
    s_st.hp_x_mg = (int16_t)hp_x;
    s_st.hp_y_mg = (int16_t)hp_y;
    int32_t amp = (hp_x < 0 ? -hp_x : hp_x);
    int32_t ampy = (hp_y < 0 ? -hp_y : hp_y);
    if (ampy > amp) {
      amp = ampy;
    }
    if (amp > s_st.peak_hp_mg) {
      s_st.peak_hp_mg = (int16_t)amp;
    }
    if (amp >= NUDGE_THRESH_MG && (now - s_last_nudge_ms) >= NUDGE_COOLDOWN_MS) {
      s_last_nudge_ms = now;
      prv_apply_impulse(hp_x, hp_y);
    }
  }
  // Neigung: Tiefpass minus Neutrallage, begrenzt.
  int32_t lx = s_avg_x / 16 - s_neutral_x;
  int32_t ly = s_avg_y / 16 - s_neutral_y;
  if (lx > TILT_LEAN_MAX_MG) lx = TILT_LEAN_MAX_MG;
  if (lx < -TILT_LEAN_MAX_MG) lx = -TILT_LEAN_MAX_MG;
  if (ly > TILT_LEAN_MAX_MG) ly = TILT_LEAN_MAX_MG;
  if (ly < -TILT_LEAN_MAX_MG) ly = -TILT_LEAN_MAX_MG;
  s_st.lean_x_mg = (int16_t)lx;
  s_st.lean_y_mg = (int16_t)ly;
}

static void prv_tap(AccelAxisType axis, int32_t direction) {
  uint32_t now = e1clock_now_ms();
  if ((now - haptics_last_call_ms()) < VIB_MASK_MS) {
    s_st.masked_vib++;
    return;   // der eigene LRA loest Taps aus
  }
  if ((int32_t)(now - s_button_mask_until) < 0) {
    s_st.masked_button++;
    return;   // ein Tastendruck ebenfalls
  }
  if (axis == ACCEL_AXIS_Z) {
    return;   // Glas: das ist meist der Zeigefinger, kein Klaps
  }
  s_st.taps++;
  fix kick = FX_FROM_INT(TAP_KICK_PX_S);
  if (axis == ACCEL_AXIS_X) {
    s_pend_dvx -= direction > 0 ? kick : -kick;
  } else {
    s_pend_dvy += direction > 0 ? kick : -kick;
  }
  s_st.bob += TILT_TAP_COST;
  if (s_st.bob > TILT_BOB_MAX) {
    s_st.bob = TILT_BOB_MAX;
  }
}

void nudge_init(void) {
  memset(&s_st, 0, sizeof(s_st));
  s_have_avg = false;
  s_neutral_x = 0;
  s_neutral_y = -1000;    // Uhr am Handgelenk, Glas nach oben: Y zeigt nach unten
  s_cal_n = 0;
  s_pend_dvx = 0;
  s_pend_dvy = 0;
  accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
  accel_data_service_subscribe(ACCEL_BATCH, prv_accel);
  accel_tap_service_subscribe(prv_tap);
}

void nudge_deinit(void) {
  accel_data_service_unsubscribe();
  accel_tap_service_unsubscribe();
}

void nudge_calibrate(void) {
  s_cal_until = e1clock_now_ms() + 500;
  s_cal_sum_x = 0;
  s_cal_sum_y = 0;
  s_cal_n = 1;   // > 0 heisst: Mittelung laeuft
}

void nudge_button_mask(void) {
  s_button_mask_until = e1clock_now_ms() + BUTTON_MASK_MS;
}

void nudge_tick(uint32_t now) {
  uint32_t dt = s_last_tick_ms ? now - s_last_tick_ms : 0;
  s_last_tick_ms = now;
  if (dt > 500) {
    dt = 500;
  }
  if (s_st.bob > 0) {
    int32_t dec = (int32_t)((TILT_BOB_MAX * (int32_t)TILT_BOB_DECAY_PCT_S * (int32_t)dt) / 100000);
    s_st.bob -= dec;
    if (s_st.bob < 0) {
      s_st.bob = 0;
    }
  }
  s_st.warn = s_st.bob >= TILT_BOB_WARN;
  if (s_st.bob >= TILT_BOB_MAX) {
    s_st.tilted = true;
  }
}

void nudge_reset_tilt(void) {
  s_st.bob = 0;
  s_st.warn = false;
  s_st.tilted = false;
}

bool nudge_take_impulse(fix *dvx, fix *dvy) {
  if (s_pend_dvx == 0 && s_pend_dvy == 0) {
    return false;
  }
  *dvx = s_pend_dvx;
  *dvy = s_pend_dvy;
  s_pend_dvx = 0;
  s_pend_dvy = 0;
  return true;
}

void nudge_gravity_offset(fix *gx, fix *gy) {
  // Neigung wirkt wie das Anheben eines Automatenbeins: ein fester Anteil der
  // Schwerkraft, hoechstens TILT_LEAN_GAIN_PCT Prozent.
  int32_t full = (GRAVITY_PX_S2 * TILT_LEAN_GAIN_PCT) / 100;   // px/s^2 bei vollem Ausschlag
  *gx = (fix)(((int64_t)s_st.lean_x_mg * full * FX_ONE) / TILT_LEAN_MAX_MG);
  *gy = (fix)((-(int64_t)s_st.lean_y_mg * full * FX_ONE) / TILT_LEAN_MAX_MG);
}

bool nudge_is_tilted(void) {
  return s_st.tilted;
}

const NudgeState *nudge_state(void) {
  return &s_st;
}
