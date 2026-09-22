// Rendert eine Szene ohne Uhr und ohne Emulator in ein PPM. Damit laesst sich
// jede Kameralage gezielt pruefen, auch die, auf die man im Emulator lange
// warten muesste (Schatten im Bodeneffekt-Fenster, Sonne im Blickfeld).
//
//   cc -O2 -I tools/shim -I src/c -o /tmp/sp tools/scenepreview.c \
//      src/c/voxel.c src/c/world.c -lm
//   /tmp/sp --agl 6 --yaw 0 > /tmp/szene.ppm
#include "voxel.h"
#include "world.h"
#include "setup.h"
#include <stdlib.h>

uint32_t bclock_now_ms(void) { return 0; }

static uint8_t s_fb[SCR_H * SCR_W];

int main(int argc, char **argv) {
  uint32_t seed = 20260916u;
  int32_t sun_deg = 25, yaw_deg = 0, roll_deg = 0;
  int agl = 6, x = 64, y = 8, rays = RAYS_FULL, sight = 0;
  for (int i = 1; i < argc - 1; i++) {
    const char *k = argv[i];
    const long v = strtol(argv[i + 1], NULL, 10);
    if (!strcmp(k, "--seed")) seed = (uint32_t)v;
    else if (!strcmp(k, "--sun")) sun_deg = (int32_t)v;
    else if (!strcmp(k, "--yaw")) yaw_deg = (int32_t)v;
    else if (!strcmp(k, "--roll")) roll_deg = (int32_t)v;
    else if (!strcmp(k, "--agl")) agl = (int)v;
    else if (!strcmp(k, "--x")) x = (int)v;
    else if (!strcmp(k, "--y")) y = (int)v;
    else if (!strcmp(k, "--rays")) rays = (int)v;
    else if (!strcmp(k, "--sight")) sight = (int)v;
  }
  setup_init();
  world_generate(seed, (sun_deg * TRIG_MAX_ANGLE) / 360);
  voxel_init(NULL);
  voxel_set_rays((uint16_t)rays);
  voxel_set_sight((uint8_t)sight);

  // Gleiterposition und daraus die Verfolgerkamera, genau wie flight.c.
  const int32_t gx16 = x << 16, gy16 = y << 16;
  const int32_t yaw = (yaw_deg * TRIG_MAX_ANGLE) / 360;
  const int32_t ground8 = world_height_at(gx16, gy16);
  const int32_t gh8 = ground8 + (agl << 8);
  const int32_t back16 = setup_cam_back_cells() << 16;
  Camera cam;
  cam.x16 = gx16 - (int32_t)(((int64_t)sin_lookup(yaw) * back16) >> 16);
  cam.y16 = gy16 - (int32_t)(((int64_t)cos_lookup(yaw) * back16) >> 16);
  cam.h8 = gh8 + (CAM_UP_CELLS << 8);
  const int32_t cg8 = world_height_at(cam.x16, cam.y16);
  if (cam.h8 < cg8 + (3 << 8)) cam.h8 = cg8 + (3 << 8);
  cam.yaw = yaw;
  cam.roll_deg8 = (roll_deg * 256) / 2;      // Kamera rollt mit halbem Winkel
  cam.pitch_px = 0;
  voxel_set_camera(&cam);
  voxel_set_agl8(agl << 8);

  memset(s_fb, 0, sizeof(s_fb));
  voxel_render_to(s_fb, SCR_W);

  fprintf(stderr, "Seed %u  Gleiter %d/%d agl %d  Kurs %d  Roll %d  Sonne %d  "
                  "Strahlen %d  Sicht %u  Boden %ld  Kameraboden %ld%s\n",
          (unsigned)seed, x, y, agl, yaw_deg, roll_deg, sun_deg, rays,
          (unsigned)voxel_sight_cells(), (long)(ground8 >> 8), (long)(cg8 >> 8),
          (cam.h8 <= cg8 + (3 << 8)) ? "  (Kamera angehoben)" : "");

  printf("P6\n%d %d\n255\n", SCR_W, SCR_H);
  for (int i = 0; i < SCR_W * SCR_H; i++) {
    const int v = s_fb[i] & 63;
    const unsigned char px[3] = { (unsigned char)(((v >> 4) & 3) * 85),
                                  (unsigned char)(((v >> 2) & 3) * 85),
                                  (unsigned char)((v & 3) * 85) };
    fwrite(px, 1, 3, stdout);
  }
  return 0;
}
