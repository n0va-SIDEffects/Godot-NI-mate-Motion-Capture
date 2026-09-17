/*
 * User settings, edited on the phone through the app's settings page and kept across restarts.
 */
#pragma once

#include <pebble.h>

//! What the backlight does while the app is open.
typedef enum {
  BacklightAuto = 0,      //! leave it to the watch
  BacklightOnBeat = 1,    //! a short flash on every beat
  BacklightAlwaysOn = 2,  //! keep it on, which costs battery
  BacklightPulse = 3,     //! stay dimly lit and swell with every beat
} BacklightMode;

typedef struct {
  bool sound_on;         //! play the beep (only watches with a speaker have one)
  uint8_t volume;        //! 0 to 100
  uint8_t pitch_note;    //! MIDI note the beep is played at; 81 is the sample's own 880 Hz
  bool vibe_on;          //! buzz on every beat
  uint8_t vibe_ms;       //! length of that buzz
  uint8_t backlight;     //! a BacklightMode
  uint8_t px_ms;         //! milliseconds per trace pixel: lower sweeps faster
  uint8_t trace_color;   //! index into the palette in settings.c
  uint32_t light_color;  //! backlight tint as 0x00RRGGBB; watches without a colour backlight ignore it
  uint8_t light_floor;   //! how dim the pulsing backlight sits between beats, 0 to 100
  bool demo;             //! simulate a pulse instead of reading the sensor
} Settings;

//! Fill in the defaults, then overwrite them with whatever was stored.
void settings_load(Settings *settings);

void settings_save(const Settings *settings);

//! Apply an settings page update. Keys that are absent keep their current value.
void settings_read_dict(Settings *settings, DictionaryIterator *iter);

//! The colour the trace is drawn in. Black and white watches ignore the setting.
GColor settings_trace_color(const Settings *settings);

//! A dimmed version of it, for the baseline the trace rests on.
GColor settings_baseline_color(const Settings *settings);

//! The backlight tint scaled to a brightness of 0 to 255, packed as 0x00RRGGBB.
uint32_t settings_light_rgb(const Settings *settings, uint8_t brightness);
