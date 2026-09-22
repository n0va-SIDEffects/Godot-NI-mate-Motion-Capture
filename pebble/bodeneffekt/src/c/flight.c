#include "flight.h"
#include "world.h"
#include "control.h"
#include "setup.h"

static Flight s_f;

void flight_reset(int32_t x16, int32_t y16) {
  memset(&s_f, 0, sizeof(s_f));
  s_f.x16 = x16;
  s_f.y16 = y16;
  s_f.ground8 = world_height_at(x16, y16);
  s_f.h8 = s_f.ground8 + (8 << 8);
  // Hoehe ueber Grund gleich mitsetzen: sonst steht sie bis zum ersten
  // flight_step auf null, das HUD zeigt agl 0.0 und der Bodenschatten fehlt -
  // sichtbar waehrend des Countdowns, in dem nicht geflogen wird.
  s_f.agl8 = s_f.h8 - s_f.ground8;
  s_f.yaw = 0;
}

void flight_step(uint32_t dt_ms) {
  if (dt_ms == 0) return;
  if (dt_ms > 200) dt_ms = 200;          // nach einer Lastspitze nicht springen
  const int32_t dt = (int32_t)dt_ms;
  const CtrlOut *in = control_out();

  // Roll naehert sich dem Kommando, begrenzt auf +-15 Grad. Der begrenzte
  // Ausschlag ist kein Balancing, sondern Lesbarkeit: ein weiter kippender
  // Horizont ist auf 1,5 Zoll nicht mehr zu lesen.
  const int32_t target = (in->roll_cmd * ROLL_MAX_DEG * 256) / 256;
  const int32_t max_step = (ROLL_RATE_DEG_S * 256 * dt) / 1000;
  int32_t d = target - s_f.roll_deg8;
  if (d > max_step) d = max_step;
  if (d < -max_step) d = -max_step;
  s_f.roll_deg8 += d;
  const int32_t lim = ROLL_MAX_DEG * 256;
  if (s_f.roll_deg8 > lim) s_f.roll_deg8 = lim;
  if (s_f.roll_deg8 < -lim) s_f.roll_deg8 = -lim;

  // Kurvenflug: der Roll dreht den Kurs.
  const int32_t turn_deg8 = (s_f.roll_deg8 * TURN_DEG_S_PER_ROLL * dt) /
                            (ROLL_MAX_DEG * 1000);
  s_f.yaw = (s_f.yaw + ((turn_deg8 * (TRIG_MAX_ANGLE / 360)) >> 8)) & (TRIG_MAX_ANGLE - 1);

  // Vorwaerts mit Grundtempo.
  const int32_t dist16 = (SPEED_CELLS_S * 65536 / 1000) * dt;
  s_f.x16 += (int32_t)(((int64_t)sin_lookup(s_f.yaw) * dist16) >> 16);
  s_f.y16 += (int32_t)(((int64_t)cos_lookup(s_f.yaw) * dist16) >> 16);

  // Hoehe: zwei Modelle, damit sich beide Profile gegeneinander testen lassen.
  if (control_profile() != ProfTasten) {
    // Der Versatz (oder die Neigung) ist eine Steigrate, nicht eine Hoehe: der
    // Gleiter haelt die Hoehe, sobald die Eingabe zur Mitte zurueckkommt.
    s_f.vz8 = (in->climb_cmd * STICK_CLIMB_CELLS_S * 256) / 256;
  } else {
    // Flappy: Select liegt = steigen, sonst sinken.
    const int32_t acc = in->climb_held ? FLAP_ACC_CELLS_S2 : -FLAP_GRAV_CELLS_S2;
    s_f.vz8 += (acc * 256 * dt) / 1000;
    const int32_t vmax = FLAP_VZ_MAX_CELLS_S * 256;
    if (s_f.vz8 > vmax) s_f.vz8 = vmax;
    if (s_f.vz8 < -vmax) s_f.vz8 = -vmax;
  }
  s_f.h8 += (s_f.vz8 * dt) / 1000;
  s_f.h8 += (in->trim8 * dt) / 1000;

  s_f.ground8 = world_height_at(s_f.x16, s_f.y16);
  s_f.agl8 = s_f.h8 - s_f.ground8;

  // Der Bodenkontakt ist eine Flanke, kein Zustand: sonst blitzt das Bild bei
  // jedem Tick rot, solange der Gleiter aufliegt, und der Zaehler laeuft mit
  // 50 Ereignissen je Sekunde davon.
  s_f.hit = false;
  if (s_f.agl8 < GROUND_HIT_AGL) {
    if (!s_f.on_ground) {
      s_f.hit = true;
      s_f.contacts++;
    }
    s_f.on_ground = true;
    s_f.h8 = s_f.ground8 + GROUND_HIT_AGL;
    s_f.agl8 = GROUND_HIT_AGL;
    if (s_f.vz8 < 0) s_f.vz8 = 0;
  } else if (s_f.agl8 > GROUND_HIT_AGL + (1 << 8)) {
    s_f.on_ground = false;        // eine Zelle Hysterese gegen Flattern
  }
  const int32_t ceiling = (ALT_MAX << 8);
  if (s_f.h8 > ceiling) {
    s_f.h8 = ceiling;
    if (s_f.vz8 > 0) s_f.vz8 = 0;
    s_f.agl8 = s_f.h8 - s_f.ground8;
  }

  s_f.in_effect = (s_f.agl8 >= EFFECT_LO_AGL && s_f.agl8 <= EFFECT_HI_AGL);
  if (s_f.in_effect) s_f.effect_ms += dt_ms;
}

const Flight *flight_state(void) { return &s_f; }

void flight_fill_camera(Camera *cam) {
  // Die Kamera sitzt hinter und ueber dem Gleiter. Nur so steht der Gleiter
  // als Objekt in der Szene und sein Schatten an der geometrisch richtigen
  // Stelle; eine Kamera am Gleiterort wuerde den Schatten weit unter den
  // Bildrand schieben, sobald man ein paar Zellen steigt.
  const int32_t back16 = setup_cam_back_cells() << 16;
  cam->x16 = s_f.x16 - (int32_t)(((int64_t)sin_lookup(s_f.yaw) * back16) >> 16);
  cam->y16 = s_f.y16 - (int32_t)(((int64_t)cos_lookup(s_f.yaw) * back16) >> 16);
  cam->h8 = s_f.h8 + (CAM_UP_CELLS << 8);
  // Die Kamera darf nicht im Fels stecken, sonst fuellt Gestein das Bild.
  const int32_t cam_ground = world_height_at(cam->x16, cam->y16);
  if (cam->h8 < cam_ground + (3 << 8)) cam->h8 = cam_ground + (3 << 8);
  cam->yaw = s_f.yaw;
  // Die Kamera rollt mit halbem Winkel, der Gleiter mit doppeltem: so bleibt
  // der Horizont ruhig genug und die Lage trotzdem ablesbar.
  cam->roll_deg8 = s_f.roll_deg8 / 2;
  cam->pitch_px = 0;
}
