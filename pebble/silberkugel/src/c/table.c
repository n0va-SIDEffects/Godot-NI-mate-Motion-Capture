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

static void prv_init_segment(Segment *s, const SegDef *d) {
  s->a = vec_make(FX_FROM_INT(d->ax), FX_FROM_INT(d->ay));
  s->b = vec_make(FX_FROM_INT(d->bx), FX_FROM_INT(d->by));
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

void table_build(Table *t) {
  t->seg_count = (uint8_t)(sizeof(s_segs) / sizeof(s_segs[0]));
  for (uint8_t i = 0; i < t->seg_count; i++) {
    prv_init_segment(&t->seg[i], &s_segs[i]);
  }
  t->circ_count = (uint8_t)(sizeof(s_circs) / sizeof(s_circs[0]));
  for (uint8_t i = 0; i < t->circ_count; i++) {
    const CircDef *d = &s_circs[i];
    Circle *c = &t->circ[i];
    c->c = vec_make(FX_FROM_INT(d->cx), FX_FROM_INT(d->cy));
    c->r = FX_FROM_INT(d->r);
    c->kind = d->kind;
    c->rest_pct = d->rest_pct;
    c->cx_px = d->cx;
    c->cy_px = d->cy;
    c->r_px = d->r;
  }

  // Flipper: Winkel 0 zeigt nach rechts. Der linke Flipper ruht bei +30 Grad
  // (Spitze rechts unten) und schlaegt auf -30 Grad; der rechte spiegelt das
  // um 180 Grad, damit beide nach oben schlagen.
  Flipper *l = &t->flip[0];
  l->pivot = vec_make(FX_FROM_INT(FLIPPER_L_PIVOT_X), FX_FROM_INT(FLIPPER_L_PIVOT_Y));
  l->len = FX_FROM_INT(FLIPPER_LEN_PX);
  l->radius = FX_FROM_INT(FLIPPER_R_PX);
  l->angle_rest = DEG_TO_TRIG(FLIPPER_SWING_DEG);
  l->angle_active = -DEG_TO_TRIG(FLIPPER_SWING_DEG);
  l->angle = l->angle_rest;
  l->angle_prev = l->angle_rest;
  l->up = false;
  l->left = true;
  l->pivot_x_px = FLIPPER_L_PIVOT_X;
  l->pivot_y_px = FLIPPER_L_PIVOT_Y;

  Flipper *r = &t->flip[1];
  r->pivot = vec_make(FX_FROM_INT(FLIPPER_R_PIVOT_X), FX_FROM_INT(FLIPPER_R_PIVOT_Y));
  r->len = FX_FROM_INT(FLIPPER_LEN_PX);
  r->radius = FX_FROM_INT(FLIPPER_R_PX);
  r->angle_rest = DEG_TO_TRIG(180 - FLIPPER_SWING_DEG);
  r->angle_active = DEG_TO_TRIG(180 + FLIPPER_SWING_DEG);
  r->angle = r->angle_rest;
  r->angle_prev = r->angle_rest;
  r->up = false;
  r->left = false;
  r->pivot_x_px = FLIPPER_R_PIVOT_X;
  r->pivot_y_px = FLIPPER_R_PIVOT_Y;
}

Vec table_plunger_pos(void) {
  return vec_make(FX_FROM_INT(PLUNGER_X), FX_FROM_INT(PLUNGER_REST_Y));
}
