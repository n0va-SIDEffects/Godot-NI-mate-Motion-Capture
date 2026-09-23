#include "setup.h"

#define PERSIST_KEY_SETUP 101
#define SETUP_FORMAT 2          // siehe duell.c: Format mitschreiben, nie raten

static Setup s_setup;

void setup_init(void) {
  s_setup = (Setup){ .profil = ProfFingerUnten, .schatten = SchattenNormal,
                     .invert = 1, .rand_rechts = 0, .licht = 0, .horizont_alt = 0 };
  if (persist_exists(PERSIST_KEY_SETUP)) {
    struct { uint16_t version; uint16_t groesse; Setup setup; } block;
    const int n = persist_read_data(PERSIST_KEY_SETUP, &block, sizeof(block));
    if (n == (int)sizeof(block) && block.version == SETUP_FORMAT &&
        block.groesse == sizeof(Setup)) {
      s_setup = block.setup;
      if (s_setup.profil >= ProfAnzahl) s_setup.profil = ProfFingerUnten;
      if (s_setup.schatten >= SchattenAnzahl) s_setup.schatten = SchattenNormal;
    } else {
      persist_delete(PERSIST_KEY_SETUP);
    }
  }
}

void setup_save(void) {
  struct { uint16_t version; uint16_t groesse; Setup setup; } block = {
    .version = SETUP_FORMAT, .groesse = (uint16_t)sizeof(Setup), .setup = s_setup
  };
  persist_write_data(PERSIST_KEY_SETUP, &block, sizeof(block));
}

const Setup *setup_get(void) { return &s_setup; }

// light_enable(true) haelt die Hintergrundbeleuchtung dauerhaft an, unabhaengig
// vom Zeitgeber des Systems. Das kostet spuerbar Akku und ist deshalb nichts
// fuer den Dauerbetrieb, aber beim Messen am Schreibtisch und beim Filmen fuers
// Store-Video ist es unverzichtbar: sonst geht das Bild mitten im Lauf aus.
void setup_licht_anwenden(void) {
  light_enable(s_setup.licht != 0);
}

const char *setup_profil_name(uint8_t p) {
  switch (p) {
    case ProfFingerUnten: return "FingU";
    case ProfFingerRand:  return "FingR";
    case ProfTilt:        return "Tilt";
    case ProfFingerFlappy: return "FingF";
    default:              return "Tast";
  }
}

bool setup_profil_ist_touch(uint8_t p) {
  return p == ProfFingerUnten || p == ProfFingerRand || p == ProfFingerFlappy;
}

bool setup_profil_ist_flappy(uint8_t p) {
  return p == ProfTasten || p == ProfFingerFlappy;
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
    case 4:
      s_setup.licht = s_setup.licht ? 0 : 1;
      setup_licht_anwenden();
      break;
    case 5: s_setup.horizont_alt = s_setup.horizont_alt ? 0 : 1; break;
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
    case 3:
      snprintf(out, n, "%sRandseite %s", m, s_setup.rand_rechts ? "rechts" : "links");
      break;
    case 4:
      snprintf(out, n, "%sLicht %s", m, s_setup.licht ? "dauernd an" : "automatisch");
      break;
    default:
      snprintf(out, n, "%sHorizont %s", m,
               s_setup.horizont_alt ? "gegen Kurve" : "mit Kurve");
      break;
  }
}
