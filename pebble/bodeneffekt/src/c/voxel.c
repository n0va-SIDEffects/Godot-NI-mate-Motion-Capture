#include "voxel.h"
#include "world.h"
#include "bclock.h"
#include "setup.h"

// ---------------------------------------------------------------- Zustand
static Layer *s_layer;
static Camera s_cam;
static VoxelStats s_st;
static PanelStats s_ps;
static AppTimer *s_panel_timer;
static uint32_t s_panel_start_ms;
static uint32_t s_panel_render_sum;
static uint32_t s_last_start_ms;
static uint8_t s_flash;
static int32_t s_agl8;
static int s_shadow_row = -1;
static int s_shadow_hw;
static char s_hud1[48];
static char s_hud2[48];
static char s_text[VOX_TEXT_LINES][32];
static bool s_scene = true;
static GFont s_font;
static GFont s_font_bold;

static uint16_t s_rays = RAYS_FULL;
static uint8_t s_sight_idx;
static uint16_t s_sight_cells = SIGHT_FAR;

// ---------------------------------------------------------------- Tabellen
// Schrittfolge der Strahlen. Die Schrittweite waechst, damit ferne Abschnitte
// grob und nahe fein abgetastet werden; z und die daraus folgende Projektion
// sind fuer jeden Schritt konstant und deshalb vorberechnet.
static uint32_t s_z8[RAY_MAX_STEPS];       // Distanz in 24.8
static uint16_t s_proj[RAY_MAX_STEPS];     // (SCALE_H << 16) / z8, siehe unten
static uint8_t s_fog_lo[RAY_MAX_STEPS];    // Nebelstufe
static uint8_t s_fog_thr[RAY_MAX_STEPS];   // Dither-Schwelle 0..16
static uint16_t s_steps;                   // gueltige Schritte bei aktueller Sichtweite

static uint8_t s_fog[FOG_LEVELS][PAL_COLORS];
static uint8_t s_dark[PAL_COLORS];
// Je Bildzeile 16 fertige Farben, indiziert mit dem Bayer-Wert. Damit
// dithert jeder Kanal fuer sich; eine gemeinsame Schwelle wuerde Rot und Blau
// an den Stufen von Gruen mitspringen lassen und den Verlauf bandig machen.
static uint8_t s_sky16[SCR_H * 16];
static int16_t s_hor[SCR_W];
static int16_t s_yb[SCR_W];

static const uint8_t s_bayer4[4][4] = {
  {  0,  8,  2, 10 },
  { 12,  4, 14,  6 },
  {  3, 11,  1,  9 },
  { 15,  7, 13,  5 },
};

// Himmel und Nebelziel. Der Nebel mischt Richtung Horizontfarbe, damit ferne
// Bergketten in genau den Ton verblassen, vor dem sie stehen.
#define SKY_TOP_R 0
#define SKY_TOP_G 85
#define SKY_TOP_B 255
#define SKY_HOR_R 170
#define SKY_HOR_G 170
#define SKY_HOR_B 255

static uint8_t s_sun_col;
static uint8_t s_emissive_a, s_emissive_b;

static inline uint8_t prv_argb(int r, int g, int b) {
  return GColorFromRGB((uint8_t)r, (uint8_t)g, (uint8_t)b).argb;
}

// 2 Bit je Kanal -> 0..3. Der Nebel rechnet in diesen vier Stufen und laesst
// das Bayer-Dither die Zwischenwerte machen.
static void prv_build_fog(void) {
  const int sr = SKY_HOR_R >> 6, sg = SKY_HOR_G >> 6, sb = SKY_HOR_B >> 6;
  for (int l = 0; l < FOG_LEVELS; l++) {
    const int t = (l * 255) / (FOG_LEVELS - 1);           // 0 = klar, 255 = voll
    for (int c = 0; c < PAL_COLORS; c++) {
      int r = (c >> 4) & 3, g = (c >> 2) & 3, b = c & 3;
      r = r + (((sr - r) * t + 128) >> 8);
      g = g + (((sg - g) * t + 128) >> 8);
      b = b + (((sb - b) * t + 128) >> 8);
      s_fog[l][c] = (uint8_t)(0xC0 | (r << 4) | (g << 2) | b);
    }
  }
  // Zwei emissive Farben: in jeder Nebelstufe Identitaet. Phase 1 nutzt davon
  // die Sonnenfarbe; die zweite ist fuer die Nachttore reserviert.
  s_emissive_a = (uint8_t)(prv_argb(255, 255, 255) & 63);
  s_emissive_b = (uint8_t)(prv_argb(255, 255, 0) & 63);
  for (int l = 0; l < FOG_LEVELS; l++) {
    s_fog[l][s_emissive_a] = (uint8_t)(0xC0 | s_emissive_a);
    s_fog[l][s_emissive_b] = (uint8_t)(0xC0 | s_emissive_b);
  }
}

static inline int prv_ch16(int v255) {
  int v = (v255 * 3 * 16) / 255;          // 0..48, vier Stufen mit 4 Bit Nachkomma
  return v > 48 ? 48 : v;
}

static void prv_build_sky(void) {
  for (int y = 0; y < SCR_H; y++) {
    int t = (y * 255) / (HORIZON_BASE + 40);
    if (t > 255) t = 255;
    const int r16 = prv_ch16(SKY_TOP_R + (((SKY_HOR_R - SKY_TOP_R) * t) >> 8));
    const int g16 = prv_ch16(SKY_TOP_G + (((SKY_HOR_G - SKY_TOP_G) * t) >> 8));
    const int b16 = prv_ch16(SKY_TOP_B + (((SKY_HOR_B - SKY_TOP_B) * t) >> 8));
    for (int bay = 0; bay < 16; bay++) {
      int r = (r16 + bay) >> 4;
      int g = (g16 + bay) >> 4;
      int b = (b16 + bay) >> 4;
      if (r > 3) r = 3;
      if (g > 3) g = 3;
      if (b > 3) b = 3;
      s_sky16[(y << 4) | bay] = (uint8_t)(0xC0 | (r << 4) | (g << 2) | b);
    }
  }
}

static void prv_build_dark(void) {
  for (int c = 0; c < PAL_COLORS; c++) {
    const int r = (c >> 4) & 3, g = (c >> 2) & 3, b = c & 3;
    s_dark[c] = (uint8_t)(0xC0 | ((r ? r - 1 : 0) << 4) | ((g ? g - 1 : 0) << 2) | (b ? b - 1 : 0));
  }
}

// Die Schrittfolge haengt nicht von der Sichtweite ab, nur die Zahl der
// gueltigen Schritte und die Nebelzuordnung.
static void prv_build_steps(void) {
  int32_t z8 = RAY_Z_START << 8;
  int32_t dz8 = RAY_DZ_START;
  const int32_t far8 = (int32_t)s_sight_cells << 8;
  int k = 0;
  for (; k < RAY_MAX_STEPS; k++) {
    if (z8 > far8) break;
    s_z8[k] = (uint32_t)z8;
    // proj = SCALE_H * 65536 / z8. Mit z8 >= 4*256 bleibt proj <= 11520 und das
    // Produkt mit der Hoehendifferenz (max 65280 in 24.8) passt in int32.
    s_proj[k] = (uint16_t)(((uint32_t)SCALE_H << 16) / (uint32_t)z8);
    // Nebel setzt erst bei einem Viertel der Sichtweite ein und steigt von dort
    // linear bis zur vollen Stufe. Physikalischer waere ein Ansteigen direkt am
    // Auge, aber dann verliert der Vordergrund seine Farbe und die Landschaft
    // wirkt einheitlich blass. Vier Bit Nachkomma fuer das Dither.
    const int32_t near8 = far8 / 4;
    const uint32_t f8max = (FOG_LEVELS - 1) * 16;
    uint32_t f8 = (z8 <= near8) ? 0u
                  : ((uint32_t)(z8 - near8) * f8max) / (uint32_t)(far8 - near8);
    if (f8 > f8max) f8 = f8max;
    s_fog_lo[k] = (uint8_t)(f8 >> 4);
    s_fog_thr[k] = (uint8_t)(16 - (f8 & 15));
    z8 += dz8;
    dz8 += RAY_DZ_GROW;
  }
  s_steps = (uint16_t)k;
}

// ---------------------------------------------------------------- Szene
static void prv_draw_terrain(uint8_t *fb, uint16_t stride) {
  const uint8_t *hmap = world_heights();
  const uint8_t *cmap = world_colors();
  const int cols = s_rays;
  const int colw = SCR_W / cols;                 // 1 oder 2 Pixel je Strahl

  // Horizontversatz je Spalte: Roll entsteht nicht durch Bildrotation, sondern
  // durch ein Add pro Spalte. tan(roll) in 8.8.
  const int32_t roll_ang = (s_cam.roll_deg8 * (TRIG_MAX_ANGLE / 360)) >> 8;
  const int32_t c_roll = cos_lookup(roll_ang);
  const int32_t tan8 = c_roll ? (int32_t)((sin_lookup(roll_ang) * 256) / c_roll) : 0;
  const int32_t hor0 = HORIZON_BASE + s_cam.pitch_px;
  const bool alt = setup_get()->horizont_alt != 0;
  for (int i = 0; i < cols; i++) {
    const int px = i * colw + (colw >> 1);
    // Vorzeichen: neigt sich der Gleiter nach rechts, sieht man auf der
    // rechten Bildseite mehr Boden, der Horizont liegt dort also HOEHER
    // (kleineres y). Die Einstellung 'gegen Kurve' stellt die alte, falsche
    // Fassung wieder her, damit sich beide messen lassen.
    const int32_t kipp = (((px - SCR_CX) * tan8) >> 8);
    s_hor[i] = (int16_t)(alt ? hor0 + kipp : hor0 - kipp);
    s_yb[i] = SCR_H;
  }

  const int32_t sy = sin_lookup(s_cam.yaw);
  const int32_t cy = cos_lookup(s_cam.yaw);
  // Blickrichtung (sin, cos), Rechtsvektor (cos, -sin), halbe Bildbreite
  // entspricht dem Faktor FOV_NUM/FOV_DEN.
  const int32_t camh8 = s_cam.h8;
  uint32_t steps_used = 0;

  for (int k = 0; k < s_steps; k++) {
    const int32_t z8 = s_z8[k];
    const int32_t proj = s_proj[k];
    // Linker Randpunkt und Schrittvektor entlang der Abtastlinie, 16.16.
    // z * forward  -/+  z * f * right
    // Weltposition des linken Randpunkts dieser Abtastlinie, 16.16:
    //   P = Kamera + z * Blickrichtung - z * f * Rechtsvektor
    // sin_lookup liefert bis 65536, z8 bis 51200; das Produkt sprengt int32,
    // deshalb 64 Bit fuer diese vier Werte. Das kostet vier Multiplikationen
    // je Schritt, nicht je Spalte.
    const int32_t fwd_x = (int32_t)(((int64_t)sy * z8) >> 8);
    const int32_t fwd_y = (int32_t)(((int64_t)cy * z8) >> 8);
    const int32_t half_x = (int32_t)(((int64_t)cy * z8 * FOV_NUM) / (FOV_DEN << 8));
    const int32_t half_y = (int32_t)((-(int64_t)sy * z8 * FOV_NUM) / (FOV_DEN << 8));
    int32_t px16 = s_cam.x16 + fwd_x - half_x;
    int32_t py16 = s_cam.y16 + fwd_y - half_y;
    const int32_t dx16 = (half_x * 2) / cols;
    const int32_t dy16 = (half_y * 2) / cols;

    const uint8_t *fog_lo = s_fog[s_fog_lo[k]];
    const uint8_t *fog_hi = s_fog[s_fog_lo[k] + 1 < FOG_LEVELS ? s_fog_lo[k] + 1 : s_fog_lo[k]];
    const int thr = s_fog_thr[k];

    bool any = false;
    for (int i = 0; i < cols; i++) {
      const int idx = (((py16 >> 16) & MAP_MASK) << MAP_BITS) | ((px16 >> 16) & MAP_MASK);
      px16 += dx16;
      py16 += dy16;
      const int yb = s_yb[i];
      if (yb <= 0) continue;
      any = true;
      int row = s_hor[i] + (((camh8 - ((int32_t)hmap[idx] << 8)) * proj) >> 16);
      if (row >= yb) continue;
      if (row < 0) row = 0;
      const uint8_t c = cmap[idx];
      const uint8_t clo = fog_lo[c & 63];
      const uint8_t chi = fog_hi[c & 63];
      const int x0 = i * colw;
      uint8_t *p = fb + (uint32_t)row * stride + x0;
      if (colw == 1) {
        const int bx = x0 & 3;
        for (int y = row; y < yb; y++, p += stride) {
          *p = (s_bayer4[y & 3][bx] >= thr) ? chi : clo;
        }
      } else {
        for (int y = row; y < yb; y++, p += stride) {
          const uint8_t *b4 = s_bayer4[y & 3];
          p[0] = (b4[x0 & 3] >= thr) ? chi : clo;
          p[1] = (b4[(x0 + 1) & 3] >= thr) ? chi : clo;
        }
      }
      s_yb[i] = (int16_t)row;
    }
    steps_used += cols;
    if (!any) break;                    // alle Spalten voll: der Rest ist verdeckt
  }
  s_st.steps_last = steps_used;
}

// Himmel wird zuletzt gefuellt, weil der Y-Buffer erst danach weiss, wo er
// aufhoert. So wird jedes Pixel des Bildes genau einmal geschrieben.
static void prv_draw_sky(uint8_t *fb, uint16_t stride) {
  const int cols = s_rays;
  const int colw = SCR_W / cols;
  for (int i = 0; i < cols; i++) {
    const int yb = s_yb[i];
    if (yb <= 0) continue;
    const int x0 = i * colw;
    uint8_t *p = fb + x0;
    if (colw == 1) {
      const int bx = x0 & 3;
      for (int y = 0; y < yb; y++, p += stride) {
        *p = s_sky16[(y << 4) | s_bayer4[y & 3][bx]];
      }
    } else {
      const int bx0 = x0 & 3, bx1 = (x0 + 1) & 3;
      for (int y = 0; y < yb; y++, p += stride) {
        const uint8_t *b4 = s_bayer4[y & 3];
        const uint8_t *sk = &s_sky16[y << 4];
        p[0] = sk[b4[bx0]];
        p[1] = sk[b4[bx1]];
      }
    }
  }
}

// Die Sonne steht auf ihrem echten Azimut relativ zum Blick und geht hinter
// Bergen unter, weil nur Pixel ueber dem Y-Buffer geschrieben werden.
static void prv_draw_sun(uint8_t *fb, uint16_t stride) {
  int32_t rel = (world_info()->sun_azimuth - s_cam.yaw) & (TRIG_MAX_ANGLE - 1);
  if (rel > TRIG_MAX_ANGLE / 2) rel -= TRIG_MAX_ANGLE;
  // Bildspalte aus dem Winkel: tan(rel) * SCALE_H, weil SCR_CX / SCALE_H
  // gerade der Tangens des halben Sichtfelds ist.
  const int32_t cr = cos_lookup(rel);
  if (cr < 8192) return;                     // Sonne seitlich oder hinter uns
  const int32_t t8 = (sin_lookup(rel) * 256) / cr;
  const int cx = SCR_CX + (int)((t8 * FOV_DEN) >> 8);
  const int cy = HORIZON_BASE + s_cam.pitch_px - 34;
  if (cx < -SUN_GLOW_R || cx > SCR_W + SUN_GLOW_R) return;
  const int cshift = (SCR_W / s_rays) == 2 ? 1 : 0;
  for (int dy = -SUN_GLOW_R; dy <= SUN_GLOW_R; dy++) {
    const int y = cy + dy;
    if (y < 0 || y >= SCR_H) continue;
    uint8_t *row = fb + (uint32_t)y * stride;
    const uint8_t *b4 = s_bayer4[y & 3];
    for (int dx = -SUN_GLOW_R; dx <= SUN_GLOW_R; dx++) {
      const int x = cx + dx;
      if (x < 0 || x >= SCR_W) continue;
      if (y >= s_yb[x >> cshift]) continue;  // hinter dem Grat
      const int d2 = dx * dx + dy * dy;
      if (d2 <= SUN_RADIUS * SUN_RADIUS) {
        row[x] = s_sun_col;
      } else if (d2 <= SUN_GLOW_R * SUN_GLOW_R) {
        // Halo als Distanzfeld, gedithert statt gestuft
        const int t = ((SUN_GLOW_R * SUN_GLOW_R - d2) * 16) /
                      (SUN_GLOW_R * SUN_GLOW_R - SUN_RADIUS * SUN_RADIUS);
        if (t > b4[x & 3]) row[x] = s_sun_col;
      }
    }
  }
}

// Der Schatten ist die wichtigste Spielinformation: er rueckt mit sinkender
// Hoehe an den Rumpf. Halbtransparent per Bayer-Schachbrett und darkLUT,
// weil der Framebuffer kein Blending kann.
static void prv_draw_shadow(uint8_t *fb, uint16_t stride) {
  const int32_t agl8 = s_agl8;
  s_shadow_row = -1;
  if (agl8 <= 0) return;
  // Der Gleiter fliegt CAM_BACK_CELLS vor der Kamera; genau dort steht sein
  // Schatten, und zwar mit derselben Projektionsformel wie das Terrain.
  const int back = setup_cam_back_cells();
  const int32_t ahead = back << 16;
  const int32_t sx = sin_lookup(s_cam.yaw), sc = cos_lookup(s_cam.yaw);
  const int32_t wx = s_cam.x16 + (int32_t)(((int64_t)sx * ahead) >> 16);
  const int32_t wy = s_cam.y16 + (int32_t)(((int64_t)sc * ahead) >> 16);
  const int32_t gh8 = world_height_at(wx, wy);
  const int32_t z8 = back << 8;
  const int32_t proj = (int32_t)(((uint32_t)SCALE_H << 16) / (uint32_t)z8);
  const int row = HORIZON_BASE + s_cam.pitch_px + (((s_cam.h8 - gh8) * proj) >> 16);
  if (row < 0 || row >= SCR_H) return;
  s_shadow_row = row;
  // Breite und Hoehe schrumpfen mit der Hoehe ueber Grund: genau das macht den
  // Schatten zum Hoehenmesser.
  int hw = 22 - (int)(agl8 >> 9);
  if (hw < 6) hw = 6;
  s_shadow_hw = hw;
  int hh = 7 - (int)(agl8 >> 10);
  if (hh < 2) hh = 2;
  for (int dy = -hh; dy <= hh; dy++) {
    const int y = row + dy;
    if (y < 0 || y >= SCR_H) continue;
    // Ellipse: span = hw * sqrt(1 - (dy/hh)^2), ganzzahlig genaehert
    const int q = (hh * hh - dy * dy) * hw * hw / (hh * hh);
    int span = 0;
    while ((span + 1) * (span + 1) <= q) span++;
    uint8_t *p = fb + (uint32_t)y * stride;
    const uint8_t *b4 = s_bayer4[y & 3];
    for (int x = SCR_CX - span; x <= SCR_CX + span; x++) {
      if (x < 0 || x >= SCR_W) continue;
      if (b4[x & 3] < 8) continue;           // 50 Prozent Schachbrett
      p[x] = s_dark[p[x] & 63];
    }
  }
}

// Der Gleiter: drei Rollposen als Spans, ohne Ressourcen. Der Rumpf rollt mit
// doppeltem Winkel, damit die Lage auf 1,5 Zoll ablesbar bleibt.
static void prv_draw_glider(uint8_t *fb, uint16_t stride) {
  const int cy = setup_glider_row() + s_cam.pitch_px;
  // Der Rumpf kippt im Bild um den vollen Rollwinkel, die Kamera nur um den
  // halben; die Differenz ist genau das, was man am Sprite sehen soll.
  const int32_t tilt = s_cam.roll_deg8 * 2 / 256;        // Grad
  const uint8_t body = prv_argb(85, 85, 85);
  const uint8_t edge = prv_argb(255, 255, 255);
  for (int dx = -22; dx <= 22; dx++) {
    const int adx = dx < 0 ? -dx : dx;
    // Der Rumpf folgt derselben Einstellung wie der Horizont, sonst passt die
    // Lage des Gleiters nicht mehr zur Welt hinter ihm.
    const int kipp = (dx * tilt) / 55;
    const int wing = cy + (adx * 6) / 22 + (setup_get()->horizont_alt ? -kipp : kipp);
    const int th = adx < 6 ? 5 : 2;
    for (int t = 0; t < th; t++) {
      const int y = wing + t;
      const int x = SCR_CX + dx;
      if (y < 0 || y >= SCR_H || x < 0 || x >= SCR_W) continue;
      fb[(uint32_t)y * stride + x] = (t == 0) ? edge : body;
    }
  }
}

static void prv_dim_hud(uint8_t *fb, uint16_t stride) {
  for (int y = HUD_Y0; y < SCR_H; y++) {
    uint8_t *p = fb + (uint32_t)y * stride;
    for (int x = 0; x < SCR_W; x++) {
      p[x] = s_dark[s_dark[p[x] & 63] & 63];
    }
  }
}

static void prv_flash(uint8_t *fb, uint16_t stride) {
  const uint8_t red = prv_argb(255, 0, 0);
  for (int y = 0; y < SCR_H; y += 2) {
    memset(fb + (uint32_t)y * stride, red, SCR_W);
  }
}

static void prv_draw_scene(uint8_t *fb, uint16_t stride) {
  prv_draw_terrain(fb, stride);
  prv_draw_sky(fb, stride);
  prv_draw_sun(fb, stride);
  prv_draw_shadow(fb, stride);
  prv_draw_glider(fb, stride);
  prv_dim_hud(fb, stride);
  if (s_flash) {
    prv_flash(fb, stride);
    s_flash--;
  }
}

// ---------------------------------------------------------------- Panel-Test
static void prv_panel_next(void *data) {
  s_panel_timer = NULL;
  if (s_ps.active && s_layer) {
    layer_mark_dirty(s_layer);
  }
}

static void prv_draw_panel(uint8_t *fb, uint16_t stride) {
  if (s_ps.variant == 2) {
    prv_draw_scene(fb, stride);
    return;
  }
  const uint8_t c = (uint8_t)(0xC0 | (s_ps.frames & 63));
  const int rows = s_ps.variant == 0 ? SCR_H : 10;
  for (int y = 0; y < rows; y++) {
    memset(fb + (uint32_t)y * stride, c, SCR_W);
  }
}

// ---------------------------------------------------------------- Update
static void prv_update(Layer *layer, GContext *ctx) {
  const uint32_t t_start = bclock_now_ms();
  GBitmap *fb = graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit);
  if (!fb) {
    s_st.capture_failures++;
    return;
  }
  uint8_t *data = gbitmap_get_data(fb);
  const uint16_t stride = gbitmap_get_bytes_per_row(fb);
  const GRect b = gbitmap_get_bounds(fb);
  s_st.stride = stride;
  s_st.fb_w = b.size.w;
  s_st.fb_h = b.size.h;
  s_st.fb_format = (uint8_t)gbitmap_get_format(fb);
  if (data && b.size.w >= SCR_W && b.size.h >= SCR_H && stride >= SCR_W) {
    if (s_ps.active) {
      prv_draw_panel(data, stride);
    } else if (s_scene) {
      prv_draw_scene(data, stride);
    } else {
      const uint8_t bg = prv_argb(0, 0, 85);
      for (int y = 0; y < SCR_H; y++) {
        memset(data + (uint32_t)y * stride, bg, SCR_W);
      }
    }
  }
  graphics_release_frame_buffer(ctx, fb);
  const uint32_t t_end = bclock_now_ms();
  const uint32_t render_ms = t_end - t_start;

  if (s_ps.active) {
    if (s_ps.frames == 0) {
      s_panel_start_ms = t_start;
      s_panel_render_sum = 0;
    }
    s_ps.frames++;
    s_panel_render_sum += render_ms;
    if (s_ps.frames >= PANEL_TEST_FRAMES) {
      uint32_t total = t_end - s_panel_start_ms;
      if (total == 0) total = 1;
      s_ps.total_ms = total;
      s_ps.ms_per_frame_x10 = (total * 10) / s_ps.frames;
      s_ps.fps_x10 = (s_ps.frames * 10000) / total;
      s_ps.render_ms_x10 = (s_panel_render_sum * 10) / s_ps.frames;
      s_ps.result_x10[s_ps.variant] = s_ps.ms_per_frame_x10;
      s_ps.result_rast_x10[s_ps.variant] = s_ps.render_ms_x10;
      s_ps.active = false;
      s_ps.done = true;
      APP_LOG(APP_LOG_LEVEL_INFO,
              "[BE][PANEL] %s: %lu Bilder, %lu ms, %lu.%lu ms/Bild, %lu.%lu fps, rast %lu.%lu ms",
              s_ps.variant == 0 ? "Vollbild" : (s_ps.variant == 1 ? "10 Zeilen" : "Voxel"),
              (unsigned long)s_ps.frames, (unsigned long)total,
              (unsigned long)(s_ps.ms_per_frame_x10 / 10), (unsigned long)(s_ps.ms_per_frame_x10 % 10),
              (unsigned long)(s_ps.fps_x10 / 10), (unsigned long)(s_ps.fps_x10 % 10),
              (unsigned long)(s_ps.render_ms_x10 / 10), (unsigned long)(s_ps.render_ms_x10 % 10));
      if (s_ps.variant == 2) {
        APP_LOG(APP_LOG_LEVEL_INFO, "[BE][PANEL] Voxel mit %u Strahlen, Sicht %u Zellen, %lu Schritte",
                (unsigned)s_rays, (unsigned)s_sight_cells, (unsigned long)s_st.steps_last);
      }
    } else if (!s_panel_timer) {
      s_panel_timer = app_timer_register(1, prv_panel_next, NULL);
    }
    return;
  }

  graphics_context_set_text_color(ctx, GColorWhite);
  if (s_scene) {
    graphics_draw_text(ctx, s_hud1, s_font, GRect(2, HUD_Y0 - 3, SCR_W - 4, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, s_hud2, s_font, GRect(2, HUD_Y0 + 11, SCR_W - 4, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  } else {
    for (int i = 0; i < VOX_TEXT_LINES; i++) {
      graphics_draw_text(ctx, s_text[i], i == 0 ? s_font_bold : s_font,
                         GRect(3, 4 + i * 21, SCR_W - 6, 21),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
  }

  s_st.frames++;
  s_st.render_ms_x10 = (s_st.render_ms_x10 * 7 + render_ms * 10) / 8;
  if (render_ms > s_st.render_ms_max) s_st.render_ms_max = render_ms;
  if (s_last_start_ms) {
    uint32_t dt = t_start - s_last_start_ms;
    if (dt == 0) dt = 1;
    s_st.frame_ms_x10 = (s_st.frame_ms_x10 * 7 + dt * 10) / 8;
    s_st.fps_x10 = (s_st.fps_x10 * 7 + 10000 / dt) / 8;
  }
  s_last_start_ms = t_start;
}

// ---------------------------------------------------------------- API
void voxel_init(Layer *layer) {
  s_layer = layer;
  s_st = (VoxelStats){ 0 };
  s_ps = (PanelStats){ 0 };
  s_last_start_ms = 0;
  s_flash = 0;
  s_agl8 = 0;
  s_hud1[0] = s_hud2[0] = '\0';
  for (int i = 0; i < VOX_TEXT_LINES; i++) s_text[i][0] = '\0';
  s_scene = true;
  s_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_font_bold = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_sun_col = prv_argb(255, 255, 255);
  prv_build_fog();
  prv_build_sky();
  prv_build_dark();
  prv_build_steps();
  layer_set_update_proc(layer, prv_update);
}

void voxel_deinit(void) {
  voxel_panel_stop();
  s_layer = NULL;
}

void voxel_set_camera(const Camera *cam) { s_cam = *cam; }
void voxel_set_flash(uint8_t frames) { s_flash = frames; }
void voxel_set_agl8(int32_t agl8) { s_agl8 = agl8; }

int voxel_shadow_row(void) { return s_shadow_row; }

int voxel_shadow_halfwidth(void) { return s_shadow_hw; }

void voxel_set_hud(const char *l1, const char *l2) {
  strncpy(s_hud1, l1, sizeof(s_hud1) - 1);
  strncpy(s_hud2, l2, sizeof(s_hud2) - 1);
  s_hud1[sizeof(s_hud1) - 1] = '\0';
  s_hud2[sizeof(s_hud2) - 1] = '\0';
}

void voxel_render_to(uint8_t *fb, uint16_t stride) { prv_draw_scene(fb, stride); }

void voxel_set_scene(bool on) { s_scene = on; }
bool voxel_scene(void) { return s_scene; }

void voxel_set_text(int idx, const char *line) {
  if (idx < 0 || idx >= VOX_TEXT_LINES) return;
  strncpy(s_text[idx], line, sizeof(s_text[0]) - 1);
  s_text[idx][sizeof(s_text[0]) - 1] = '\0';
}

void voxel_set_rays(uint16_t rays) {
  s_rays = (rays == RAYS_HALF) ? RAYS_HALF : RAYS_FULL;
}

uint16_t voxel_rays(void) { return s_rays; }

void voxel_set_sight(uint8_t idx) {
  static const uint16_t tab[SIGHT_COUNT] = { SIGHT_FAR, SIGHT_MID, SIGHT_NEAR };
  s_sight_idx = (uint8_t)(idx % SIGHT_COUNT);
  s_sight_cells = tab[s_sight_idx];
  prv_build_steps();
}

uint8_t voxel_sight(void) { return s_sight_idx; }
uint16_t voxel_sight_cells(void) { return s_sight_cells; }

void voxel_panel_start(uint8_t variant) {
  s_ps.active = true;
  s_ps.done = false;
  s_ps.variant = (uint8_t)(variant % PANEL_VARIANTS);
  s_ps.frames = 0;
  s_ps.total_ms = 0;
  APP_LOG(APP_LOG_LEVEL_INFO, "[BE][PANEL] Start %s, %d Bilder",
          s_ps.variant == 0 ? "Vollbild" : (s_ps.variant == 1 ? "10 Zeilen" : "Voxel"),
          PANEL_TEST_FRAMES);
  if (s_layer) layer_mark_dirty(s_layer);
}

void voxel_panel_stop(void) {
  s_ps.active = false;
  if (s_panel_timer) {
    app_timer_cancel(s_panel_timer);
    s_panel_timer = NULL;
  }
}

const PanelStats *voxel_panel_stats(void) { return &s_ps; }
const VoxelStats *voxel_stats(void) { return &s_st; }
