#include "world.h"
#include "bclock.h"

// Value-Noise mit vier Oktaven, Integer-Hash, bilineare Interpolation in
// Festkomma. Die Gitterwerte jeder Oktave werden einmal in eine kleine Tabelle
// gehasht (4x4 bis 32x32, zusammen 1360 Byte auf dem Stack), danach kostet
// eine Zelle nur noch vier Ladevorgaenge und drei Interpolationen. Das ist der
// Unterschied zwischen 262.144 Hashes und 1.360 Hashes fuer dieselbe Karte.

static uint8_t s_hmap[MAP_CELLS];
static uint8_t s_cmap[MAP_CELLS];
static WorldInfo s_info;

// Fuenf Terrainrampen zu je fuenf Helligkeitsstufen, als RGB-Tripel.
// GColorFromRGB quantisiert jeden Kanal auf 0/85/170/255.
static const uint8_t s_ramp_rgb[5][5][3] = {
  // Wasser
  { {   0,   0,  85 }, {   0,   0, 170 }, {   0,  85, 170 }, {   0,  85, 255 }, {  85, 170, 255 } },
  // Sand
  { {  85,  85,   0 }, { 170,  85,   0 }, { 170, 170,  85 }, { 255, 170,  85 }, { 255, 255, 170 } },
  // Gras
  { {   0,  85,   0 }, {   0, 170,   0 }, {  85, 170,   0 }, {  85, 170,  85 }, { 170, 255,  85 } },
  // Fels: Rostrot bis Ocker, der Comanche-Ton. Die oberste Stufe bleibt
  // bewusst ockerhell statt rosa, sonst kippt die ganze Landschaft ins Rosa,
  // sobald der Nebel Richtung Himmelsblau mischt.
  { {  85,   0,   0 }, { 170,   0,   0 }, { 170,  85,   0 }, { 255, 170,  85 }, { 255, 255, 170 } },
  // Schnee: im Schatten blau, in der Sonne weiss
  { {  85,  85, 170 }, { 170, 170, 255 }, { 170, 170, 255 }, { 255, 255, 255 }, { 255, 255, 255 } },
};

static uint8_t s_ramp[5][5];

static inline uint32_t prv_hash(uint32_t seed, uint32_t x, uint32_t y) {
  uint32_t h = seed + x * 374761393u + y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

// Glaettung 0..255 -> 0..255, klassisches 3t^2 - 2t^3.
static inline int32_t prv_smooth(int32_t t) {
  int32_t v = (t * t * (3 * 256 - 2 * t)) >> 16;
  return v > 255 ? 255 : v;
}

// Die Amplituden der vier Oktaven summieren sich auf 240, jede Oktave liefert
// nur nicht-negative Werte. Deshalb akkumuliert das Rauschen direkt in der
// uint8-Heightmap und braucht keinen 64-KB-Zwischenpuffer.
static uint8_t s_lat[32 * 32];

static void prv_octave(uint8_t *acc, uint32_t seed, int grid_bits, int32_t amp) {
  const int g = 1 << grid_bits;
  const int gm = g - 1;
  const int cell_bits = MAP_BITS - grid_bits;     // Zellen je Gittermasche
  const int cell_mask = (1 << cell_bits) - 1;
  uint8_t *lat = s_lat;
  for (int gy = 0; gy < g; gy++) {
    for (int gx = 0; gx < g; gx++) {
      lat[gy * g + gx] = (uint8_t)(prv_hash(seed, (uint32_t)gx, (uint32_t)gy) & 0xFF);
    }
  }
  for (int y = 0; y < MAP_SIZE; y++) {
    const int gy = y >> cell_bits;
    const int gy1 = (gy + 1) & gm;
    const int32_t sy = prv_smooth(((y & cell_mask) << 8) >> cell_bits);
    const uint8_t *r0 = &lat[gy * g];
    const uint8_t *r1 = &lat[gy1 * g];
    uint8_t *out = acc + y * MAP_SIZE;
    for (int x = 0; x < MAP_SIZE; x++) {
      const int gx = x >> cell_bits;
      const int gx1 = (gx + 1) & gm;
      const int32_t sx = prv_smooth(((x & cell_mask) << 8) >> cell_bits);
      const int32_t a = r0[gx], b = r0[gx1], c = r1[gx], d = r1[gx1];
      const int32_t top = a + (((b - a) * sx) >> 8);
      const int32_t bot = c + (((d - c) * sx) >> 8);
      out[x] = (uint8_t)(out[x] + ((((top + (((bot - top) * sy) >> 8)) * amp) >> 8)));
    }
  }
}

// Der Talweg schlaengelt sich in X als Funktion von Y und ist ueber die
// Kachelgrenze hinweg stetig, weil er eine ganze Sinusperiode auf 128 Zellen
// legt. Sonst haette die Schlucht an der Nahtstelle einen Knick.
static inline int prv_path_x(int y, int32_t phase) {
  int32_t ang = (int32_t)(((uint32_t)y << 16) >> MAP_BITS) + phase;
  return MAP_SIZE / 2 + ((CANYON_AMPLITUDE * sin_lookup(ang)) >> 16);
}

void world_generate(uint32_t seed, int32_t sun_azimuth) {
  const uint32_t t0 = bclock_now_ms();

  for (int t = 0; t < 5; t++) {
    for (int l = 0; l < 5; l++) {
      s_ramp[t][l] = GColorFromRGB(s_ramp_rgb[t][l][0], s_ramp_rgb[t][l][1],
                                   s_ramp_rgb[t][l][2]).argb;
    }
  }

  memset(s_hmap, 0, sizeof(s_hmap));
  // Die beiden hohen Oktaven bekommen weniger Gewicht als die klassische
  // Halbierung vorgibt. Mit 32 und 16 wird die Silhouette am Horizont zu einer
  // Nadelwand; die grossen Formen tragen das Bild, die feinen sollen nur die
  // Haenge texturieren.
  prv_octave(s_hmap, seed ^ 0x9E3779B9u, 2, 128);
  prv_octave(s_hmap, seed ^ 0x85EBCA6Bu, 3, 64);
  prv_octave(s_hmap, seed ^ 0xC2B2AE35u, 4, 24);
  prv_octave(s_hmap, seed ^ 0x27D4EB2Fu, 5, 8);

  const int32_t phase = (int32_t)(seed & 0xFFFF);
  uint8_t hmin = 255, hmax = 0;
  for (int y = 0; y < MAP_SIZE; y++) {
    const int px = prv_path_x(y, phase);
    uint8_t *row = &s_hmap[y * MAP_SIZE];
    for (int x = 0; x < MAP_SIZE; x++) {
      int32_t h = row[x];
      // Talprofil: innerhalb der halben Breite zieht ein quadratisches Profil
      // die Hoehe auf den Canyon-Boden herunter.
      int d = x - px;
      if (d < -MAP_SIZE / 2) d += MAP_SIZE;
      if (d > MAP_SIZE / 2) d -= MAP_SIZE;
      if (d < 0) d = -d;
      if (d < CANYON_HALFWIDTH) {
        const int32_t t = (d << 8) / CANYON_HALFWIDTH;   // 0..255
        const int32_t t2 = (t * t) >> 8;
        if (h > CANYON_FLOOR) {
          h = CANYON_FLOOR + (((h - CANYON_FLOOR) * t2) >> 8);
        }
      }
      if (h < WATER_LEVEL) h = WATER_LEVEL - ((WATER_LEVEL - h) >> 2);  // flache Seen
      row[x] = (uint8_t)h;
      if (h < hmin) hmin = (uint8_t)h;
      if (h > hmax) hmax = (uint8_t)h;
    }
  }

  // Eingebackene Beleuchtung: die Neigung Richtung Sonne wird einmal pro Zelle
  // auf fuenf Helligkeitsstufen quantisiert. Zur Laufzeit kostet Licht nichts.
  // Der Abtastabstand entscheidet, ob man die Beleuchtung sieht. Ueber eine
  // einzige Zelle ist der Hoehenunterschied bei diesem Rauschen so klein, dass
  // fast jede Zelle auf der mittleren Helligkeitsstufe landet und die
  // Landschaft flach eingefaerbt wirkt. Drei Zellen treffen die Groessenordnung
  // der Hangneigung.
  const int reach = 3;
  int sdx = (sin_lookup(sun_azimuth) * reach + (1 << 15)) >> 16;
  int sdy = (cos_lookup(sun_azimuth) * reach + (1 << 15)) >> 16;
  if (sdx == 0 && sdy == 0) sdx = reach;
  for (int y = 0; y < MAP_SIZE; y++) {
    const int ys = ((y + sdy) & MAP_MASK) * MAP_SIZE;
    const uint8_t *row = &s_hmap[y * MAP_SIZE];
    uint8_t *out = &s_cmap[y * MAP_SIZE];
    for (int x = 0; x < MAP_SIZE; x++) {
      const int h = row[x];
      const int hs = s_hmap[ys + ((x + sdx) & MAP_MASK)];
      int slope = hs - h;              // positiv = Hang von der Sonne weg
      int lvl = 2 - (slope / 5);
      if (lvl < 0) lvl = 0;
      if (lvl > 4) lvl = 4;
      const int as = slope < 0 ? -slope : slope;
      int type;
      if (h <= WATER_LEVEL) {
        type = 0;                      // Wasser
        lvl = 2 + (lvl > 2 ? 1 : 0);
      } else if (as > 18) {
        type = 3;                      // steil = Fels, unabhaengig von der Hoehe
      } else if (h < 60) {
        type = 1;                      // Sand
      } else if (h < 105) {
        type = 2;                      // Gras
      } else if (h < 188) {
        type = 3;                      // Fels
      } else {
        type = 4;                      // Schnee
      }
      out[x] = s_ramp[type][lvl];
    }
  }

  s_info.seed = seed;
  s_info.sun_azimuth = sun_azimuth;
  s_info.h_min = hmin;
  s_info.h_max = hmax;
  s_info.gen_ms = bclock_now_ms() - t0;
}

const WorldInfo *world_info(void) {
  return &s_info;
}

const uint8_t *world_heights(void) {
  return s_hmap;
}

const uint8_t *world_colors(void) {
  return s_cmap;
}

int32_t world_height_at(int32_t x16, int32_t y16) {
  const int xi = (x16 >> 16) & MAP_MASK;
  const int yi = (y16 >> 16) & MAP_MASK;
  const int xi1 = (xi + 1) & MAP_MASK;
  const int yi1 = (yi + 1) & MAP_MASK;
  const int32_t fx = (x16 >> 8) & 255;
  const int32_t fy = (y16 >> 8) & 255;
  const int32_t a = s_hmap[yi * MAP_SIZE + xi];
  const int32_t b = s_hmap[yi * MAP_SIZE + xi1];
  const int32_t c = s_hmap[yi1 * MAP_SIZE + xi];
  const int32_t d = s_hmap[yi1 * MAP_SIZE + xi1];
  const int32_t top = (a << 8) + ((b - a) * fx);
  const int32_t bot = (c << 8) + ((d - c) * fx);
  return top + (((bot - top) * fy) >> 8);
}
