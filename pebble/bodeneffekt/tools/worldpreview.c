// Begutachtet die prozedurale Welt ohne Uhr: Hoehenhistogramm, Verteilung der
// Terraintypen und ein PPM der Colormap. So laesst sich die Landschaft
// abstimmen, ohne jedes Mal den Emulator zu starten.
//
//   cc -O2 -I tools/shim -I src/c -o /tmp/wp tools/worldpreview.c src/c/world.c -lm
//   /tmp/wp 20260916 > /tmp/welt.ppm
#include "world.h"
#include <stdlib.h>

int main(int argc, char **argv) {
  const uint32_t seed = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 20260916u;
  const int32_t azim = (argc > 2) ? (int32_t)strtol(argv[2], NULL, 10) : 0;
  world_generate(seed, azim);
  const uint8_t *h = world_heights();
  const uint8_t *c = world_colors();

  int hist[16] = { 0 };
  int colhist[64] = { 0 };
  long sum = 0;
  for (int i = 0; i < MAP_CELLS; i++) {
    hist[h[i] >> 4]++;
    colhist[c[i] & 63]++;
    sum += h[i];
  }
  fprintf(stderr, "Seed %u  h %u..%u  Mittel %ld  Erzeugung %u ms\n",
          (unsigned)seed, world_info()->h_min, world_info()->h_max,
          sum / MAP_CELLS, (unsigned)world_info()->gen_ms);
  fprintf(stderr, "Hoehenhistogramm (16er-Klassen):\n");
  for (int i = 0; i < 16; i++) {
    fprintf(stderr, "%4d-%3d %5d %.1f%%\n", i * 16, i * 16 + 15, hist[i],
            100.0 * hist[i] / MAP_CELLS);
  }
  fprintf(stderr, "Genutzte Farben:\n");
  for (int i = 0; i < 64; i++) {
    if (colhist[i]) {
      fprintf(stderr, "  rgb %d/%d/%d  %5d  %.1f%%\n", (i >> 4) & 3, (i >> 2) & 3, i & 3,
              colhist[i], 100.0 * colhist[i] / MAP_CELLS);
    }
  }

  printf("P6\n%d %d\n255\n", MAP_SIZE, MAP_SIZE);
  for (int i = 0; i < MAP_CELLS; i++) {
    const int v = c[i] & 63;
    const unsigned char px[3] = { (unsigned char)(((v >> 4) & 3) * 85),
                                  (unsigned char)(((v >> 2) & 3) * 85),
                                  (unsigned char)((v & 3) * 85) };
    fwrite(px, 1, 3, stdout);
  }
  return 0;
}

uint32_t bclock_now_ms(void) { return 0; }
