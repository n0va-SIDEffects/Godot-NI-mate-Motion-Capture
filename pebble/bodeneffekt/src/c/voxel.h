#pragma once
#include <pebble.h>
#include "config.h"

// Voxel-Space-Renderer direkt im 8-Bit-Framebuffer.
// Comanche-Algorithmus mit Y-Buffer: die Strahlen laufen von vorne nach hinten,
// der Y-Buffer haelt je Spalte die oberste bereits gefuellte Zeile. Damit wird
// jedes Bildpixel genau einmal geschrieben und die Verdeckung stimmt ohne
// Z-Buffer. (Das Konzept nennt "von hinten nach vorne"; der dort beschriebene
// Y-Buffer-Test "Zeile ueber ybuffer[col]" ist aber genau der Vorne-nach-hinten-
// Fall, und nur der schreibt jedes Pixel einmal.)

typedef struct {
  int32_t x16, y16;          // Weltposition, 16.16
  int32_t h8;                // Kamerahoehe ueber Null, 24.8
  int32_t yaw;               // TRIG-Winkel
  int32_t roll_deg8;         // Roll in Grad, 24.8, positiv = rechts
  int32_t pitch_px;          // Nicklage als Horizontversatz in Bildzeilen
} Camera;

typedef struct {
  uint32_t frames;
  uint32_t render_ms_x10;    // geglaettet: unsere Rasterzeit im Update-Proc
  uint32_t render_ms_max;
  uint32_t frame_ms_x10;     // geglaettet: Abstand zweier Update-Proc-Starts
  uint32_t fps_x10;
  uint32_t capture_failures;
  uint32_t steps_last;       // Strahlenschritte im letzten Bild
  uint16_t stride;
  int16_t fb_w, fb_h;
  uint8_t fb_format;
} VoxelStats;

typedef struct {
  bool active;
  bool done;
  uint8_t variant;             // 0 Vollbild, 1 zehn Zeilen, 2 echte Voxel-Szene
  uint32_t frames;
  uint32_t total_ms;
  uint32_t ms_per_frame_x10;
  uint32_t fps_x10;
  uint32_t render_ms_x10;
  uint32_t result_x10[PANEL_VARIANTS];        // ms je Bild, gesamt
  uint32_t result_rast_x10[PANEL_VARIANTS];   // davon unsere eigene Rasterzeit
} PanelStats;

void voxel_init(Layer *layer);
void voxel_deinit(void);

void voxel_set_camera(const Camera *cam);

// Der Renderer zeigt entweder die Szene (mit zwei HUD-Zeilen im unteren Band)
// oder eine reine Textseite fuer den Messbildschirm.
#define VOX_TEXT_LINES 8
void voxel_set_scene(bool on);
bool voxel_scene(void);
void voxel_set_hud(const char *l1, const char *l2);
void voxel_set_text(int idx, const char *line);
void voxel_set_flash(uint8_t frames);        // Rot-Blitz bei Bodenkontakt
void voxel_set_agl8(int32_t agl8);           // Hoehe ueber Grund fuer den Schatten

// Stellschrauben, die die Messung gegeneinander stellt
void voxel_set_rays(uint16_t rays);          // 200 oder 100 (dann 2 px je Spalte)
uint16_t voxel_rays(void);
void voxel_set_sight(uint8_t idx);           // 0..SIGHT_COUNT-1, Nebeldichte
uint8_t voxel_sight(void);
uint16_t voxel_sight_cells(void);

// Rendert die Szene in einen beliebigen 8-Bit-Puffer. Auf der Uhr nicht
// benutzt; die Host-Vorschau in tools/ erzeugt damit Bilder ohne Emulator.
void voxel_render_to(uint8_t *fb, uint16_t stride);

void voxel_panel_start(uint8_t variant);
void voxel_panel_stop(void);
const PanelStats *voxel_panel_stats(void);
const VoxelStats *voxel_stats(void);
