#include "backlight.h"
#include "config.h"

static bool s_enabled;
static int32_t s_fork_chz = FORK_START_CHZ;
static int32_t s_flower_chz = 44000;
static int32_t s_beat_chz;
static bool s_in_window;
static bool s_near;
static bool s_locked;
static uint32_t s_last_apply_ms;
static uint32_t s_last_rgb;
static uint32_t s_calls;
static int32_t s_hue_deg;
static uint8_t s_test_step = 5;
static uint32_t s_test_start_ms;
static uint32_t s_test_last_log_ms;

// Quintenzirkel-Farbkreis: C rot, G orange, D gelb, A gruen, E tuerkis, H blau ...
static const int16_t s_hue_by_pc[12] = { 0, 210, 60, 270, 120, 330, 180, 30, 240, 90, 300, 150 };
// 2^(i/12) in 16.16
static const uint32_t s_semi16[13] = { 65536, 69433, 73562, 77936, 82570, 87480, 92682,
                                       98193, 104032, 110218, 116772, 123715, 131072 };

static uint32_t prv_hsv_to_rgb(int32_t hue_deg, uint8_t v) {
  while (hue_deg < 0) {
    hue_deg += 360;
  }
  hue_deg %= 360;
  int32_t sector = hue_deg / 60;
  int32_t f = ((hue_deg % 60) * 255) / 60;
  uint32_t p = 0;
  uint32_t q = (uint32_t)((255 - f) * v / 255);
  uint32_t t = (uint32_t)(f * v / 255);
  uint32_t r, g, b;
  switch (sector) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
  return (r << 16) | (g << 8) | b;
}

// Tonklasse plus Bruchteil aus einer Frequenz; liefert Farbton in Grad.
static int32_t prv_hue_from_chz(int32_t chz) {
  if (chz < 1000) {
    chz = 1000;
  }
  uint32_t r16 = (uint32_t)(((uint64_t)(uint32_t)chz << 16) / 44000u);   // relativ zu A4
  while (r16 >= (2u << 16)) {
    r16 >>= 1;
  }
  while (r16 < (1u << 16)) {
    r16 <<= 1;
  }
  int i = 0;
  while (i < 11 && r16 >= s_semi16[i + 1]) {
    i++;
  }
  uint32_t span = s_semi16[i + 1] - s_semi16[i];
  int32_t frac = (int32_t)(((r16 - s_semi16[i]) * 256u) / span);   // 0..255
  int pc = (9 + i) % 12;                                              // A = 9
  int32_t h0 = s_hue_by_pc[pc];
  int32_t h1 = s_hue_by_pc[(pc + 1) % 12];
  int32_t d = h1 - h0;
  if (d > 180) {
    d -= 360;
  } else if (d < -180) {
    d += 360;
  }
  return h0 + (d * frac) / 256;
}

static void prv_apply(uint32_t rgb, uint32_t now, bool force) {
  int32_t dr = (int32_t)((rgb >> 16) & 0xFF) - (int32_t)((s_last_rgb >> 16) & 0xFF);
  int32_t dg = (int32_t)((rgb >> 8) & 0xFF) - (int32_t)((s_last_rgb >> 8) & 0xFF);
  int32_t db = (int32_t)(rgb & 0xFF) - (int32_t)(s_last_rgb & 0xFF);
  if (dr < 0) dr = -dr;
  if (dg < 0) dg = -dg;
  if (db < 0) db = -db;
  int32_t maxd = dr > dg ? dr : dg;
  if (db > maxd) maxd = db;
  if (!force && maxd < BL_HYST && (now - s_last_apply_ms) < BL_KEEPALIVE_MS) {
    return;
  }
  light_set_color_rgb888(rgb);
  s_last_rgb = rgb;
  s_last_apply_ms = now;
  s_calls++;
}

void backlight_init(void) {
  s_enabled = false;
  s_calls = 0;
  s_last_rgb = 0xFFFFFFFF;
  s_test_step = 5;
}

void backlight_enable(bool on) {
  if (on == s_enabled) {
    return;
  }
  s_enabled = on;
  light_enable(on);
  if (!on) {
    light_set_system_color();
  }
  s_last_rgb = 0xFFFFFFFF;
}

void backlight_set_pitch(int32_t fork_chz, int32_t flower_chz) {
  s_fork_chz = fork_chz;
  s_flower_chz = flower_chz;
}

void backlight_set_state(int32_t beat_chz, bool in_window, bool near, bool locked) {
  s_beat_chz = beat_chz;
  s_in_window = in_window;
  s_near = near;
  s_locked = locked;
}

void backlight_tick(uint32_t now) {
  if (!s_enabled || s_test_step != 5) {
    return;
  }
  if ((now - s_last_apply_ms) < BL_UPDATE_MS) {
    return;
  }
  s_hue_deg = prv_hue_from_chz(s_locked ? s_flower_chz : s_fork_chz);
  uint8_t v;
  if (s_locked) {
    v = 255;
  } else if (s_near && s_beat_chz >= LOCK_CHZ && s_beat_chz <= BL_BREATH_MAX_CHZ) {
    // |cos(pi f t)|: Periode 1/f
    uint32_t angle = (uint32_t)(((uint64_t)(uint32_t)s_beat_chz * now * TRIG_MAX_ANGLE) / 200000u);
    int32_t c = cos_lookup((int32_t)(angle & (TRIG_MAX_ANGLE - 1)));
    if (c < 0) {
      c = -c;
    }
    v = (uint8_t)(77 + (178 * c) / TRIG_MAX_RATIO);
  } else if (s_near) {
    v = 230;
  } else if (s_in_window) {
    v = 200;
  } else {
    v = 150;
  }
  prv_apply(prv_hsv_to_rgb(s_hue_deg, v), now, false);
}

void backlight_test_set_step(uint8_t step) {
  s_test_step = step;
  s_test_start_ms = 0;
  s_test_last_log_ms = 0;
  if (step <= 4) {
    backlight_enable(true);
  } else {
    backlight_enable(false);
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "[E1][LICHT] Schritt %u (%s)", (unsigned)step,
          step <= 3 ? "Atmen" : (step == 4 ? "Dimmrampe" : "aus"));
}

void backlight_test_tick(uint32_t now) {
  if (s_test_step > 4) {
    return;
  }
  if (s_test_start_ms == 0) {
    s_test_start_ms = now;
  }
  if ((now - s_last_apply_ms) < BL_UPDATE_MS) {
    return;
  }
  uint32_t el = now - s_test_start_ms;
  if (s_test_step <= 3) {
    uint32_t hz = (uint32_t)s_test_step + 1;
    uint32_t angle = (hz * el * (TRIG_MAX_ANGLE / 2)) / 1000;   // pi*f*t
    int32_t c = cos_lookup((int32_t)(angle & (TRIG_MAX_ANGLE - 1)));
    if (c < 0) {
      c = -c;
    }
    uint8_t v = (uint8_t)(77 + (178 * c) / TRIG_MAX_RATIO);
    prv_apply(prv_hsv_to_rgb(35, v), now, false);
  } else {
    static const uint8_t levels[8] = { 255, 192, 144, 108, 80, 60, 44, 32 };
    uint32_t idx = (el / 2000) % 8;
    uint32_t rgb = prv_hsv_to_rgb(35, levels[idx]);
    if (s_test_last_log_ms == 0 || (now - s_test_last_log_ms) >= 2000) {
      s_test_last_log_ms = now;
      APP_LOG(APP_LOG_LEVEL_INFO, "[E1][LICHT] Dimmstufe %lu: rgb=%06lx", (unsigned long)idx,
              (unsigned long)rgb);
    }
    prv_apply(rgb, now, false);
  }
}

uint32_t backlight_call_count(void) {
  return s_calls;
}

uint32_t backlight_last_rgb(void) {
  return s_last_rgb;
}

int32_t backlight_hue_deg(void) {
  return s_hue_deg;
}

void backlight_refresh(void) {
  if (!s_enabled) {
    return;
  }
  light_enable(true);
  if (s_last_rgb != 0xFFFFFFFF) {
    light_set_color_rgb888(s_last_rgb);
    s_calls++;
  }
}
