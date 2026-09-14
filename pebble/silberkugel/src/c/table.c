#include "table.h"
#include "config.h"

// Die Punktreihenfolge bestimmt die Normale: n = (dy, -dx) / Laenge. Laeuft man
// die Aussenkontur im Uhrzeigersinn ab (links hinunter, rechts hinauf, oben nach
// links), zeigt n immer ins Spielfeld. Fuer normale Banden ist das nur eine
// Rueckfallebene, weil die Normale sonst aus dem Kugelmittelpunkt folgt; fuer
// das Einwegtor entscheidet sie, von welcher Seite es sperrt.

typedef struct {
  int16_t ax, ay, bx, by;
  uint8_t kind;
  uint8_t rest_pct;
} SegDef;

static const SegDef s_segs[] = {
  // Linke Aussenwand, von oben nach unten
  {   8,  56,   8, 224, SegWall,  REST_WALL_PCT },
  // Rechte Aussenwand, von unten nach oben
  { 192, 224, 192,  54, SegWall,  REST_WALL_PCT },
  // Oberer Bogen, von rechts nach links
  { 192,  54, 185,  36, SegWall,  REST_WALL_PCT },
  { 185,  36, 170,  22, SegWall,  REST_WALL_PCT },
  { 170,  22, 146,  13, SegWall,  REST_WALL_PCT },
  { 146,  13, 110,   9, SegWall,  REST_WALL_PCT },
  { 110,   9,  72,   9, SegWall,  REST_WALL_PCT },
  {  72,   9,  44,  13, SegWall,  REST_WALL_PCT },
  {  44,  13,  24,  22, SegWall,  REST_WALL_PCT },
  {  24,  22,  12,  38, SegWall,  REST_WALL_PCT },
  {  12,  38,   8,  56, SegWall,  REST_WALL_PCT },
  // Plunger-Bahn: Trennwand, Boden, Einwegtor nach oben.
  // Die Trennwand endet hoeher als der linke Fusspunkt des Tores. Stossen
  // beide in einem Punkt zusammen, findet eine Kugel, die knapp ueber die
  // Wandkante kommt, den Weg daran vorbei in den Kanal; mit der Ueberlappung
  // liegt sie immer auf dem Tor auf (mit tools/hosttest nachgewiesen).
  { 174, 224, 174,  52, SegWall,  REST_WALL_PCT },
  { 174, 222, 192, 222, SegWall,  REST_WALL_PCT },
  { 174,  64, 192,  54, SegGate,  REST_WALL_PCT },
  // Rueckfuehrungen: Ohne sie rollt jede Kugel, die oben aus dem Bogen kommt,
  // an der Wand entlang geradewegs in die Outlane, und jeder Ball ist nach
  // einer Sekunde weg. Die Schraege kippt sie nach innen auf den Slingshot zu;
  // in die Outlane kommt sie dann nur noch ueber deren Kante, also ueber einen
  // Abpraller. Genau diese Rolle haben die Rueckfuehrungen am echten Automaten.
  {   8, 120,  27, 139, SegWall,  REST_WALL_PCT },
  { 174, 120, 155, 139, SegWall,  REST_WALL_PCT },
  // Linker Slingshot: Innenkante (Gummi), Aussenkante an der Outlane, Unterkante
  {  30, 150,  52, 190, SegSling, REST_SLING_PCT },
  {  30, 150,  20, 190, SegWall,  REST_WALL_PCT },
  {  20, 190,  52, 190, SegWall,  REST_WALL_PCT },
  // Rechter Slingshot, an x = 91 gespiegelt (die Plunger-Bahn frisst rechts Platz)
  { 152, 150, 130, 190, SegSling, REST_SLING_PCT },
  { 152, 150, 162, 190, SegWall,  REST_WALL_PCT },
  { 130, 190, 162, 190, SegWall,  REST_WALL_PCT },
};

typedef struct {
  int16_t cx, cy, r;
  uint8_t kind;
  uint8_t rest_pct;
} CircDef;

static const CircDef s_circs[] = {
  {  91,  70, 11, CircBumper, REST_WALL_PCT },
  {  56, 104,  5, CircPost,   REST_POST_PCT },
  { 126, 104,  5, CircPost,   REST_POST_PCT },
};

#define DEG_TO_TRIG(d) ((int32_t)(((int32_t)(d) * TRIG_MAX_ANGLE) / 360))

// Ab dieser Hoehe wandert alles mit der Streckung nach unten. Die Linie liegt
// unter den Pfosten und ueber den Rueckfuehrungen, laeuft also nur durch die
// beiden senkrechten Seitenwaende und die Trennwand der Abschussbahn: Genau
// dort laesst sich ein gerades Stueck einfuegen, ohne die Form zu veraendern.
#define STRETCH_ANCHOR_Y 115
// Oberkante der Abschussbahn: Dort sitzt das Einwegtor, so hoch muss die
// Kugel mindestens kommen.
#define GATE_TOP_Y 54

static int16_t prv_y(int16_t y, int16_t stretch) {
  return (int16_t)(y >= STRETCH_ANCHOR_Y ? y + stretch : y);
}

static void prv_init_segment(Segment *s, const SegDef *d, int16_t stretch) {
  s->a = vec_make(FX_FROM_INT(d->ax), FX_FROM_INT(prv_y(d->ay, stretch)));
  s->b = vec_make(FX_FROM_INT(d->bx), FX_FROM_INT(prv_y(d->by, stretch)));
  Vec ab = vec_sub(s->b, s->a);
  s->len = vec_len(ab);
  if (s->len > 0) {
    s->dir = vec_make(fx_div(ab.x, s->len), fx_div(ab.y, s->len));
  } else {
    s->dir = vec_make(FX_ONE, 0);
  }
  s->n = vec_make(s->dir.y, -s->dir.x);
  s->kind = d->kind;
  s->rest_pct = d->rest_pct;
}

void table_build(Table *t, int16_t stretch_px) {
  if (stretch_px < 0) {
    stretch_px = 0;
  }
  t->stretch = stretch_px;
  t->height_px = (int16_t)(TABLE_H + stretch_px);
  t->drain_y = (int16_t)(DRAIN_Y + stretch_px);
  t->mag_dead_y = (int16_t)(MAG_DEAD_Y + stretch_px);
  t->plunger_y = (int16_t)(PLUNGER_REST_Y + stretch_px);

  t->seg_count = (uint8_t)(sizeof(s_segs) / sizeof(s_segs[0]));
  for (uint8_t i = 0; i < t->seg_count; i++) {
    prv_init_segment(&t->seg[i], &s_segs[i], stretch_px);
  }
  t->circ_count = (uint8_t)(sizeof(s_circs) / sizeof(s_circs[0]));
  for (uint8_t i = 0; i < t->circ_count; i++) {
    const CircDef *d = &s_circs[i];
    Circle *c = &t->circ[i];
    int16_t cy = prv_y(d->cy, stretch_px);
    c->c = vec_make(FX_FROM_INT(d->cx), FX_FROM_INT(cy));
    c->r = FX_FROM_INT(d->r);
    c->kind = d->kind;
    c->rest_pct = d->rest_pct;
    c->cx_px = d->cx;
    c->cy_px = cy;
    c->r_px = d->r;
  }
  // Ein gestreckter Tisch bekommt in der neuen Mitte einen zweiten Bumper,
  // sonst ist das eingefuegte Stueck eine leere Flaeche, auf der nichts
  // passiert und an der die Kamera nichts zu zeigen hat.
  if (stretch_px >= 60 && t->circ_count < TABLE_MAX_CIRCLES) {
    Circle *c = &t->circ[t->circ_count++];
    c->cx_px = 91;
    c->cy_px = (int16_t)(STRETCH_ANCHOR_Y + stretch_px / 2);
    c->r_px = 11;
    c->c = vec_make(FX_FROM_INT(c->cx_px), FX_FROM_INT(c->cy_px));
    c->r = FX_FROM_INT(c->r_px);
    c->kind = CircBumper;
    c->rest_pct = REST_WALL_PCT;
  }

  // Flipper: Winkel 0 zeigt nach rechts. Der linke Flipper ruht bei +30 Grad
  // (Spitze rechts unten) und schlaegt auf -30 Grad; der rechte spiegelt das
  // um 180 Grad, damit beide nach oben schlagen.
  const int16_t piv_y = (int16_t)(FLIPPER_L_PIVOT_Y + stretch_px);
  Flipper *l = &t->flip[0];
  l->pivot = vec_make(FX_FROM_INT(FLIPPER_L_PIVOT_X), FX_FROM_INT(piv_y));
  l->len = FX_FROM_INT(FLIPPER_LEN_PX);
  l->radius = FX_FROM_INT(FLIPPER_R_PX);
  l->angle_rest = DEG_TO_TRIG(FLIPPER_SWING_DEG);
  l->angle_active = -DEG_TO_TRIG(FLIPPER_SWING_DEG);
  l->angle = l->angle_rest;
  l->angle_prev = l->angle_rest;
  l->up = false;
  l->left = true;
  l->pivot_x_px = FLIPPER_L_PIVOT_X;
  l->pivot_y_px = piv_y;

  Flipper *r = &t->flip[1];
  r->pivot = vec_make(FX_FROM_INT(FLIPPER_R_PIVOT_X), FX_FROM_INT(piv_y));
  r->len = FX_FROM_INT(FLIPPER_LEN_PX);
  r->radius = FX_FROM_INT(FLIPPER_R_PX);
  r->angle_rest = DEG_TO_TRIG(180 - FLIPPER_SWING_DEG);
  r->angle_active = DEG_TO_TRIG(180 + FLIPPER_SWING_DEG);
  r->angle = r->angle_rest;
  r->angle_prev = r->angle_rest;
  r->up = false;
  r->left = false;
  r->pivot_x_px = FLIPPER_R_PIVOT_X;
  r->pivot_y_px = piv_y;
}

// Ganzzahlige Wurzel nach Newton; die Physik selbst rechnet in Q12, hier geht
// es aber um einen glatten Pixelwert.
static int32_t prv_isqrt(int32_t v) {
  if (v <= 0) {
    return 0;
  }
  int32_t x = v;
  int32_t y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + v / x) / 2;
  }
  return x;
}

int16_t table_plunger_min_speed(const Table *t, int32_t gravity_px_s2) {
  int32_t h = t->plunger_y - GATE_TOP_Y;
  if (h < 20) {
    h = 20;
  }
  int32_t v = prv_isqrt(2 * gravity_px_s2 * h);
  v = (v * 115) / 100;      // 15 Prozent Reserve fuer Reibung und Abpraller
  return (int16_t)v;
}

Vec table_plunger_pos(const Table *t) {
  return vec_make(FX_FROM_INT(PLUNGER_X), FX_FROM_INT(t->plunger_y));
}
