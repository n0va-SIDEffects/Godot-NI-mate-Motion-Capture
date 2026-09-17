#include <stdlib.h>

#include "settings.h"

// Generated from the messageKeys in package.json.
#include "message_keys.auto.h"


#define SETTINGS_KEY     10
#define SETTINGS_VERSION 6

#if defined(PBL_COLOR)
// Kept in the same order as the settings page offers them.
static const uint8_t s_palette_argb[] = {
  GColorGreenARGB8,
  GColorRedARGB8,
  GColorWhiteARGB8,
  GColorYellowARGB8,
  GColorCyanARGB8,
};
// The dimmed companion of each entry, used for the baseline.
static const uint8_t s_palette_dim_argb[] = {
  GColorDarkGreenARGB8,
  GColorDarkCandyAppleRedARGB8,
  GColorLightGrayARGB8,
  GColorLimerickARGB8,
  GColorTiffanyBlueARGB8,
};
#define PALETTE_LEN ((uint8_t)(sizeof(s_palette_argb) / sizeof(s_palette_argb[0])))
#else
#define PALETTE_LEN 5   // the setting is still stored on a black and white watch, just unused
#endif

typedef struct {
  uint8_t version;
  Settings settings;
} StoredSettings;

void settings_load(Settings *settings) {
  *settings = (Settings){
    .sound_on = true,
    .volume = 65,
    .pitch_note = 81,          // the sample's own pitch, 880 Hz
    .vibe_on = true,
    .vibe_ms = 25,
    .backlight = BacklightAuto,
    .px_ms = 20,               // 50 pixels a second
    .trace_color = 0,          // green
    .light_color = 0xFFFFFF,   // white
    .light_floor = 15,
    .demo = false,
  };
  StoredSettings stored;
  if (persist_exists(SETTINGS_KEY) &&
      persist_read_data(SETTINGS_KEY, &stored, sizeof(stored)) == (int)sizeof(stored) &&
      stored.version == SETTINGS_VERSION) {
    *settings = stored.settings;
  }
}

void settings_save(const Settings *settings) {
  const StoredSettings stored = {.version = SETTINGS_VERSION, .settings = *settings};
  persist_write_data(SETTINGS_KEY, &stored, sizeof(stored));
}

//! Read a number whatever width the phone sent it as. Returns the fallback when the key is absent.
static int32_t dict_int(DictionaryIterator *iter, uint32_t key, int32_t fallback) {
  Tuple *tuple = dict_find(iter, key);
  if (!tuple) {
    return fallback;
  }
  switch (tuple->type) {
    case TUPLE_INT:
      switch (tuple->length) {
        case 1: return tuple->value->int8;
        case 2: return tuple->value->int16;
        default: return tuple->value->int32;
      }
    case TUPLE_UINT:
      switch (tuple->length) {
        case 1: return tuple->value->uint8;
        case 2: return tuple->value->uint16;
        default: return (int32_t)tuple->value->uint32;
      }
    case TUPLE_CSTRING:
      // The settings page sends a dropdown as text unless its values are written as numbers.
      // Accepting both means a stray quotation mark in the page cannot silently drop a setting.
      return (tuple->length > 0) ? atoi(tuple->value->cstring) : fallback;
    default:
      return fallback;
  }
}

static uint8_t clamp_u8(int32_t value, int32_t low, int32_t high) {
  if (value < low) return (uint8_t)low;
  if (value > high) return (uint8_t)high;
  return (uint8_t)value;
}

void settings_read_dict(Settings *settings, DictionaryIterator *iter) {
  settings->sound_on = dict_int(iter, MESSAGE_KEY_SOUND_ON, settings->sound_on) != 0;
  settings->volume = clamp_u8(dict_int(iter, MESSAGE_KEY_VOLUME, settings->volume), 0, 100);
  settings->pitch_note = clamp_u8(dict_int(iter, MESSAGE_KEY_PITCH, settings->pitch_note), 48, 108);
  settings->vibe_on = dict_int(iter, MESSAGE_KEY_VIBE_ON, settings->vibe_on) != 0;
  settings->vibe_ms = clamp_u8(dict_int(iter, MESSAGE_KEY_VIBE_MS, settings->vibe_ms), 10, 80);
  settings->backlight = clamp_u8(dict_int(iter, MESSAGE_KEY_BACKLIGHT, settings->backlight),
                                 BacklightAuto, BacklightPulse);
  settings->light_color =
      (uint32_t)dict_int(iter, MESSAGE_KEY_LIGHT_COLOR, (int32_t)settings->light_color) & 0xFFFFFF;
  settings->light_floor = clamp_u8(dict_int(iter, MESSAGE_KEY_LIGHT_FLOOR, settings->light_floor),
                                   0, 100);
  settings->px_ms = clamp_u8(dict_int(iter, MESSAGE_KEY_SWEEP_MS, settings->px_ms), 10, 40);
  settings->trace_color = clamp_u8(dict_int(iter, MESSAGE_KEY_TRACE_COLOR, settings->trace_color),
                                   0, PALETTE_LEN - 1);
  settings->demo = dict_int(iter, MESSAGE_KEY_DEMO, settings->demo) != 0;
}

GColor settings_trace_color(const Settings *settings) {
#if defined(PBL_COLOR)
  const uint8_t index = settings->trace_color < PALETTE_LEN ? settings->trace_color : 0;
  return (GColor){.argb = s_palette_argb[index]};
#else
  (void)settings;
  return GColorWhite;
#endif
}

uint32_t settings_light_rgb(const Settings *settings, uint8_t brightness) {
  const uint32_t rgb = settings->light_color;
  const uint32_t r = ((rgb >> 16) & 0xFF) * brightness / 255;
  const uint32_t g = ((rgb >> 8) & 0xFF) * brightness / 255;
  const uint32_t b = (rgb & 0xFF) * brightness / 255;
  return (r << 16) | (g << 8) | b;
}

GColor settings_baseline_color(const Settings *settings) {
#if defined(PBL_COLOR)
  const uint8_t index = settings->trace_color < PALETTE_LEN ? settings->trace_color : 0;
  return (GColor){.argb = s_palette_dim_argb[index]};
#else
  (void)settings;
  return GColorWhite;
#endif
}
