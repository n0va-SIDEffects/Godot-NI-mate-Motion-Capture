#pragma once
#include <pebble.h>

// RGB-Backlight als Farborgel: Tonklasse der Gabel auf einem Farbkreis,
// Atmen in der Schwebungsrate nur bis 3 Hz (Aliasing, Photosensibilitaet),
// Neuanwendung alle 500 ms, weil das System die Farbe bei Benachrichtigungen
// zuruecksetzt.
void backlight_init(void);
void backlight_enable(bool on);
void backlight_set_pitch(int32_t fork_chz, int32_t flower_chz);
void backlight_set_state(int32_t beat_chz, bool in_window, bool near, bool locked);
void backlight_tick(uint32_t now_ms);
// Testmodus: 0..3 Atmen mit 1/2/3/4 Hz, 4 Dimmrampe in 8 Stufen, 5 aus
void backlight_test_set_step(uint8_t step);
void backlight_test_tick(uint32_t now_ms);
uint32_t backlight_call_count(void);
uint32_t backlight_last_rgb(void);
int32_t backlight_hue_deg(void);
void backlight_refresh(void);   // nach Fokusrueckkehr: light_enable und Farbe erneut setzen
