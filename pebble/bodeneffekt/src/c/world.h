#pragma once
#include <pebble.h>
#include "config.h"

// Prozedurale Welt aus einem Seed: 128 x 128 Heightmap plus Colormap mit
// eingebackener Beleuchtung. Die Bytes der Colormap sind bereits fertige
// Framebuffer-Werte (GColor8.argb), zur Laufzeit kostet Licht damit nichts.

typedef struct {
  uint32_t seed;
  int32_t sun_azimuth;      // TRIG-Winkel, 0 = Blick nach +Y
  uint32_t gen_ms;          // Dauer der letzten Erzeugung
  uint8_t h_min;
  uint8_t h_max;
} WorldInfo;

void world_generate(uint32_t seed, int32_t sun_azimuth);
const WorldInfo *world_info(void);

const uint8_t *world_heights(void);
const uint8_t *world_colors(void);

// Hoehe an einer Weltposition (16.16), bilinear, Ergebnis in 24.8.
int32_t world_height_at(int32_t x16, int32_t y16);
