#include "duell.h"
#include "world.h"
#include "setup.h"

// Ein Lauf je Profil bleibt im Flash stehen, damit sich Laeufe auch ueber eine
// Pause hinweg vergleichen lassen.
//
// Mit Version und Groesse davor, und das ist kein Zierrat: als aus zwei
// Profilen vier wurden und der Lauf ein Feld dazubekam, hat der alte Datensatz
// ohne diese Pruefung stillschweigend als neuer gelesen und alle gespeicherten
// Laeufe entwertet. Passt eine der beiden Angaben nicht, wird verworfen statt
// falsch ausgelegt.
#define PERSIST_KEY_LAUF 100
#define DUELL_FORMAT 2

typedef struct {
  uint16_t version;
  uint16_t groesse;      // sizeof(DuellLauf) zur Kontrolle
  uint8_t slots;
} DuellKopf;

static DuellLauf s_laeufe[ProfAnzahl];
static DuellLauf *s_aktiv;
static uint32_t s_rest_ms;
static uint32_t s_vorlauf_ms;
static uint16_t s_nummer[ProfAnzahl];

void duell_init(void) {
  memset(s_laeufe, 0, sizeof(s_laeufe));
  s_aktiv = NULL;
  s_rest_ms = 0;
  if (persist_exists(PERSIST_KEY_LAUF)) {
    struct { DuellKopf kopf; DuellLauf laeufe[ProfAnzahl]; } block;
    const int n = persist_read_data(PERSIST_KEY_LAUF, &block, sizeof(block));
    if (n == (int)sizeof(block) && block.kopf.version == DUELL_FORMAT &&
        block.kopf.groesse == sizeof(DuellLauf) && block.kopf.slots == ProfAnzahl) {
      memcpy(s_laeufe, block.laeufe, sizeof(s_laeufe));
    } else {
      APP_LOG(APP_LOG_LEVEL_INFO,
              "[BE] gespeicherte Duell-Laeufe verworfen: fremdes Format");
      persist_delete(PERSIST_KEY_LAUF);
    }
  }
}

void duell_start(void) {
  const uint8_t p = control_profile();
  s_aktiv = &s_laeufe[p % ProfAnzahl];
  memset(s_aktiv, 0, sizeof(*s_aktiv));
  s_aktiv->seed = world_info()->seed;
  s_aktiv->profil = p;
  s_aktiv->invert = control_pitch_invert() ? 1 : 0;
  s_aktiv->schatten = setup_get()->schatten;
  s_aktiv->nummer = ++s_nummer[p % ProfAnzahl];
  s_rest_ms = DUELL_DAUER_MS;
  s_vorlauf_ms = DUELL_VORLAUF_MS;
}

void duell_abort(void) {
  s_aktiv = NULL;
  s_rest_ms = 0;
  s_vorlauf_ms = 0;
}

bool duell_aktiv(void) { return s_aktiv != NULL; }
uint32_t duell_rest_ms(void) { return s_rest_ms; }
uint32_t duell_vorlauf_ms(void) { return s_vorlauf_ms; }

DuellPhase duell_tick(uint32_t dt_ms, int32_t agl8, bool kontakt, bool in_effekt,
                      int schatten_zeile, int schatten_hw, const CtrlStats *touch) {
  if (!s_aktiv) return DuellAus;
  if (dt_ms > 200) dt_ms = 200;          // nach einer Lastspitze nicht springen

  if (s_vorlauf_ms) {
    // Countdown: es wird weder geflogen noch gewertet. Am Ende meldet die
    // Phase DuellStart, und erst dann setzt der Neigungssensor seinen
    // Nullpunkt - auf die Haltung, in der wirklich geflogen wird.
    if (s_vorlauf_ms <= dt_ms) {
      s_vorlauf_ms = 0;
      return DuellStart;
    }
    s_vorlauf_ms -= dt_ms;
    return DuellVorlauf;
  }

  s_aktiv->dauer_ms += dt_ms;
  if (in_effekt) s_aktiv->sohle_ms += dt_ms;
  if (kontakt) s_aktiv->kontakte++;
  if (agl8 > 0) {
    s_aktiv->agl_sum8 += (uint32_t)agl8;
    s_aktiv->proben++;
  }

  // Verdeckung. Senkrecht bleibt die Annahme grosszuegig zugunsten der Kritik:
  // Finger und Hand kommen von unten, also gilt alles ab FINGER_COVER_PX
  // oberhalb der Beruehrung als verdeckt. Waagerecht wird seit dem Randprofil
  // mitgeprueft, denn genau darauf beruht dessen Idee: liegt der Finger am
  // Rand und der Schatten in der Mitte, ist nichts verdeckt.
  if (schatten_zeile >= 0) {
    s_aktiv->schatten_ms += dt_ms;
    if (touch->down && schatten_zeile >= (int)touch->abs_y - FINGER_COVER_PX) {
      const int dx = (int)touch->abs_x - SCR_CX;
      const int abstand = dx < 0 ? -dx : dx;
      if (abstand <= FINGER_COVER_PX + schatten_hw) {
        s_aktiv->blind_ms += dt_ms;
      }
    }
  }

  if (s_rest_ms <= dt_ms) {
    s_rest_ms = 0;
    s_aktiv->gueltig = true;
    s_aktiv = NULL;
    struct { DuellKopf kopf; DuellLauf laeufe[ProfAnzahl]; } block;
    block.kopf = (DuellKopf){ .version = DUELL_FORMAT,
                              .groesse = (uint16_t)sizeof(DuellLauf),
                              .slots = ProfAnzahl };
    memcpy(block.laeufe, s_laeufe, sizeof(s_laeufe));
    persist_write_data(PERSIST_KEY_LAUF, &block, sizeof(block));
    return DuellZuende;
  }
  s_rest_ms -= dt_ms;
  return DuellWertung;
}

const DuellLauf *duell_ergebnis(uint8_t profil) {
  return &s_laeufe[profil % ProfAnzahl];
}
