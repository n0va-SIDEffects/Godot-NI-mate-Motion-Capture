#include "settings.h"

#define SETTINGS_KEY     1
#define SETTINGS_VERSION 1

static PtzSettings s_settings;

static void set_defaults(void) {
  s_settings.speed        = 3;
  s_settings.gyro_sens    = 5;
  s_settings.gyro_dead    = 60;
  s_settings.invert_pan   = false;
  s_settings.invert_tilt  = false;
  s_settings.vibrate      = true;
  s_settings.preset_count = 8;
  s_settings.gyro_axes    = GYRO_AXES_PAN_TILT;
  s_settings.active_cam   = 0;
  s_settings.active_axis  = PTZ_AXIS_TILT;
}

static uint8_t clamp_u8(int32_t v, uint8_t lo, uint8_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return (uint8_t)v;
}

void settings_load(void) {
  set_defaults();
  if (persist_exists(SETTINGS_KEY)) {
    PtzSettings stored;
    int read = persist_read_data(SETTINGS_KEY, &stored, sizeof(stored));
    if (read == sizeof(stored)) {
      s_settings = stored;
      // Gegen beschaedigte oder aeltere Datensaetze absichern.
      s_settings.speed        = clamp_u8(s_settings.speed, 1, 5);
      s_settings.gyro_sens    = clamp_u8(s_settings.gyro_sens, 1, 10);
      s_settings.gyro_dead    = clamp_u8(s_settings.gyro_dead, 20, 200);
      s_settings.preset_count = clamp_u8(s_settings.preset_count, 1, PTZ_MAX_PRESETS);
      s_settings.gyro_axes    = clamp_u8(s_settings.gyro_axes, 0, GYRO_AXES_COUNT - 1);
      s_settings.active_cam   = clamp_u8(s_settings.active_cam, 0, PTZ_MAX_CAMERAS - 1);
      s_settings.active_axis  = clamp_u8(s_settings.active_axis, 0, PTZ_AXIS_COUNT - 1);
    }
  }
}

void settings_save(void) {
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

PtzSettings *settings_get(void) {
  return &s_settings;
}

bool settings_apply_message(DictionaryIterator *iter) {
  bool changed = false;
  Tuple *t;

  if ((t = dict_find(iter, MESSAGE_KEY_cfgSpeed))) {
    s_settings.speed = clamp_u8(t->value->int32, 1, 5);
    changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_cfgGyroSens))) {
    s_settings.gyro_sens = clamp_u8(t->value->int32, 1, 10);
    changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_cfgGyroDead))) {
    s_settings.gyro_dead = clamp_u8(t->value->int32, 20, 200);
    changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_cfgInvertPan))) {
    s_settings.invert_pan = t->value->int32 != 0;
    changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_cfgInvertTilt))) {
    s_settings.invert_tilt = t->value->int32 != 0;
    changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_cfgVibrate))) {
    s_settings.vibrate = t->value->int32 != 0;
    changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_cfgPresetCount))) {
    s_settings.preset_count = clamp_u8(t->value->int32, 1, PTZ_MAX_PRESETS);
    changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_cfgGyroLock))) {
    s_settings.gyro_axes = clamp_u8(t->value->int32, 0, GYRO_AXES_COUNT - 1);
    changed = true;
  }

  if (changed) {
    settings_save();
  }
  return changed;
}

uint8_t settings_speed_limit(void) {
  // Stufe 1..5 auf einen Maximalausschlag abbilden. Stufe 1 bleibt betont
  // langsam, damit sich Bildausschnitte im laufenden Betrieb fein anlegen
  // lassen; Stufe 5 nutzt den vollen Bereich der Kamera.
  static const uint8_t limits[] = { 8, 16, 26, 36, PTZ_SPEED_MAX };
  uint8_t idx = s_settings.speed;
  if (idx < 1) idx = 1;
  if (idx > 5) idx = 5;
  return limits[idx - 1];
}
