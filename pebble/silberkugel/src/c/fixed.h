#pragma once
#include <pebble.h>

// Q20.12 in int32: 12 Nachkommabits, 1,0 = 4096.
// Wertebereich +-524288,0 - der Tisch ist 200x228 Pixel gross, Geschwindigkeiten
// bleiben unter 2000 px/s, also liegt alles weit innerhalb.
// Alle Laengen sind Pixel, alle Geschwindigkeiten Pixel pro Sekunde,
// alle Beschleunigungen Pixel pro Sekunde im Quadrat.
//
// Warum ueberhaupt Fixed-Point: Die Physik soll deterministisch sein, damit
// Replay, Tagestisch und Regressionstest aus dem Konzept spaeter tragen. Der
// Cortex-M4 der Pebble Time 2 hat zwar eine FPU, aber float bringt hier nichts
// als Nichtdeterminismus zwischen Compilerversionen und Optimierungsstufen.

typedef int32_t fix;

#define FX_BITS 12
#define FX_ONE  (1 << FX_BITS)
#define FX_HALF (FX_ONE / 2)

#define FX(whole)            ((fix)((whole) * FX_ONE))
// Bruch ohne Float im Praeprozessor: FXQ(3, 2) = 1,5
#define FXQ(num, den)        ((fix)(((int32_t)(num) * FX_ONE) / (den)))
#define FX_FROM_INT(i)       ((fix)((i) << FX_BITS))
#define FX_TO_INT(f)         ((int32_t)((f) >> FX_BITS))
// Runden statt abschneiden, damit die Kugel beim Zeichnen nicht bei jedem
// Vorzeichenwechsel um ein Pixel springt.
#define FX_TO_INT_R(f)       ((int32_t)(((f) + FX_HALF) >> FX_BITS))

typedef struct {
  fix x;
  fix y;
} Vec;

static inline fix fx_mul(fix a, fix b) {
  return (fix)(((int64_t)a * (int64_t)b) >> FX_BITS);
}

static inline fix fx_div(fix a, fix b) {
  if (b == 0) {
    return 0;
  }
  return (fix)((((int64_t)a) << FX_BITS) / b);
}

static inline fix fx_abs(fix a) {
  return a < 0 ? -a : a;
}

static inline fix fx_min(fix a, fix b) {
  return a < b ? a : b;
}

static inline fix fx_max(fix a, fix b) {
  return a > b ? a : b;
}

static inline fix fx_clamp(fix v, fix lo, fix hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Wurzel auf ganzzahliger Basis: erst den Rohwert auf 2*FX_BITS schieben,
// dann ganzzahlig wurzeln. Newton braucht hier keine Division durch Null,
// weil x == 0 vorher abgefangen wird. Sechs Iterationen reichen fuer 32 Bit.
static inline fix fx_sqrt(fix x) {
  if (x <= 0) {
    return 0;
  }
  uint64_t v = ((uint64_t)(uint32_t)x) << FX_BITS;
  // Startwert ueber die halbe Bitbreite: konvergiert in wenigen Schritten.
  uint32_t r = 1u << 16;
  while ((uint64_t)r * r > v) {
    r >>= 1;
  }
  if (r == 0) {
    r = 1;
  }
  for (int i = 0; i < 8; i++) {
    uint32_t next = (uint32_t)((r + v / r) >> 1);
    if (next == r) {
      break;
    }
    r = next;
  }
  return (fix)r;
}

static inline Vec vec_make(fix x, fix y) {
  Vec v = { x, y };
  return v;
}

static inline Vec vec_add(Vec a, Vec b) {
  return vec_make(a.x + b.x, a.y + b.y);
}

static inline Vec vec_sub(Vec a, Vec b) {
  return vec_make(a.x - b.x, a.y - b.y);
}

static inline Vec vec_scale(Vec a, fix s) {
  return vec_make(fx_mul(a.x, s), fx_mul(a.y, s));
}

static inline fix vec_dot(Vec a, Vec b) {
  return (fix)((((int64_t)a.x * b.x) + ((int64_t)a.y * b.y)) >> FX_BITS);
}

// Kreuzprodukt in der Ebene (z-Komponente). Vorzeichen sagt, auf welcher
// Seite ein Punkt liegt.
static inline fix vec_cross(Vec a, Vec b) {
  return (fix)((((int64_t)a.x * b.y) - ((int64_t)a.y * b.x)) >> FX_BITS);
}

// Quadrierte Laenge als int64, damit auch 2000 px/s nicht ueberlaeuft:
// (2000 * 4096)^2 passt nicht in int32, das Zwischenergebnis also nicht kuerzen.
static inline int64_t vec_len2_raw(Vec a) {
  return ((int64_t)a.x * a.x) + ((int64_t)a.y * a.y);
}

static inline fix vec_len(Vec a) {
  int64_t l2 = vec_len2_raw(a);
  if (l2 == 0) {
    return 0;
  }
  // Ganzzahlige Wurzel aus dem ungekuerzten Quadrat: das Ergebnis liegt
  // bereits wieder im Q12-Format, weil sqrt(v^2 * 2^24) = v * 2^12.
  uint64_t v = (uint64_t)l2;
  uint32_t r = 1u << 24;
  while ((uint64_t)r * r > v) {
    r >>= 1;
  }
  if (r == 0) {
    r = 1;
  }
  for (int i = 0; i < 10; i++) {
    uint32_t next = (uint32_t)((r + v / r) >> 1);
    if (next == r) {
      break;
    }
    r = next;
  }
  return (fix)r;
}

// Einheitsvektor. Gibt bei Laenge 0 den Nullvektor zurueck; der Aufrufer
// muss diesen Fall behandeln (Kugelmittelpunkt genau auf der Wand).
static inline Vec vec_norm(Vec a) {
  fix l = vec_len(a);
  if (l == 0) {
    return vec_make(0, 0);
  }
  return vec_make(fx_div(a.x, l), fx_div(a.y, l));
}

// Dreht einen Vektor um einen Winkel in Pebble-Trigonometrieeinheiten
// (TRIG_MAX_ANGLE = 65536 entspricht einer vollen Umdrehung).
static inline Vec vec_rot(Vec a, int32_t angle) {
  int32_t c = cos_lookup(angle);   // Q16
  int32_t s = sin_lookup(angle);
  fix rx = (fix)((((int64_t)a.x * c) - ((int64_t)a.y * s)) >> 16);
  fix ry = (fix)((((int64_t)a.x * s) + ((int64_t)a.y * c)) >> 16);
  return vec_make(rx, ry);
}
