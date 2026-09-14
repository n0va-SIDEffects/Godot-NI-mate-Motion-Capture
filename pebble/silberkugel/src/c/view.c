#include "view.h"
#include "config.h"

// Hochformat: Bildschirm-x ist die Tischbreite, Bildschirm-y die Tischlaenge.
//   sx = tx,  sy = ty - cam
// Querformat: Die Tischlaenge laeuft nach rechts, die Tischbreite nach unten,
// und die Flipper liegen links. Dreht man die Uhr um 90 Grad gegen den
// Uhrzeigersinn (linke Gehaeuseseite nach unten), steht der Tisch wieder
// aufrecht vor einem.
//   sx = (view_len - 1) - (ty - cam),  sy = tx + wid_off

static void prv_geom(View *v, const Table *t) {
  if (v->rot90) {
    v->view_len = SCR_W;                 // sichtbare Tischlaenge
    v->view_wid = SCR_H;                 // Platz fuer die Tischbreite
  } else {
    v->view_len = SCR_H;
    v->view_wid = SCR_W;
  }
  v->wid_off = (int16_t)((v->view_wid - TABLE_W) / 2);
  if (v->wid_off < 0) {
    v->wid_off = 0;
  }
  int16_t max = (int16_t)(t->height_px - v->view_len);
  v->cam_max = max > 0 ? max : 0;
  if (!v->camera) {
    // Ohne Kamera zeigt der Ausschnitt das untere Ende des Tisches: Dort
    // stehen die Flipper, und ohne sie ist nicht zu spielen.
    v->cam = v->cam_max;
  }
  if (v->cam > v->cam_max) {
    v->cam = v->cam_max;
  }
}

void view_init(View *v, const Table *t) {
  v->rot90 = false;
  v->camera = false;
  v->cam = 0;
  prv_geom(v, t);
}

void view_set_rot(View *v, bool rot90, const Table *t) {
  v->rot90 = rot90;
  prv_geom(v, t);
}

void view_set_camera(View *v, bool on, const Table *t) {
  v->camera = on;
  prv_geom(v, t);
}

void view_track(View *v, int16_t ball_y) {
  if (!v->camera || v->cam_max == 0 || ball_y < 0) {
    return;
  }
  // Totband: Erst wenn die Kugel den mittleren Streifen verlaesst, zieht die
  // Kamera nach, und dann nur bis an den Rand des Streifens. Das ist die
  // Hysterese aus dem Konzept; ohne sie folgt das Bild jedem Zittern.
  int16_t rel = (int16_t)(ball_y - v->cam);
  int16_t band = (int16_t)((v->view_len - CAM_DEADBAND_PX) / 2);
  if (band < 10) {
    band = 10;
  }
  if (rel < band) {
    v->cam = (int16_t)(ball_y - band);
  } else if (rel > v->view_len - band) {
    v->cam = (int16_t)(ball_y - (v->view_len - band));
  }
  if (v->cam < 0) {
    v->cam = 0;
  }
  if (v->cam > v->cam_max) {
    v->cam = v->cam_max;
  }
}

void view_snap(View *v, int16_t ball_y) {
  if (!v->camera || v->cam_max == 0 || ball_y < 0) {
    return;
  }
  v->cam = (int16_t)(ball_y - v->view_len / 2);
  if (v->cam < 0) {
    v->cam = 0;
  }
  if (v->cam > v->cam_max) {
    v->cam = v->cam_max;
  }
}

void view_to_screen(const View *v, int16_t tx, int16_t ty, int16_t *sx, int16_t *sy) {
  int16_t along = (int16_t)(ty - v->cam);
  if (v->rot90) {
    *sx = (int16_t)(v->view_len - 1 - along);
    *sy = (int16_t)(tx + v->wid_off);
  } else {
    *sx = (int16_t)(tx + v->wid_off);
    *sy = along;
  }
}

void view_to_table(const View *v, int16_t sx, int16_t sy, int16_t *tx, int16_t *ty) {
  if (v->rot90) {
    *tx = (int16_t)(sy - v->wid_off);
    *ty = (int16_t)(v->view_len - 1 - sx + v->cam);
  } else {
    *tx = (int16_t)(sx - v->wid_off);
    *ty = (int16_t)(sy + v->cam);
  }
}

void view_dir_to_table(const View *v, fix dx, fix dy, fix *tx, fix *ty) {
  if (v->rot90) {
    *tx = dy;
    *ty = -dx;
  } else {
    *tx = dx;
    *ty = dy;
  }
}

bool view_visible(const View *v, int16_t tx, int16_t ty) {
  int16_t along = (int16_t)(ty - v->cam);
  return along >= -8 && along < v->view_len + 8 && tx >= -8 && tx < TABLE_W + 8;
}
