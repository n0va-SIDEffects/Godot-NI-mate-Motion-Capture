// Minimaler Ersatz fuer pebble.h, damit sich die Physik auf dem Rechner
// uebersetzen und pruefen laesst. Nur das, was fixed.h, table.c und
// physics.c wirklich benutzen: Trigonometrie in Pebble-Einheiten, memset,
// die Ganzzahltypen. Die Winkelfunktionen sind bitgleich zur Uhr, weil sie
// aus derselben Definition kommen: 65536 Einheiten fuer den vollen Kreis,
// Ergebnis in Q16.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define P1_PI 3.14159265358979323846

#define TRIG_MAX_ANGLE 0x10000
#define TRIG_MAX_RATIO 0xffff

static inline int32_t sin_lookup(int32_t angle) {
  double a = (double)angle * 2.0 * P1_PI / (double)TRIG_MAX_ANGLE;
  return (int32_t)lround(sin(a) * 65536.0);
}

static inline int32_t cos_lookup(int32_t angle) {
  double a = (double)angle * 2.0 * P1_PI / (double)TRIG_MAX_ANGLE;
  return (int32_t)lround(cos(a) * 65536.0);
}
