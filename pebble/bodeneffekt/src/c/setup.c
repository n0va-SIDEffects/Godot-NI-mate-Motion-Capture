#include "setup.h"

#define PERSIST_KEY_SETUP 101

static Setup s_setup;

void setup_init(void) {
  s_setup = (Setup){ .profil = ProfFingerUnten, .schatten = SchattenNormal,
                     .invert = 1, .rand_rechts = 0 };
  if (persist_exists(PERSIST_KEY_SETUP)) {
    persist_read_data(PERSIST_KEY_SETUP, &s_setup, sizeof(s_setup));
    if (s_setup.profil >= ProfAnzahl) s_setup.profil = ProfFingerUnten;
    if (s_setup.schatten >= SchattenAnzahl) s_setup.schatten = SchattenNormal;
  }
}

void setup_save(void) {
  persist_write_data(PERSIST_KEY_SETUP, &s_setup, sizeof(s_setup));
}

const Setup *setup_get(void) { return &s_setup; }

const char *setup_profil_name(uint8_t p) {
  switch (p) {
    case ProfFingerUnten: return "FingU";
    case ProfFingerRand:  return "FingR";
    case ProfTilt:        return "Tilt ";
    default:              return "Tast ";
  }
}

bool setup_profil_ist_touch(uint8_t p) {
  return p == ProfFingerUnten || p == ProfFingerRand;
}

int setup_cam_back_cells(void) {
  return s_setup.schatten == SchattenHoch ? CAM_BACK_HOCH : CAM_BACK_CELLS;
}

int setup_glider_row(void) {
  return HORIZON_BASE + (CAM_UP_CELLS * SCALE_H) / setup_cam_back_cells();
}

void setup_naechster_wert(int zeile) {
  switch (zeile) {
    case 0: s_setup.profil = (uint8_t)((s_setup.profil + 1) % ProfAnzahl); break;
    case 1: s_setup.schatten = (uint8_t)((s_setup.schatten + 1) % SchattenAnzahl); break;
    case 2: s_setup.invert = s_setup.invert ? 0 : 1; break;
    case 3: s_setup.rand_rechts = s_setup.rand_rechts ? 0 : 1; break;
    default: return;
  }
  setup_save();
}

void setup_text(int zeile, char *out, size_t n, bool markiert) {
  const char *m = markiert ? ">" : " ";
  switch (zeile) {
    case 0:
      snprintf(out, n, "%sSteuerung %s", m, setup_profil_name(s_setup.profil));
      break;
    case 1:
      snprintf(out, n, "%sSchatten %s", m,
               s_setup.schatten == SchattenHoch ? "hoch" : "normal");
      break;
    case 2:
      snprintf(out, n, "%sNicklage %s", m, s_setup.invert ? "umgekehrt" : "direkt");
      break;
    default:
      snprintf(out, n, "%sRandseite %s", m, s_setup.rand_rechts ? "rechts" : "links");
      break;
  }
}
