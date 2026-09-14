#include "render.h"
#include "config.h"
#include "e1clock.h"

static Layer *s_layer;
static const World *s_world;
static Overlay s_ov;
static RenderStats s_rs;
static PanelStats s_ps;
static AppTimer *s_panel_timer;
static uint32_t s_panel_start_ms;
static uint32_t s_panel_render_sum_ms;
static uint32_t s_last_frame_ms;
static char s_hud1[48];
static char s_hud2[48];
static char s_hud3[48];
static GFont s_font;

// Graustufen aus den 64 Farben des Panels. Mehr braucht ein Testtisch nicht,
// die Lichtstimmung des Konzepts kommt erst in Phase 2.
static uint8_t C_VOID, C_FLOOR, C_WALL, C_SLING, C_BUMPER, C_BALL, C_TRAIL, C_DIM, C_RING;

static uint8_t *s_fb;
static uint16_t s_stride;

static inline void prv_px(int x, int y, uint8_t c) {
  if (x < 0 || x >= SCR_W || y < 0 || y >= SCR_H) {
    return;
  }
  s_fb[(uint32_t)y * s_stride + x] = c;
}

static void prv_rect(int x0, int y0, int x1, int y1, uint8_t c) {
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > SCR_W) x1 = SCR_W;
  if (y1 > SCR_H) y1 = SCR_H;
  for (int y = y0; y < y1; y++) {
    memset(s_fb + (uint32_t)y * s_stride + x0, c, (size_t)(x1 - x0));
  }
}

static void prv_line(int x0, int y0, int x1, int y1, uint8_t c) {
  int dx = x1 - x0;
  int dy = y1 - y0;
  int sx = dx < 0 ? -1 : 1;
  int sy = dy < 0 ? -1 : 1;
  dx = dx < 0 ? -dx : dx;
  dy = dy < 0 ? -dy : dy;
  int err = dx - dy;
  for (;;) {
    prv_px(x0, y0, c);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    int e2 = err * 2;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

// Gefuellte Scheibe. Zeilenweise ueber die Kreisgleichung, ohne Wurzel je
// Pixel: pro Zeile wird die halbe Breite einmal gesucht.
static void prv_disc(int cx, int cy, int r, uint8_t c) {
  for (int dy = -r; dy <= r; dy++) {
    int y = cy + dy;
    if (y < 0 || y >= SCR_H) {
      continue;
    }
    int rem = r * r - dy * dy;
    int dx = 0;
    while ((dx + 1) * (dx + 1) <= rem) {
      dx++;
    }
    int x0 = cx - dx;
    int x1 = cx + dx + 1;
    if (x0 < 0) x0 = 0;
    if (x1 > SCR_W) x1 = SCR_W;
    if (x0 < x1) {
      memset(s_fb + (uint32_t)y * s_stride + x0, c, (size_t)(x1 - x0));
    }
  }
}

// Kreisumriss nach Bresenham. Fuer den Magnetring: nur Umriss, denn die
// Flaeche darunter gehoert dem Spielfeld (und dem echten Finger).
static void prv_circle(int cx, int cy, int r, uint8_t c) {
  int x = r;
  int y = 0;
  int err = 1 - r;
  while (x >= y) {
    prv_px(cx + x, cy + y, c);
    prv_px(cx + y, cy + x, c);
    prv_px(cx - y, cy + x, c);
    prv_px(cx - x, cy + y, c);
    prv_px(cx - x, cy - y, c);
    prv_px(cx - y, cy - x, c);
    prv_px(cx + y, cy - x, c);
    prv_px(cx + x, cy - y, c);
    y++;
    if (err < 0) {
      err += 2 * y + 1;
    } else {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

// Gestrichelter Kreisumriss, fuer Umrisse, die Information sind, aber nicht
// vom Spielfeld ablenken duerfen (Fingerkuppe).
static void prv_circle_dashed(int cx, int cy, int r, uint8_t c) {
  int x = r;
  int y = 0;
  int err = 1 - r;
  int k = 0;
  while (x >= y) {
    if ((k & 3) < 2) {
      prv_px(cx + x, cy + y, c);
      prv_px(cx + y, cy + x, c);
      prv_px(cx - y, cy + x, c);
      prv_px(cx - x, cy + y, c);
      prv_px(cx - x, cy - y, c);
      prv_px(cx - y, cy - x, c);
      prv_px(cx + y, cy - x, c);
      prv_px(cx + x, cy - y, c);
    }
    k++;
    y++;
    if (err < 0) {
      err += 2 * y + 1;
    } else {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

// Kapsel: Scheiben entlang der Achse. Schrittweite 2 px laesst bei Radius 4
// keine Luecken und halbiert die Arbeit gegenueber jedem Pixel.
static void prv_capsule(int x0, int y0, int x1, int y1, int r, uint8_t c) {
  int dx = x1 - x0;
  int dy = y1 - y0;
  int steps = (dx < 0 ? -dx : dx);
  int ady = (dy < 0 ? -dy : dy);
  if (ady > steps) {
    steps = ady;
  }
  if (steps == 0) {
    prv_disc(x0, y0, r, c);
    return;
  }
  int n = steps / 2;
  if (n < 1) {
    n = 1;
  }
  for (int i = 0; i <= n; i++) {
    int x = x0 + (dx * i) / n;
    int y = y0 + (dy * i) / n;
    prv_disc(x, y, r, c);
  }
}

static void prv_build_colors(void) {
  C_VOID   = GColorBlack.argb;
  C_FLOOR  = GColorFromRGB(85, 85, 85).argb;
  C_WALL   = GColorFromRGB(255, 255, 255).argb;
  C_SLING  = GColorFromRGB(170, 170, 170).argb;
  C_BUMPER = GColorFromRGB(170, 170, 170).argb;
  C_BALL   = GColorFromRGB(255, 255, 255).argb;
  C_TRAIL  = GColorFromRGB(170, 170, 170).argb;
  C_DIM    = GColorFromRGB(0, 0, 0).argb;
  C_RING   = GColorFromRGB(255, 255, 255).argb;
}

static void prv_draw_table(void) {
  const Table *t = &s_world->table;
  prv_rect(0, 0, SCR_W, SCR_H, C_FLOOR);
  // Rand ausserhalb der Aussenkontur abdunkeln, damit der Tisch eine Form hat
  prv_rect(0, 0, SCR_W, 8, C_VOID);
  prv_rect(0, 8, 7, SCR_H, C_VOID);
  prv_rect(193, 8, SCR_W, SCR_H, C_VOID);

  for (uint8_t i = 0; i < t->seg_count; i++) {
    const Segment *s = &t->seg[i];
    uint8_t c = (s->kind == SegSling) ? C_SLING : C_WALL;
    int ax = FX_TO_INT_R(s->a.x), ay = FX_TO_INT_R(s->a.y);
    int bx = FX_TO_INT_R(s->b.x), by = FX_TO_INT_R(s->b.y);
    if (s->kind == SegGate) {
      // Einwegtor gestrichelt: es ist eine Regel, keine Wand.
      int steps = 8;
      for (int k = 0; k < steps; k += 2) {
        int x0 = ax + ((bx - ax) * k) / steps;
        int y0 = ay + ((by - ay) * k) / steps;
        int x1 = ax + ((bx - ax) * (k + 1)) / steps;
        int y1 = ay + ((by - ay) * (k + 1)) / steps;
        prv_line(x0, y0, x1, y1, C_SLING);
      }
      continue;
    }
    prv_line(ax, ay, bx, by, c);
  }
  for (uint8_t i = 0; i < t->circ_count; i++) {
    const Circle *c = &t->circ[i];
    prv_disc(c->cx_px, c->cy_px, c->r_px, C_BUMPER);
    if (c->kind == CircBumper) {
      prv_circle(c->cx_px, c->cy_px, c->r_px, C_WALL);
      prv_disc(c->cx_px, c->cy_px, 3, C_WALL);
    }
  }
}

static void prv_draw_flippers(void) {
  for (uint8_t i = 0; i < 2; i++) {
    const Flipper *f = &s_world->table.flip[i];
    Vec tip = phys_flipper_tip(f);
    prv_capsule(f->pivot_x_px, f->pivot_y_px, FX_TO_INT_R(tip.x), FX_TO_INT_R(tip.y),
                FLIPPER_R_PX, C_WALL);
    prv_disc(f->pivot_x_px, f->pivot_y_px, 2, C_FLOOR);
  }
}

static void prv_draw_balls(void) {
  for (uint8_t i = 0; i < s_world->ball_count; i++) {
    const Ball *b = &s_world->ball[i];
    if (!b->alive) {
      continue;
    }
    // Spur zuerst, damit die Kugel obenauf liegt. Sie fuellt die Luecke
    // zwischen zwei Bildern: bei 25 fps und 500 px/s sind das 20 px.
    for (uint8_t k = BALL_TRAIL; k > 0; k--) {
      const Vec *p = &b->trail[k - 1];
      prv_disc(FX_TO_INT_R(p->x), FX_TO_INT_R(p->y), (k >= 3) ? 1 : 2, C_TRAIL);
    }
    prv_disc(FX_TO_INT_R(b->p.x), FX_TO_INT_R(b->p.y), BALL_R_PX, C_BALL);
  }
}

static void prv_draw_overlay(void) {
  if (s_ov.show_deadline) {
    for (int x = 8; x < 193; x += 6) {
      prv_px(x, MAG_DEAD_Y, C_SLING);
      prv_px(x + 1, MAG_DEAD_Y, C_SLING);
    }
  }
  if (s_ov.target_r > 0) {
    prv_circle(s_ov.target_x, s_ov.target_y, s_ov.target_r, C_WALL);
    if (s_ov.target_hit) {
      prv_circle(s_ov.target_x, s_ov.target_y, s_ov.target_r - 2, C_WALL);
    }
  }
  if (s_ov.finger) {
    // Magnetring: aussen bei 64 px, innen bei 56 px. Beide liegen ausserhalb
    // der Fingerkuppe (rund 40 px Radius), sind also sichtbar, waehrend der
    // Finger die Mitte verdeckt. Genau darum geht es der Jury.
    prv_circle(s_ov.fx, s_ov.fy, MAG_RADIUS_PX, C_RING);
    if (s_ov.charge_pct > 0) {
      int inner = 56 - (56 - 46) * s_ov.charge_pct / 100;
      prv_circle(s_ov.fx, s_ov.fy, inner, C_RING);
    }
    if (s_ov.grab) {
      prv_circle(s_ov.fx, s_ov.fy, MAG_GRAB_DIST_PX, C_RING);
    }
    if (s_ov.show_tip) {
      // Was der echte Zeigefinger verdecken wuerde. Im Emulator gibt es keine
      // Fingerkuppe, also wird sie gezeichnet.
      prv_circle_dashed(s_ov.fx, s_ov.fy, FINGER_TIP_R_PX, C_DIM);
    }
  }
  if (s_ov.plunger_pull > 0) {
    // Zugweg als Balken neben der Abschussbahn, plus ein Strich je Ratsche
    int h = s_ov.plunger_pull;
    prv_rect(195, 222 - h, 198, 222, C_WALL);
    for (uint8_t k = 0; k < s_ov.plunger_ticks; k++) {
      prv_rect(193, 220 - k * 10, 199, 221 - k * 10, C_SLING);
    }
  }
  if (s_ov.tilted) {
    prv_rect(0, SCR_H - 3, SCR_W, SCR_H, C_WALL);
  } else if (s_ov.tilt_warn) {
    for (int x = 0; x < SCR_W; x += 8) {
      prv_rect(x, SCR_H - 3, x + 4, SCR_H, C_SLING);
    }
  }
}

static void prv_panel_next(void *data) {
  s_panel_timer = NULL;
  if (s_ps.active && s_layer) {
    layer_mark_dirty(s_layer);
  }
}

static void prv_draw_panel(void) {
  uint8_t c = (uint8_t)(0xC0 | (s_ps.frames & 63));
  int rows = s_ps.variant == 0 ? SCR_H : 10;
  prv_rect(0, 0, SCR_W, rows, c);
}

static void prv_update(Layer *layer, GContext *ctx) {
  uint32_t t_start = e1clock_now_ms();
  GBitmap *fb = graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit);
  if (!fb) {
    s_rs.capture_failures++;
    return;
  }
  s_fb = gbitmap_get_data(fb);
  s_stride = gbitmap_get_bytes_per_row(fb);
  GRect b = gbitmap_get_bounds(fb);
  s_rs.stride = s_stride;
  s_rs.fb_w = b.size.w;
  s_rs.fb_h = b.size.h;
  s_rs.fb_format = (uint8_t)gbitmap_get_format(fb);
  bool ok = (s_fb != NULL) && (b.size.w >= SCR_W) && (b.size.h >= SCR_H) && (s_stride >= SCR_W);
  if (ok) {
    if (s_ps.active) {
      prv_draw_panel();
    } else if (s_world) {
      prv_draw_table();
      prv_draw_flippers();
      prv_draw_balls();
      prv_draw_overlay();
      prv_rect(0, 0, SCR_W, HUD_H, C_VOID);
    }
  }
  graphics_release_frame_buffer(ctx, fb);
  s_fb = NULL;
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
      if (total == 0) {
        total = 1;
      }
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
              "[P1][PANEL] %s: %lu Frames, %lu ms, %lu.%lu ms/Frame, %lu.%lu fps, zeichnen %lu.%lu ms",
              s_ps.variant == 0 ? "Vollbild" : "10 Zeilen", (unsigned long)s_ps.frames,
              (unsigned long)total, (unsigned long)(s_ps.ms_per_frame_x10 / 10),
              (unsigned long)(s_ps.ms_per_frame_x10 % 10), (unsigned long)(s_ps.fps_x10 / 10),
              (unsigned long)(s_ps.fps_x10 % 10), (unsigned long)(s_ps.render_ms_x10 / 10),
              (unsigned long)(s_ps.render_ms_x10 % 10));
    } else if (!s_panel_timer) {
      s_panel_timer = app_timer_register(1, prv_panel_next, NULL);
    }
  } else {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, s_hud1, s_font, GRect(2, -3, SCR_W - 4, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    if (s_hud2[0]) {
      graphics_draw_text(ctx, s_hud2, s_font, GRect(2, SCR_H - 34, SCR_W - 4, 16),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
    if (s_hud3[0]) {
      graphics_draw_text(ctx, s_hud3, s_font, GRect(2, SCR_H - 20, SCR_W - 4, 16),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
    s_rs.frames++;
    s_rs.render_ms_x10 = (s_rs.render_ms_x10 * 7 + render_ms * 10) / 8;
    if (render_ms > s_rs.render_ms_max) {
      s_rs.render_ms_max = render_ms;
    }
    if (s_last_frame_ms != 0) {
      uint32_t dt = t_start - s_last_frame_ms;
      if (dt == 0) {
        dt = 1;
      }
      s_rs.fps_x10 = (s_rs.fps_x10 * 7 + 10000 / dt) / 8;
    }
    s_last_frame_ms = t_start;
  }
}

void render_init(Layer *layer) {
  s_layer = layer;
  memset(&s_rs, 0, sizeof(s_rs));
  memset(&s_ps, 0, sizeof(s_ps));
  memset(&s_ov, 0, sizeof(s_ov));
  s_last_frame_ms = 0;
  s_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_hud1[0] = s_hud2[0] = s_hud3[0] = '\0';
  prv_build_colors();
  layer_set_update_proc(layer, prv_update);
}

void render_deinit(void) {
  render_panel_test_stop();
  s_layer = NULL;
  s_world = NULL;
}

void render_set_world(const World *w) {
  s_world = w;
}

void render_set_overlay(const Overlay *ov) {
  s_ov = *ov;
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
  APP_LOG(APP_LOG_LEVEL_INFO, "[P1][PANEL] Start %s, %d Frames",
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

const RenderStats *render_stats(void) {
  return &s_rs;
}

const PanelStats *render_panel_stats(void) {
  return &s_ps;
}
