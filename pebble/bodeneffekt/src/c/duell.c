#include "duell.h"
#include "world.h"

// Ein Lauf je Profil bleibt im Flash stehen, damit sich zwei Laeufe auch ueber
// eine Pause hinweg vergleichen lassen.
#define PERSIST_KEY_LAUF 100

static DuellLauf s_laeufe[2];
static DuellLauf *s_aktiv;
static uint32_t s_rest_ms;

void duell_init(void) {
  memset(s_laeufe, 0, sizeof(s_laeufe));
  s_aktiv = NULL;
  s_rest_ms = 0;
  if (persist_exists(PERSIST_KEY_LAUF)) {
    persist_read_data(PERSIST_KEY_LAUF, s_laeufe, sizeof(s_laeufe));
  }
}

void duell_start(void) {
  const CtrlProfile p = control_profile();
  s_aktiv = &s_laeufe[p == CtrlFinger ? 0 : 1];
  memset(s_aktiv, 0, sizeof(*s_aktiv));
  s_aktiv->seed = world_info()->seed;
  s_aktiv->profil = (uint8_t)p;
  s_aktiv->invert = control_pitch_invert() ? 1 : 0;
  s_rest_ms = DUELL_DAUER_MS;
}

void duell_abort(void) {
  s_aktiv = NULL;
  s_rest_ms = 0;
}

bool duell_aktiv(void) { return s_aktiv != NULL; }
uint32_t duell_rest_ms(void) { return s_rest_ms; }

bool duell_tick(uint32_t dt_ms, int32_t agl8, bool kontakt, bool in_effekt,
                int schatten_zeile, const CtrlStats *touch) {
  if (!s_aktiv) return false;
  if (dt_ms > 200) dt_ms = 200;          // nach einer Lastspitze nicht springen

  s_aktiv->dauer_ms += dt_ms;
  if (in_effekt) s_aktiv->sohle_ms += dt_ms;
  if (kontakt) s_aktiv->kontakte++;
  if (agl8 > 0) {
    s_aktiv->agl_sum8 += (uint32_t)agl8;
    s_aktiv->proben++;
  }

  // Verdeckung. Die Annahme ist bewusst grosszuegig zugunsten der Kritik:
  // Finger und Hand kommen von unten, also gilt alles ab FINGER_COVER_PX
  // oberhalb der Beruehrung als verdeckt. Waagerecht wird nicht geprueft,
  // weil die Handflaeche breit aufliegt und eine Spaltenrechnung hier
  // scheingenau waere.
  if (schatten_zeile >= 0) {
    s_aktiv->schatten_ms += dt_ms;
    if (touch->down && schatten_zeile >= (int)touch->abs_y - FINGER_COVER_PX) {
      s_aktiv->blind_ms += dt_ms;
    }
  }

  if (s_rest_ms <= dt_ms) {
    s_rest_ms = 0;
    s_aktiv->gueltig = true;
    s_aktiv = NULL;
    persist_write_data(PERSIST_KEY_LAUF, s_laeufe, sizeof(s_laeufe));
    return true;
  }
  s_rest_ms -= dt_ms;
  return false;
}

const DuellLauf *duell_ergebnis(CtrlProfile p) {
  return &s_laeufe[p == CtrlFinger ? 0 : 1];
}
