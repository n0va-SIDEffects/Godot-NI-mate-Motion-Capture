/*
 * The sound bank: imported recordings, synthesized sounds and note
 * sequences.
 *
 * Realistic noises (applause, animals, explosions ...) are real recordings
 * imported as PCM resources, see samples.inc. The synthesized sounds here
 * are the clean, tonal ones that work well on the tiny watch speaker.
 */
#include "sounds.h"

#define MS SYNTH_NOW_MS

/* ======================================================================
 * PCM generators
 * ====================================================================== */

// --- Sad trombone: four descending "wah"s ------------------------------
typedef struct { uint16_t start, dur, f0, f1; } Seg;

static const Seg TROMBONE[] = {
  {    0, 480, 294, 277 },
  {  540, 480, 277, 262 },
  { 1080, 480, 262, 247 },
  { 1620, 880, 247, 233 },
};

static int32_t gen_trombone(Synth *s) {
  uint32_t ms = MS(s);
  for (int i = 0; i < 4; i++) {
    const Seg *g = &TROMBONE[i];
    if (ms < g->start || ms >= (uint32_t)g->start + g->dur) continue;
    uint32_t pos = ms - g->start;
    int32_t f = ramp(g->f0, g->f1, pos, 220);
    if (i == 3 && pos > 250) {
      s->ph[1] += hz_inc(5);
      f += (osc_sin(s->ph[1]) * 5) >> 15;
    }
    s->ph[0] += hz_inc(f);
    int32_t wah = ramp(900, 9000, pos, 260);
    int32_t x = lp1(&s->lp[0], osc_saw(s->ph[0]), wah);
    uint32_t rel = (i == 3) ? 500 : 70;
    int32_t env = ar_env(s->t - SYNTH_MS(g->start), SYNTH_MS(g->dur), SYNTH_MS(40), SYNTH_MS(rel));
    return q15mul(x, env);
  }
  return 0;
}

// --- Cricket: three chirp groups ----------------------------------------
static int32_t gen_cricket(Synth *s) {
  uint32_t ms = MS(s);
  uint32_t g = ms / 800;
  uint32_t gp = ms % 800;
  bool on = (g < 3 && gp < 3 * 90) && ((gp % 90) < 50);
  int32_t gate = lp1(&s->lp[0], on ? Q15_MAX : 0, 1500);
  s->ph[0] += hz_inc(4300);
  s->ph[1] += hz_inc(58);
  int32_t am = 20000 + ((osc_sin(s->ph[1]) * 3) >> 3);
  int32_t x = q15mul(osc_sin(s->ph[0]), am);
  int32_t v = q15mul(x, gate);
  return v - (v >> 2);
}

// --- Wolf whistle: up, pause, up-and-down -------------------------------
static int32_t gen_whistle(Synth *s) {
  uint32_t ms = MS(s);
  int32_t f;
  uint32_t lt, ld;
  if (ms < 380) {
    f = ramp(900, 2000, ms, 380);
    lt = s->t;
    ld = SYNTH_MS(380);
  } else if (ms < 520) {
    return q15mul(noise(s), 400);
  } else {
    uint32_t p = ms - 520;
    f = (p < 260) ? ramp(1800, 2450, p, 260) : ramp(2450, 800, p - 260, 520);
    lt = s->t - SYNTH_MS(520);
    ld = SYNTH_MS(780);
  }
  s->ph[0] += hz_inc(f);
  int32_t x = osc_sin(s->ph[0]) + q15mul(noise(s), 900);
  int32_t env = ar_env(lt, ld, SYNTH_MS(25), SYNTH_MS(40));
  int32_t out = q15mul(x, env);
  return out - (out >> 3);
}

// --- Laser: three falling "pew"s ----------------------------------------
static int32_t gen_laser(Synth *s) {
  uint32_t ms = MS(s);
  const uint32_t per = 260, dur = 190;
  uint32_t i = ms / per;
  uint32_t pos = ms % per;
  if (i >= 3 || pos >= dur) return 0;
  int32_t x = ramp(Q15_MAX, 0, pos, dur);
  int32_t x2 = q15mul(x, x);
  int32_t f = 220 + ((1700 * x2) >> 15);
  s->ph[0] += hz_inc(f);
  int32_t v = ((osc_sin(s->ph[0]) * 3) >> 2) + (osc_sq(s->ph[0]) >> 2);
  int32_t env = ar_env(s->t - SYNTH_MS(i * per), SYNTH_MS(dur), SYNTH_MS(3), SYNTH_MS(40));
  return q15mul(v, env);
}

/* ======================================================================
 * Note based sounds
 * ====================================================================== */

#define N(note, wave, ms, vel) { .midi_note = (note), .waveform = (wave), .duration_ms = (ms), .velocity = (vel) }
#define REST(ms)               { .midi_note = 0, .waveform = SpeakerWaveformSine, .duration_ms = (ms), .velocity = 0 }

#define SQ  SpeakerWaveformSquare
#define TRI SpeakerWaveformTriangle
#define SAW SpeakerWaveformSawtooth
#define SIN SpeakerWaveformSine

// Fanfare "Ta-daaa!" -- clean lead with a soft echo voice
static const SpeakerNote FANFARE_LEAD[] = {
  N(67, TRI, 120, 110), N(72, TRI, 120, 110), N(76, TRI, 120, 115),
  N(79, TRI, 360, 125), N(76, TRI, 120, 110), N(79, TRI, 750, 127),
};
static const SpeakerNote FANFARE_ECHO[] = {
  REST(70), N(67, SIN, 120, 45), N(72, SIN, 120, 45), N(76, SIN, 120, 45),
  N(79, SIN, 360, 50), N(76, SIN, 120, 45), N(79, SIN, 680, 50),
};
static const SpeakerTrack FANFARE[] = {
  { .notes = FANFARE_LEAD, .num_notes = ARRAY_LENGTH(FANFARE_LEAD), .sample = NULL },
  { .notes = FANFARE_ECHO, .num_notes = ARRAY_LENGTH(FANFARE_ECHO), .sample = NULL },
};

// Level-up arpeggio
static const SpeakerNote LEVELUP[] = {
  N(72, SQ, 60, 100), N(76, SQ, 60, 100), N(79, SQ, 60, 100), N(84, SQ, 60, 110),
  N(88, SQ, 60, 110), N(91, SQ, 60, 120), N(96, SQ, 90, 127), REST(40), N(96, SQ, 260, 127),
};

// Game over -- two voices sinking
static const SpeakerNote GAMEOVER_LEAD[] = {
  N(64, TRI, 380, 110), REST(20), N(60, TRI, 380, 110), REST(20),
  N(57, TRI, 380, 110), REST(20), N(53, TRI, 900, 120),
};
static const SpeakerNote GAMEOVER_BASS[] = {
  N(52, SAW, 380, 70), REST(20), N(48, SAW, 380, 70), REST(20),
  N(45, SAW, 380, 70), REST(20), N(41, SAW, 900, 80),
};
static const SpeakerTrack GAMEOVER[] = {
  { .notes = GAMEOVER_LEAD, .num_notes = ARRAY_LENGTH(GAMEOVER_LEAD), .sample = NULL },
  { .notes = GAMEOVER_BASS, .num_notes = ARRAY_LENGTH(GAMEOVER_BASS), .sample = NULL },
};

// Doorbell "ding dong" with an octave shimmer
static const SpeakerNote BELL_LOW[]  = { N(76, SIN, 450, 127), N(72, SIN, 800, 127) };
static const SpeakerNote BELL_HIGH[] = { N(88, SIN, 450, 45),  N(84, SIN, 800, 45) };
static const SpeakerTrack DOORBELL[] = {
  { .notes = BELL_LOW,  .num_notes = ARRAY_LENGTH(BELL_LOW),  .sample = NULL },
  { .notes = BELL_HIGH, .num_notes = ARRAY_LENGTH(BELL_HIGH), .sample = NULL },
};

/* ======================================================================
 * The bank
 * ====================================================================== */

#define SYNTH(nm, hnt, col, ms, fn) \
  { .name = nm, .hint = hnt, .argb = col, .kind = SoundKindSynth, .duration_ms = ms, .render = fn }
#define NOTES(nm, hnt, col, arr) \
  { .name = nm, .hint = hnt, .argb = col, .kind = SoundKindNotes, .notes = arr, .count = ARRAY_LENGTH(arr) }
#define TRACKS(nm, hnt, col, arr) \
  { .name = nm, .hint = hnt, .argb = col, .kind = SoundKindTracks, .tracks = arr, .count = ARRAY_LENGTH(arr) }
#define SAMPLE(nm, hnt, col, res, fmt, cdc) \
  { .name = nm, .hint = hnt, .argb = col, .kind = SoundKindSample, .resource_id = res, .pcm_format = fmt, .codec = cdc },

const Sound SOUNDS[] = {
  // Real recordings first (see tools/import_sample.py and resources/samples/ATTRIBUTION.md).
#include "samples.inc"
  // Synthesized and note based sounds.
  SYNTH ("Sad Trombone", "Wah wah waaah",    GColorCobaltBlueARGB8,          2500, gen_trombone),
  NOTES ("Level-Up",     "Power-Up!",        GColorBrightGreenARGB8,         LEVELUP),
  TRACKS("Game Over",    "Nochmal?",         GColorImperialPurpleARGB8,      GAMEOVER),
  TRACKS("Tusch",        "Ta-daaa!",         GColorChromeYellowARGB8,        FANFARE),
  SYNTH ("Grille",       "Peinliche Stille", GColorDarkGreenARGB8,           2200, gen_cricket),
  SYNTH ("Pfiff",        "Wolf-Pfiff",       GColorPictonBlueARGB8,          1300, gen_whistle),
  SYNTH ("Laser",        "Pew pew pew",      GColorElectricUltramarineARGB8,  780, gen_laser),
  TRACKS("Klingel",      "Ding dong",        GColorTiffanyBlueARGB8,         DOORBELL),
};

const uint8_t NUM_SOUNDS = ARRAY_LENGTH(SOUNDS);
