#include "physics.h"

// Fester Zeitschritt von 5 ms. Geschwindigkeiten stehen in px/s, deshalb ist
// der Weg eines Substeps v / PHYS_HZ. Die Division durch eine Konstante
// uebersetzt der Compiler in eine Multiplikation, sie kostet also nichts.

#define BALL_R FX_FROM_INT(BALL_R_PX)
#define VEL_MAX FX_FROM_INT(VEL_MAX_PX_S)

// Winkelschritt pro Substep, aus der Hubzeit gerechnet.
#define SWING_TRIG ((int32_t)((2 * FLIPPER_SWING_DEG * (int32_t)TRIG_MAX_ANGLE) / 360))
#define FLIP_STEP_UP   (SWING_TRIG * PHYS_DT_MS / FLIPPER_UP_MS)
#define FLIP_STEP_DOWN (SWING_TRIG * PHYS_DT_MS / FLIPPER_DOWN_MS)

Vec phys_flipper_tip(const Flipper *f) {
  return vec_add(f->pivot, vec_rot(vec_make(f->len, 0), f->angle));
}

void phys_init(World *w) {
  memset(w, 0, sizeof(*w));
  table_build(&w->table);
  w->gravity = vec_make(0, FX_FROM_INT(GRAVITY_PX_S2));
  w->ball_count = 0;
  phys_reset_stats(w);
}

void phys_reset_stats(World *w) {
  memset(&w->st, 0, sizeof(w->st));
}

uint8_t phys_alive_count(const World *w) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < w->ball_count; i++) {
    if (w->ball[i].alive) {
      n++;
    }
  }
  return n;
}

void phys_clear_balls(World *w) {
  for (uint8_t i = 0; i < MAX_BALLS; i++) {
    w->ball[i].alive = false;
    w->ball[i].held = false;
  }
  w->ball_count = 0;
}

Ball *phys_spawn_lane(World *w) {
  for (uint8_t i = 0; i < MAX_BALLS; i++) {
    Ball *b = &w->ball[i];
    if (b->alive) {
      continue;
    }
    memset(b, 0, sizeof(*b));
    b->p = table_plunger_pos();
    b->v = vec_make(0, 0);
    b->alive = true;
    b->in_lane = true;
    for (uint8_t k = 0; k < BALL_TRAIL; k++) {
      b->trail[k] = b->p;
    }
    b->trail_n = BALL_TRAIL;
    if (i >= w->ball_count) {
      w->ball_count = (uint8_t)(i + 1);
    }
    return b;
  }
  return NULL;
}

void phys_set_flipper(World *w, uint8_t idx, bool up) {
  if (idx < 2) {
    w->table.flip[idx].up = up;
  }
}

void phys_nudge(World *w, fix dvx, fix dvy) {
  for (uint8_t i = 0; i < w->ball_count; i++) {
    Ball *b = &w->ball[i];
    if (!b->alive || b->held) {
      continue;
    }
    b->v.x += dvx;
    b->v.y += dvy;
  }
}

// --------------------------------------------------------------- Kollisionen

// Abprall an einer Flaeche mit Normale n. vn ist bereits bekannt und negativ
// (Annaeherung). Die Reibung greift den Tangentialanteil nur so weit an, wie
// der Normalimpuls es zulaesst (Coulomb): Ein harter Stoss bremst spuerbar,
// eine anliegende Kugel kaum. Damit haengt die Bremsung an der Physik und
// nicht an der Zahl der Substeps.
static void prv_bounce(Ball *b, Vec n, fix vn, uint8_t rest_pct, Vec surface_v) {
  Vec vrel = vec_sub(b->v, surface_v);
  Vec vn_vec = vec_scale(n, vn);
  Vec vt = vec_sub(vrel, vn_vec);
  fix e = FXQ(rest_pct, 100);
  fix jn = -fx_mul(vn, FX_ONE + e);           // Normalimpuls, positiv
  fix vt_len = vec_len(vt);
  if (vt_len > 0) {
    fix max_fr = fx_mul(jn, FXQ(FRICTION_MU_PCT, 100));
    fix reduce = vt_len < max_fr ? vt_len : max_fr;
    vt = vec_scale(vt, fx_div(vt_len - reduce, vt_len));
  }
  Vec vn_new = vec_scale(n, -fx_mul(vn, e));
  b->v = vec_add(surface_v, vec_add(vt, vn_new));
}

static bool prv_hit_segment(World *w, Ball *b, const Segment *s) {
  w->st.narrow_tests++;
  Vec ap = vec_sub(b->p, s->a);
  fix t = vec_dot(ap, s->dir);
  if (t < 0) {
    t = 0;
  } else if (t > s->len) {
    t = s->len;
  }
  Vec closest = vec_add(s->a, vec_scale(s->dir, t));
  Vec d = vec_sub(b->p, closest);
  // Erst ohne Wurzel pruefen: der Normalfall ist "kein Kontakt", und der soll
  // billig sein.
  int64_t d2 = vec_len2_raw(d);
  int64_t r2 = (int64_t)BALL_R * BALL_R;
  if (d2 >= r2) {
    return false;
  }
  // Einwegtor: sperrt nur, wenn die Kugel auf der Seite liegt, in die die
  // Normale zeigt. Von der anderen Seite faehrt sie durch.
  if (s->kind == SegGate && vec_dot(ap, s->n) < 0) {
    return false;
  }
  fix dist = vec_len(d);
  Vec n = (dist > 0) ? vec_make(fx_div(d.x, dist), fx_div(d.y, dist)) : s->n;
  b->p = vec_add(b->p, vec_scale(n, BALL_R - dist));
  fix vn = vec_dot(b->v, n);
  if (vn >= 0) {
    return false;   // beruehrt, entfernt sich aber schon: nur entklemmen
  }
  prv_bounce(b, n, vn, s->rest_pct, vec_make(0, 0));
  w->st.contacts_seg++;
  if (s->kind == SegSling) {
    b->v = vec_add(b->v, vec_scale(n, FX_FROM_INT(SLING_KICK_PX_S)));
    w->ev.sling++;
  } else if (s->kind == SegGate) {
    w->ev.gate++;
  } else {
    w->ev.wall++;
  }
  return true;
}

static bool prv_hit_circle(World *w, Ball *b, const Circle *c) {
  w->st.narrow_tests++;
  Vec d = vec_sub(b->p, c->c);
  fix rsum = c->r + BALL_R;
  int64_t d2 = vec_len2_raw(d);
  int64_t r2 = (int64_t)rsum * rsum;
  if (d2 >= r2) {
    return false;
  }
  fix dist = vec_len(d);
  Vec n = (dist > 0) ? vec_make(fx_div(d.x, dist), fx_div(d.y, dist)) : vec_make(0, -FX_ONE);
  b->p = vec_add(b->p, vec_scale(n, rsum - dist));
  fix vn = vec_dot(b->v, n);
  if (vn >= 0) {
    return false;
  }
  prv_bounce(b, n, vn, c->rest_pct, vec_make(0, 0));
  w->st.contacts_circ++;
  if (c->kind == CircBumper) {
    if (b->bumper_cd_ms == 0) {
      b->v = vec_add(b->v, vec_scale(n, FX_FROM_INT(BUMPER_KICK_PX_S)));
      b->bumper_cd_ms = BUMPER_COOLDOWN_MS;
      w->ev.bumper++;
    }
  } else {
    w->ev.wall++;
  }
  return true;
}

// Flipper als rotierende Kapsel. Der Uebertrag der Winkelgeschwindigkeit kommt
// nicht aus einer Radiant-Rechnung, sondern daraus, wohin der Kontaktpunkt im
// letzten Substep gewandert ist: Das ist exakt dieselbe Groesse, spart die
// Umrechnung und stimmt auch am Anschlag, wo der Flipper steht.
static bool prv_hit_flipper(World *w, Ball *b, const Flipper *f) {
  w->st.narrow_tests++;
  Vec tip = phys_flipper_tip(f);
  Vec ab = vec_sub(tip, f->pivot);
  fix len = f->len;
  Vec dir = vec_make(fx_div(ab.x, len), fx_div(ab.y, len));
  Vec ap = vec_sub(b->p, f->pivot);
  fix t = vec_dot(ap, dir);
  if (t < 0) {
    t = 0;
  } else if (t > len) {
    t = len;
  }
  Vec closest = vec_add(f->pivot, vec_scale(dir, t));
  Vec d = vec_sub(b->p, closest);
  fix rsum = f->radius + BALL_R;
  int64_t d2 = vec_len2_raw(d);
  int64_t r2 = (int64_t)rsum * rsum;
  if (d2 >= r2) {
    return false;
  }
  fix dist = vec_len(d);
  Vec n;
  if (dist > 0) {
    n = vec_make(fx_div(d.x, dist), fx_div(d.y, dist));
  } else {
    n = vec_make(dir.y, -dir.x);
  }
  b->p = vec_add(b->p, vec_scale(n, rsum - dist));

  // Geschwindigkeit des Kontaktpunkts: Wanderung im letzten Substep mal Rate.
  Vec surface_v = vec_make(0, 0);
  int32_t dtheta = f->angle - f->angle_prev;
  if (dtheta != 0) {
    Vec rvec = vec_sub(closest, f->pivot);
    Vec moved = vec_sub(vec_rot(rvec, dtheta), rvec);
    surface_v = vec_scale(moved, FX_FROM_INT(PHYS_HZ));
  }
  fix vn = vec_dot(vec_sub(b->v, surface_v), n);
  if (vn >= 0) {
    return false;
  }
  prv_bounce(b, n, vn, REST_FLIPPER_PCT, surface_v);
  w->st.contacts_flip++;
  w->ev.flipper++;
  return true;
}

// Alle Koerper gegen eine Kugel. Zwei Durchgaenge, damit eine Kugel, die in
// eine Ecke geraet (Flipper gegen Bande), nicht durch die zweite Flaeche
// geschoben wird. Mehr als zwei bringt nichts und kostet nur.
static void prv_resolve(World *w, Ball *b) {
  for (int pass = 0; pass < 2; pass++) {
    bool any = false;
    for (uint8_t i = 0; i < w->table.seg_count; i++) {
      any |= prv_hit_segment(w, b, &w->table.seg[i]);
    }
    for (uint8_t i = 0; i < w->table.circ_count; i++) {
      any |= prv_hit_circle(w, b, &w->table.circ[i]);
    }
    for (uint8_t i = 0; i < 2; i++) {
      any |= prv_hit_flipper(w, b, &w->table.flip[i]);
    }
    if (!any) {
      break;
    }
  }
}

// --------------------------------------------------------------- Zeitschritt

static void prv_step_flippers(World *w) {
  for (uint8_t i = 0; i < 2; i++) {
    Flipper *f = &w->table.flip[i];
    f->angle_prev = f->angle;
    int32_t target = f->up ? f->angle_active : f->angle_rest;
    int32_t step = f->up ? FLIP_STEP_UP : FLIP_STEP_DOWN;
    int32_t diff = target - f->angle;
    if (diff > step) {
      f->angle += step;
    } else if (diff < -step) {
      f->angle -= step;
    } else {
      f->angle = target;
    }
  }
}

static void prv_apply_forces(World *w, Ball *b) {
  Vec a = w->gravity;
  if (w->mag_on && b->p.y < FX_FROM_INT(MAG_DEAD_Y)) {
    // Magnetfinger: Kraft faellt mit 1/(r^2 + r0^2) ab, wie im Konzept. Der
    // weiche Kern r0 verhindert, dass die Kraft direkt unter der Kuppe
    // explodiert und die Kugel durch den Tisch schiesst.
    Vec d = vec_sub(w->mag_pos, b->p);
    int64_t d2 = vec_len2_raw(d);
    int64_t rad2 = (int64_t)FX_FROM_INT(MAG_RADIUS_PX) * FX_FROM_INT(MAG_RADIUS_PX);
    if (d2 < rad2) {
      fix dist = vec_len(d);
      int32_t dist_px = FX_TO_INT(dist);
      int32_t denom = dist_px * dist_px + (MAG_SOFT_PX * MAG_SOFT_PX);
      fix strength = (fix)(((int64_t)w->mag_accel * (MAG_SOFT_PX * MAG_SOFT_PX)) / denom);
      if (dist > 0) {
        Vec n = vec_make(fx_div(d.x, dist), fx_div(d.y, dist));
        a = vec_add(a, vec_scale(n, strength));
      }
    }
  }
  b->v.x += a.x / PHYS_HZ;
  b->v.y += a.y / PHYS_HZ;

  // Leichte Dauerdaempfung: sonst rollt die Kugel auf waagerechten Flaechen
  // ewig hin und her, und ein Prototyp soll zur Ruhe kommen koennen. Ein
  // Q12-Schritt pro Substep sind rund 4,8 Prozent pro Sekunde.
  b->v = vec_scale(b->v, FX_ONE - 1);
}

static void prv_move(World *w, Ball *b) {
  // Weg dieses Substeps. Kein Teilschritt darf laenger als MAX_STEP sein,
  // sonst kann die Kugel eine unendlich duenne Bande ueberspringen. Bei
  // 1500 px/s sind das sieben Teilschritte, im Normalfall einer oder zwei.
  fix speed = vec_len(b->v);
  // Sicherheitsklemme hier statt bei den Kraeften: Die Laenge ist an dieser
  // Stelle ohnehin schon berechnet.
  if (speed > VEL_MAX) {
    b->v = vec_scale(b->v, fx_div(VEL_MAX, speed));
    speed = VEL_MAX;
  }
  if (speed > w->st.speed_max) {
    w->st.speed_max = speed;
  }
  fix step_len = speed / PHYS_HZ;                  // Weg pro Substep in px (Q12)
  fix max_step = (fix)((FX_ONE * MAX_STEP_Q4) / 16);
  int32_t n = 1;
  if (step_len > max_step) {
    n = (int32_t)((step_len + max_step - 1) / max_step);
    if (n > 16) {
      n = 16;    // 16 Teilschritte sind 0,3 ms Weg: darunter tunnelt nichts mehr
    }
  }
  if ((uint32_t)n > w->st.splits_max) {
    w->st.splits_max = (uint32_t)n;
  }
  for (int32_t i = 0; i < n; i++) {
    // Immer mit der aktuellen Geschwindigkeit rechnen: aendert ein Kontakt sie,
    // laeuft der Rest des Substeps schon in die neue Richtung.
    b->p.x += b->v.x / (PHYS_HZ * n);
    b->p.y += b->v.y / (PHYS_HZ * n);
    prv_resolve(w, b);
  }
}

void phys_substep(World *w) {
  w->st.substeps++;
  prv_step_flippers(w);
  for (uint8_t i = 0; i < w->ball_count; i++) {
    Ball *b = &w->ball[i];
    if (!b->alive) {
      continue;
    }
    if (b->bumper_cd_ms > 0) {
      b->bumper_cd_ms = (b->bumper_cd_ms > PHYS_DT_MS) ? b->bumper_cd_ms - PHYS_DT_MS : 0;
    }
    if (b->held) {
      continue;   // der Magnetgriff fuehrt die Kugel, die Physik ruht
    }
    prv_apply_forces(w, b);
    prv_move(w, b);

    if (b->p.y > FX_FROM_INT(DRAIN_Y)) {
      b->alive = false;
      w->ev.drain++;
      continue;
    }
    // Sicherheitsnetz: Wer trotz Teilschritten aus dem Tisch geraet, wird
    // gezaehlt und zurueckgeholt. Steht hier je etwas anderes als 0, stimmt
    // die Kollisionsaufloesung nicht.
    if (b->p.x < 0 || b->p.x > FX_FROM_INT(TABLE_W) || b->p.y < -FX_FROM_INT(8)) {
      w->st.escapes++;
      b->p.x = fx_clamp(b->p.x, FX_FROM_INT(BALL_R_PX + 1),
                        FX_FROM_INT(TABLE_W - BALL_R_PX - 1));
      b->p.y = fx_clamp(b->p.y, FX_FROM_INT(BALL_R_PX + 1), FX_FROM_INT(DRAIN_Y));
      b->v = vec_scale(b->v, FX_HALF);
    }
    if (b->in_lane && b->p.x < FX_FROM_INT(PLUNGER_LANE_X0)) {
      b->in_lane = false;
    }
  }
}

// Spur fortschreiben: einmal pro Bild, nicht pro Substep. Drei Positionen
// reichen, um bei 25 fps und 500 px/s die Luecke zwischen zwei Bildern als
// Bewegung lesbar zu machen.
void phys_push_trail(World *w) {
  for (uint8_t i = 0; i < w->ball_count; i++) {
    Ball *b = &w->ball[i];
    if (!b->alive) {
      continue;
    }
    for (uint8_t k = BALL_TRAIL - 1; k > 0; k--) {
      b->trail[k] = b->trail[k - 1];
    }
    b->trail[0] = b->p;
  }
}
