#pragma once
#include <pebble.h>

// Renderer direkt im 8-Bit-Framebuffer: Himmel, Kaustik-Boden (100 Spalten,
// pixelverdoppelt), Glasblume mit Refraktions-Blend und Scanline-Vibration,
// Bloom per LUT. Danach HUD-Text per GContext. Dazu der Panel-Test:
// Vollbild gegen 10 Zeilen, je 300 Frames so schnell wie moeglich.
typedef struct {
  bool visible;
  int32_t vib_amp16;     // Amplitude in 1/16 Pixel (max 32 = 2 px)
  uint32_t vib_phase;    // TRIG-Winkel
  uint8_t glow16;        // 0..16 Bloom-Intensitaet
} FlowerVis;

typedef struct {
  uint32_t frames;
  uint32_t fps_x10;
  uint32_t render_ms_x10;
  uint32_t capture_failures;
  uint16_t stride;
  uint8_t fb_format;
  int16_t fb_w;
  int16_t fb_h;
} RenderStats;

typedef struct {
  bool active;
  bool done;
  uint8_t variant;             // 0 = Vollbild, 1 = 10 Zeilen
  uint32_t frames;
  uint32_t total_ms;
  uint32_t ms_per_frame_x10;
  uint32_t fps_x10;
  uint32_t render_ms_x10;
  uint32_t result_full_x10;    // ms pro Frame, Vollbild (0 = noch nicht gemessen)
  uint32_t result_band_x10;    // ms pro Frame, 10 Zeilen
} PanelStats;

void render_init(Layer *layer);
void render_set_flower(const FlowerVis *vis);
void render_set_hud(const char *line1, const char *line2, const char *line3);
void render_panel_test_start(uint8_t variant);
void render_panel_test_stop(void);
const RenderStats *render_stats(void);
const PanelStats *render_panel_stats(void);
