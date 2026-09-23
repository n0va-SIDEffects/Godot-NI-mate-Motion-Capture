#include "duell.h"
#include "world.h"
#include "setup.h"
#include <stddef.h>

// Ein Lauf je Profil bleibt im Flash stehen, damit sich Laeufe auch ueber eine
// Pause hinweg vergleichen lassen.
//
// Mit Version und Groesse davor, und das ist kein Zierrat: als aus zwei
// Profilen vier wurden und der Lauf ein Feld dazubekam, hat der alte Datensatz
// ohne diese Pruefung stillschweigend als neuer gelesen und alle gespeicherten
// Laeufe entwertet. Passt eine der beiden Angaben nicht, wird verworfen statt
// falsch ausgelegt.
#define PERSIST_KEY_LAUF 100
#define DUELL_FORMAT 3

typedef struct {
  uint16_t version;
  uint16_t groesse;      // sizeof(DuellReihe) zur Kontrolle
  uint8_t slots;
} DuellKopf;

static DuellReihe s_reihen[ProfAnzahl];
static DuellReihe *s_aktiv;
static uint32_t s_lauf_sohle_ms, s_lauf_dauer_ms;
static uint32_t s_rest_ms;
static uint32_t s_vorlauf_ms;

void duell_init(void) {
  memset(s_reihen, 0, sizeof(s_reihen));
  s_aktiv = NULL;
  s_rest_ms = 0;
  if (persist_exists(PERSIST_KEY_LAUF)) {
    struct { DuellKopf kopf; DuellReihe reihen[ProfAnzahl]; } block;
    const int n = persist_read_data(PERSIST_KEY_LAUF, &block, sizeof(block));
    // Weniger Slots als heute ist in Ordnung: ein spaeter angehaengtes Profil
    // darf die bereits geflogenen Laeufe nicht wegwerfen. Nur Version und
    // Satzgroesse muessen stimmen, sonst laege der Inhalt falsch ausgelegt vor.
    //
    // offsetof statt sizeof(DuellKopf): der Kopf ist 6 Byte gross, liegt im
    // Block aber auf 8 gepolstert, weil DuellReihe auf 4 ausgerichtet ist. Mit
    // sizeof faellt die Rechnung um genau diese zwei Byte daneben, und jeder
    // gespeicherte Lauf waere beim naechsten neuen Profil weg gewesen.
    const int erwartet = (int)offsetof(__typeof__(block), reihen) +
                         (int)block.kopf.slots * (int)sizeof(DuellReihe);
    if (n >= (int)sizeof(DuellKopf) && block.kopf.version == DUELL_FORMAT &&
        block.kopf.groesse == sizeof(DuellReihe) &&
        block.kopf.slots <= ProfAnzahl && n == erwartet) {
      memcpy(s_reihen, block.reihen, (size_t)block.kopf.slots * sizeof(DuellReihe));
    } else {
      APP_LOG(APP_LOG_LEVEL_INFO,
              "[BE] gespeicherte Duell-Laeufe verworfen: fremdes Format");
      persist_delete(PERSIST_KEY_LAUF);
    }
  }
}

void duell_start(void) {
  const uint8_t p = control_profile();
  s_aktiv = &s_reihen[p % ProfAnzahl];
  // Eine Reihe gilt fuer eine Strecke und eine Schattenlage. Aendert sich
  // eines davon, faengt sie von vorn an, sonst mischt der Mittelwert
  // Unvergleichbares.
  if (s_aktiv->laeufe == 0 || s_aktiv->seed != world_info()->seed ||
      s_aktiv->schatten != setup_get()->schatten) {
    memset(s_aktiv, 0, sizeof(*s_aktiv));
  }
  s_aktiv->seed = world_info()->seed;
  s_aktiv->profil = p;
  s_aktiv->invert = control_pitch_invert() ? 1 : 0;
  s_aktiv->schatten = setup_get()->schatten;
  s_lauf_sohle_ms = 0;
  s_lauf_dauer_ms = 0;
  s_rest_ms = DUELL_DAUER_MS;
  s_vorlauf_ms = DUELL_VORLAUF_MS;
}

void duell_reihe_loeschen(uint8_t profil) {
  memset(&s_reihen[profil % ProfAnzahl], 0, sizeof(DuellReihe));
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
  s_lauf_dauer_ms += dt_ms;
  if (in_effekt) {
    s_aktiv->sohle_ms += dt_ms;
    s_lauf_sohle_ms += dt_ms;
  }
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
    // Spanne ueber die Einzellaeufe mitschreiben, damit die Streuung sichtbar
    // bleibt und nicht im Mittelwert verschwindet.
    const uint32_t sohle_pct = s_lauf_dauer_ms
                                 ? (s_lauf_sohle_ms * 100) / s_lauf_dauer_ms : 0;
    if (s_aktiv->laeufe == 0 || sohle_pct < s_aktiv->sohle_min) {
      s_aktiv->sohle_min = (uint8_t)sohle_pct;
    }
    if (sohle_pct > s_aktiv->sohle_max) s_aktiv->sohle_max = (uint8_t)sohle_pct;
    s_aktiv->laeufe++;
    s_aktiv = NULL;
    struct { DuellKopf kopf; DuellReihe reihen[ProfAnzahl]; } block;
    block.kopf = (DuellKopf){ .version = DUELL_FORMAT,
                              .groesse = (uint16_t)sizeof(DuellReihe),
                              .slots = ProfAnzahl };
    memcpy(block.reihen, s_reihen, sizeof(s_reihen));
    persist_write_data(PERSIST_KEY_LAUF, &block, sizeof(block));
    return DuellZuende;
  }
  s_rest_ms -= dt_ms;
  return DuellWertung;
}

const DuellReihe *duell_ergebnis(uint8_t profil) {
  return &s_reihen[profil % ProfAnzahl];
}
