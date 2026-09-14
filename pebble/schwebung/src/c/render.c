#include "render.h"
#include "config.h"
#include "e1clock.h"

#define PETAL_PTS 12

typedef struct {
  int16_t x;
  int16_t y;
} Pt;

typedef struct {
  Pt p[PETAL_PTS];
  int16_t ymin;
  int16_t ymax;
} Petal;

static Layer *s_layer;
static Petal s_petals[FLOWER_PETALS];
static uint8_t s_sinA[256];
static uint8_t s_sinB[256];
static uint8_t s_sinC[256];
static uint8_t s_pal_floor[16];
static uint8_t s_sky_t16[SCR_H];
static uint8_t s_sky_a;
static uint8_t s_sky_b;
static uint8_t s_hud_bg;
static uint8_t s_blend_glass[64];
static uint8_t s_blend_core[64];
static uint8_t s_dark[64];
static uint8_t s_bright[4][64];
static uint8_t s_falloff64[256];
static uint8_t s_stepx[FLOOR_ROWS];
static uint8_t s_diag[SCR_W + FLOOR_ROWS + 2];
static const uint8_t s_bayer4[4][4] = {
  { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 }
};
static uint8_t s_bayer8[8][8];
static uint32_t s_frame;
static uint32_t s_last_frame_ms;
static RenderStats s_rs;
static PanelStats s_ps;
static AppTimer *s_panel_timer;
static uint32_t s_panel_start_ms;
static uint32_t s_panel_render_sum_ms;
static FlowerVis s_fl;
static char s_hud1[64];
static char s_hud2[64];
static char s_hud3[64];
static GFont s_font;
static GFont s_font_small;

static uint8_t prv_rgb(uint8_t r, uint8_t g, uint8_t b) {
  return GColorFromRGB(r, g, b).argb;
}

static uint8_t prv_blend_to(uint8_t c, uint8_t tr, uint8_t tg, uint8_t tb) {
  uint8_t r = (c >> 4) & 3;
  uint8_t g = (c >> 2) & 3;
  uint8_t b = c & 3;
  r = (uint8_t)((r + tr + 1) >> 1);
  g = (uint8_t)((g + tg + 1) >> 1);
  b = (uint8_t)((b + tb + 1) >> 1);
  return (uint8_t)(0xC0 | (r << 4) | (g << 2) | b);
}

static void prv_build_tables(void) {
  for (int i = 0; i < 256; i++) {
    int32_t s = sin_lookup((int32_t)(((int64_t)i * TRIG_MAX_ANGLE) / 256));
    uint8_t v = (uint8_t)(32 + (s * 31) / TRIG_MAX_RATIO);
    s_sinA[i] = v;
    s_sinB[i] = v;
    s_sinC[i] = v;
  }
  // Boden: Sand -> helles Tuerkis -> Weiss
  for (int i = 0; i < 16; i++) {
    uint8_t r, g, b;
    if (i < 8) {
      r = 170;
      g = (uint8_t)(170 + (255 - 170) * i / 8);
      b = (uint8_t)(85 + (255 - 85) * i / 8);
    } else {
      int j = i - 8;
      r = (uint8_t)(170 + (255 - 170) * j / 8);
      g = 255;
      b = 255;
    }
    s_pal_floor[i] = prv_rgb(r, g, b);
  }
  s_sky_a = prv_rgb(170, 170, 255);
  s_sky_b = prv_rgb(255, 255, 255);
  s_hud_bg = prv_rgb(0, 0, 85);
  for (int y = 0; y < SCR_H; y++) {
    int t = (y - SKY_Y0) * 16 / (FLOOR_Y0 - SKY_Y0);
    if (t < 0) t = 0;
    if (t > 16) t = 16;
    s_sky_t16[y] = (uint8_t)t;
  }
  for (int c = 0; c < 64; c++) {
    s_blend_glass[c] = prv_blend_to((uint8_t)c, 3, 3, 2);   // blasses Gelb-Weiss
    s_blend_core[c] = prv_blend_to((uint8_t)c, 3, 3, 3);    // Weiss
    uint8_t r = (c >> 4) & 3, g = (c >> 2) & 3, b = c & 3;
    s_dark[c] = (uint8_t)(0xC0 | ((r ? r - 1 : 0) << 4) | ((g ? g - 1 : 0) << 2) | (b ? b - 1 : 0));
    for (int l = 0; l < 4; l++) {
      uint8_t rr = (uint8_t)(r + l > 3 ? 3 : r + l);
      uint8_t gg = (uint8_t)(g + l > 3 ? 3 : g + l);
      uint8_t bb = (uint8_t)(b + l > 3 ? 3 : b + l);
      s_bright[l][c] = (uint8_t)(0xC0 | (rr << 4) | (gg << 2) | bb);
    }
  }
  for (int i = 0; i < 256; i++) {
    int32_t d2 = i * 16;
    int32_t v = 64 - (d2 * 64) / (BLOOM_R * BLOOM_R);
    s_falloff64[i] = (uint8_t)(v < 0 ? 0 : v);
  }
  for (int ry = 0; ry < FLOOR_ROWS; ry++) {
    s_stepx[ry] = (uint8_t)(2 + ((FLOOR_ROWS - 1 - ry) * 6) / (FLOOR_ROWS - 1));
  }
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      uint8_t base = s_bayer4[y & 3][x & 3];
      uint8_t add = (uint8_t)(((y >> 2) & 1) * 2 + ((x >> 2) & 1) * 3);
      if (((y >> 2) & 1) && ((x >> 2) & 1)) add = 1;
      s_bayer8[y][x] = (uint8_t)(base * 4 + add);
    }
  }
  // Blaetter: Ellipsen um einen Ring
  const int32_t d = FLOWER_R * 55 / 100;
  const int32_t a = FLOWER_R * 50 / 100;
  const int32_t b = FLOWER_R * 28 / 100;
  for (int k = 0; k < FLOWER_PETALS; k++) {
    int32_t phi = (int32_t)(((int64_t)k * TRIG_MAX_ANGLE) / FLOWER_PETALS);
    int32_t cph = cos_lookup(phi);
    int32_t sph = sin_lookup(phi);
    int32_t cx = (d * cph) >> 16;
    int32_t cy = (d * sph) >> 16;
    Petal *pt = &s_petals[k];
    pt->ymin = 32767;
    pt->ymax = -32768;
    for (int i = 0; i < PETAL_PTS; i++) {
      int32_t psi = (int32_t)(((int64_t)i * TRIG_MAX_ANGLE) / PETAL_PTS);
      int32_t u = (a * cos_lookup(psi)) >> 16;
      int32_t v = (b * sin_lookup(psi)) >> 16;
      int32_t x = cx + ((u * cph - v * sph) >> 16);
      int32_t y = cy + ((u * sph + v * cph) >> 16);
      pt->p[i].x = (int16_t)x;
      pt->p[i].y = (int16_t)y;
      if (y < pt->ymin) pt->ymin = (int16_t)y;
      if (y > pt->ymax) pt->ymax = (int16_t)y;
    }
  }
}

static inline void prv_span(uint8_t *fb, uint16_t stride, int y, int x0, int x1,
                            const uint8_t *blend) {
  if (y < SKY_Y0 || y >= SCR_H) return;
  if (x0 < 0) x0 = 0;
  if (x1 > SCR_W) x1 = SCR_W;
  if (x0 >= x1) return;
  uint8_t *row = fb + (uint32_t)y * stride;
  const uint8_t *src = fb + (uint32_t)(y + 1 < SCR_H ? y + 1 : y) * stride;
  for (int x = x0; x < x1; x++) {
    int sx = x + 1 < SCR_W ? x + 1 : x;
    row[x] = blend[src[sx] & 63];
  }
}

static void prv_draw_petal(uint8_t *fb, uint16_t stride, const Petal *pt, int cx, int cy,
                           int32_t vib_amp16, uint32_t vib_phase) {
  for (int yy = pt->ymin; yy <= pt->ymax; yy++) {
    int y = cy + yy;
    int xl = 32767;
    int xr = -32768;
    for (int i = 0; i < PETAL_PTS; i++) {
      Pt p0 = pt->p[i];
      Pt p1 = pt->p[(i + 1) % PETAL_PTS];
      int y0 = p0.y, y1 = p1.y;
      if (y0 == y1) continue;
      int ylo = y0 < y1 ? y0 : y1;
      int yhi = y0 < y1 ? y1 : y0;
      if (yy < ylo || yy >= yhi) continue;
      int x = p0.x + ((p1.x - p0.x) * (yy - y0)) / (y1 - y0);
      if (x < xl) xl = x;
      if (x > xr) xr = x;
    }
    if (xl > xr) continue;
    int32_t dx16 = 0;
    if (vib_amp16) {
      uint32_t ang = (vib_phase + (uint32_t)(yy * 3000)) & (TRIG_MAX_ANGLE - 1);
      dx16 = (vib_amp16 * sin_lookup((int32_t)ang)) / TRIG_MAX_RATIO;
    }
    int shift = dx16 >= 0 ? (dx16 >> 4) : -((-dx16) >> 4);
    int frac = dx16 >= 0 ? (dx16 & 15) : ((-dx16) & 15);
    int x0 = cx + xl + shift;
    int x1 = cx + xr + 1 + shift;
    prv_span(fb, stride, y, x0, x1, s_blend_glass);
    if (y >= SKY_Y0 && y < SCR_H) {
      uint8_t *row = fb + (uint32_t)y * stride;
      if (frac >= 8) {
        if (x0 - 1 >= 0 && x0 - 1 < SCR_W) row[x0 - 1] = s_blend_glass[row[x0 - 1] & 63];
        if (x1 >= 0 && x1 < SCR_W) row[x1] = s_blend_glass[row[x1] & 63];
      }
      if (x0 >= 0 && x0 < SCR_W) row[x0] = s_dark[row[x0] & 63];
      if (x1 - 1 >= 0 && x1 - 1 < SCR_W) row[x1 - 1] = s_dark[row[x1 - 1] & 63];
    }
  }
}

static int prv_isqrt(int v) {
  int r = 0;
  while ((r + 1) * (r + 1) <= v) r++;
  return r;
}

static void prv_draw_core(uint8_t *fb, uint16_t stride, int cx, int cy) {
  const int rc = FLOWER_R * 22 / 100;
  for (int dy = -rc; dy <= rc; dy++) {
    int half = prv_isqrt(rc * rc - dy * dy);
    prv_span(fb, stride, cy + dy, cx - half, cx + half + 1, s_blend_core);
  }
}

static void prv_bloom(uint8_t *fb, uint16_t stride, int cx, int cy, uint32_t intensity16) {
  for (int dy = -BLOOM_R; dy <= BLOOM_R; dy++) {
    int y = cy + dy;
    if (y < SKY_Y0 || y >= SCR_H) continue;
    uint8_t *row = fb + (uint32_t)y * stride;
    int dy2 = dy * dy;
    for (int dx = -BLOOM_R; dx <= BLOOM_R; dx++) {
      int x = cx + dx;
      if (x < 0 || x >= SCR_W) continue;
      int d2 = dx * dx + dy2;
      if (d2 > BLOOM_R * BLOOM_R) continue;
      uint32_t v = ((uint32_t)s_falloff64[d2 >> 4] * intensity16) >> 4;
      uint32_t lvl = v >> 4;
      uint32_t rem = v & 15;
      if (rem * 4 > s_bayer8[y & 7][x & 7]) lvl++;
      if (lvl > 3) lvl = 3;
      if (lvl) row[x] = s_bright[lvl][row[x] & 63];
    }
  }
}

static void prv_draw_scene(uint8_t *fb, uint16_t stride) {
  const uint32_t t = s_frame;
  // HUD-Hintergrund
  for (int y = 0; y < HUD_H; y++) {
    memset(fb + (uint32_t)y * stride, s_hud_bg, SCR_W);
  }
  // Himmel, gedithert
  for (int y = SKY_Y0; y < FLOOR_Y0; y++) {
    uint8_t *row = fb + (uint32_t)y * stride;
    const uint8_t *bay = s_bayer4[y & 3];
    uint8_t t16 = s_sky_t16[y];
    for (int x = 0; x < SCR_W; x++) {
      row[x] = (t16 > bay[x & 3]) ? s_sky_b : s_sky_a;
    }
  }
  // Kaustik-Boden: 100 Spalten, pixelverdoppelt
  for (int i = 0; i < SCR_W + FLOOR_ROWS + 2; i++) {
    s_diag[i] = s_sinC[((uint32_t)i * 2 + t * 3) & 255];
  }
  for (int ry = 0; ry < FLOOR_ROWS; ry++) {
    int y = FLOOR_Y0 + ry;
    uint8_t *row = fb + (uint32_t)y * stride;
    uint32_t phase = t * 3;
    const uint32_t step = s_stepx[ry];
    const uint8_t b_term = s_sinB[((uint32_t)ry * 5 - t * 2) & 255];
    const uint8_t *bay = s_bayer4[y & 3];
    for (int cx = 0; cx < SCR_W / 2; cx++) {
      uint32_t sum = (uint32_t)s_sinA[phase & 255] + b_term + s_diag[cx * 2 + ry];
      phase += step;
      uint32_t lvl = (sum * 16 + (uint32_t)bay[(cx * 2) & 3] * 12) / 192;
      if (lvl > 15) lvl = 15;
      uint8_t c = s_pal_floor[lvl];
      row[cx * 2] = c;
      row[cx * 2 + 1] = c;
    }
  }
  // Glasblume
  if (s_fl.visible) {
    if (s_fl.glow16) {
      prv_bloom(fb, stride, FLOWER_CX, FLOWER_CY, s_fl.glow16);
    }
    for (int k = 0; k < FLOWER_PETALS; k++) {
      prv_draw_petal(fb, stride, &s_petals[k], FLOWER_CX, FLOWER_CY, s_fl.vib_amp16, s_fl.vib_phase);
    }
    prv_draw_core(fb, stride, FLOWER_CX, FLOWER_CY);
  }
}

static void prv_panel_next(void *data) {
  s_panel_timer = NULL;
  if (s_ps.active && s_layer) {
    layer_mark_dirty(s_layer);
  }
}

static void prv_draw_panel(uint8_t *fb, uint16_t stride) {
  uint8_t c = (uint8_t)(0xC0 | (s_ps.frames & 63));
  int rows = s_ps.variant == 0 ? SCR_H : 10;
  for (int y = 0; y < rows; y++) {
    memset(fb + (uint32_t)y * stride, c, SCR_W);
  }
}

static void prv_update(Layer *layer, GContext *ctx) {
  uint32_t t_start = e1clock_now_ms();
  GBitmap *fb = graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit);
  if (!fb) {
    s_rs.capture_failures++;
    return;
  }
  uint8_t *data = gbitmap_get_data(fb);
  uint16_t stride = gbitmap_get_bytes_per_row(fb);
  GRect b = gbitmap_get_bounds(fb);
  s_rs.stride = stride;
  s_rs.fb_w = b.size.w;
  s_rs.fb_h = b.size.h;
  s_rs.fb_format = (uint8_t)gbitmap_get_format(fb);
  bool ok = (data != NULL) && (b.size.w >= SCR_W) && (b.size.h >= SCR_H) && (stride >= SCR_W);
  if (ok) {
    if (s_ps.active) {
      prv_draw_panel(data, stride);
    } else {
      prv_draw_scene(data, stride);
    }
  }
  graphics_release_frame_buffer(ctx, fb);
  s_frame++;
  uint32_t t_end = e1clock_now_ms();
  uint32_t render_ms = t_end - t_start;

  if (s_ps.active) {
    if (s_ps.frames == 0) {
      s_panel_start_ms = t_start;
      s_panel_render_sum_ms = 0;
    }
    s_ps.frames++;
    s_panel_render_sum_ms += render_ms;
    if (s_ps.frames >= PANEL_TEST_FRAMES) {
      uint32_t total = t_end - s_panel_start_ms;
      if (total == 0) total = 1;
      s_ps.total_ms = total;
      s_ps.ms_per_frame_x10 = (total * 10) / s_ps.frames;
      s_ps.fps_x10 = (s_ps.frames * 10000) / total;
      s_ps.render_ms_x10 = (s_panel_render_sum_ms * 10) / s_ps.frames;
      if (s_ps.variant == 0) {
        s_ps.result_full_x10 = s_ps.ms_per_frame_x10;
      } else {
        s_ps.result_band_x10 = s_ps.ms_per_frame_x10;
      }
      s_ps.active = false;
      s_ps.done = true;
      APP_LOG(APP_LOG_LEVEL_INFO,
              "[E1][PANEL] %s: %lu Frames, %lu ms, %lu.%lu ms/Frame, %lu.%lu fps, rend %lu.%lu ms",
              s_ps.variant == 0 ? "Vollbild" : "10 Zeilen", (unsigned long)s_ps.frames,
              (unsigned long)total, (unsigned long)(s_ps.ms_per_frame_x10 / 10),
              (unsigned long)(s_ps.ms_per_frame_x10 % 10), (unsigned long)(s_ps.fps_x10 / 10),
              (unsigned long)(s_ps.fps_x10 % 10), (unsigned long)(s_ps.render_ms_x10 / 10),
              (unsigned long)(s_ps.render_ms_x10 % 10));
    } else if (!s_panel_timer) {
      s_panel_timer = app_timer_register(1, prv_panel_next, NULL);
    }
  } else {
    // HUD
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, s_hud1, s_font, GRect(2, -2, SCR_W - 4, 18), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, s_hud2, s_font_small, GRect(2, 15, SCR_W - 4, 18), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
    graphics_context_set_text_color(ctx, GColorOxfordBlue);
    graphics_draw_text(ctx, s_hud3, s_font_small, GRect(2, SCR_H - 18, SCR_W - 4, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    // Statistik
    s_rs.frames++;
    s_rs.render_ms_x10 = (s_rs.render_ms_x10 * 7 + render_ms * 10) / 8;
    if (s_last_frame_ms != 0) {
      uint32_t dt = t_start - s_last_frame_ms;
      if (dt == 0) dt = 1;
      uint32_t fps_x10 = 10000 / dt;
      s_rs.fps_x10 = (s_rs.fps_x10 * 7 + fps_x10) / 8;
    }
    s_last_frame_ms = t_start;
  }
}

void render_init(Layer *layer) {
  s_layer = layer;
  s_rs = (RenderStats){ 0 };
  s_ps = (PanelStats){ 0 };
  s_fl = (FlowerVis){ .visible = true, .vib_amp16 = 0, .vib_phase = 0, .glow16 = 0 };
  s_frame = 0;
  s_last_frame_ms = 0;
  s_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_font_small = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_hud1[0] = s_hud2[0] = s_hud3[0] = '\0';
  prv_build_tables();
  layer_set_update_proc(layer, prv_update);
}

void render_set_flower(const FlowerVis *vis) {
  s_fl = *vis;
}

void render_set_hud(const char *line1, const char *line2, const char *line3) {
  strncpy(s_hud1, line1, sizeof(s_hud1) - 1);
  strncpy(s_hud2, line2, sizeof(s_hud2) - 1);
  strncpy(s_hud3, line3, sizeof(s_hud3) - 1);
  s_hud1[sizeof(s_hud1) - 1] = '\0';
  s_hud2[sizeof(s_hud2) - 1] = '\0';
  s_hud3[sizeof(s_hud3) - 1] = '\0';
}

void render_panel_test_start(uint8_t variant) {
  s_ps.active = true;
  s_ps.done = false;
  s_ps.variant = variant;
  s_ps.frames = 0;
  s_ps.total_ms = 0;
  APP_LOG(APP_LOG_LEVEL_INFO, "[E1][PANEL] Start %s, %d Frames",
          variant == 0 ? "Vollbild" : "10 Zeilen", PANEL_TEST_FRAMES);
  if (s_layer) {
    layer_mark_dirty(s_layer);
  }
}

void render_panel_test_stop(void) {
  s_ps.active = false;
  if (s_panel_timer) {
    app_timer_cancel(s_panel_timer);
    s_panel_timer = NULL;
  }
}

void render_deinit(void) {
  render_panel_test_stop();
  s_layer = NULL;
}

const RenderStats *render_stats(void) {
  return &s_rs;
}

const PanelStats *render_panel_stats(void) {
  return &s_ps;
}
