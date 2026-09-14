#pragma once
#include <pebble.h>
#include "physics.h"

// Renderer direkt im 8-Bit-Framebuffer. Grauer Testtisch, noch ohne
// Grafikpracht: Banden als Linien, Bumper und Pfosten als Scheiben, Flipper
// als Kapseln, Kugel mit kurzer Spur, Magnetring und der Umriss der
// Fingerkuppe. Danach HUD-Text per GContext.
//
// Bewusst kein Dirty-Rect-Renderer: graphics_release_frame_buffer meldet immer
// den ganzen Puffer als schmutzig, und der Compositor ruft ohnehin
// framebuffer_dirty_all. Was zaehlt, ist die Bildrate; die misst der
// PANEL-Bildschirm.

typedef struct {
  bool finger;
  int16_t fx;
  int16_t fy;
  uint8_t charge_pct;
  bool grab;
  bool show_tip;          // Umriss der Fingerkuppe zeichnen (Verdeckung sichtbar machen)
  bool show_deadline;     // Grenze der Magnet-Totzone zeigen
  int16_t plunger_pull;
  uint8_t plunger_ticks;
  bool tilt_warn;
  bool tilted;
  int16_t target_x;       // Magnet-Uebung: Zielkreis, -1 = keiner
  int16_t target_y;
  int16_t target_r;
  bool target_hit;
} Overlay;

typedef struct {
  uint32_t frames;
  uint32_t fps_x10;
  uint32_t render_ms_x10;
  uint32_t render_ms_max;
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
  uint32_t result_full_x10;
  uint32_t result_band_x10;
} PanelStats;

void render_init(Layer *layer);
void render_deinit(void);
void render_set_world(const World *w);
void render_set_overlay(const Overlay *ov);
void render_set_hud(const char *line1, const char *line2, const char *line3);
void render_panel_test_start(uint8_t variant);
void render_panel_test_stop(void);
const RenderStats *render_stats(void);
const PanelStats *render_panel_stats(void);
