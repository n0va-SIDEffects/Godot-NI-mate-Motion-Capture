/*
 * The sound bank. Every sound here is either synthesized on the fly
 * (no audio assets needed) or expressed as a compact note sequence.
 *
 * Design note: the watch speaker is tiny, so pure sine tones below
 * ~300 Hz are barely audible. The low sounds (fart, burp, toms) rely on
 * saw/square harmonics and noise to carry their character.
 */
#include "sounds.h"

#define MS SYNTH_NOW_MS

/* ======================================================================
 * PCM generators
 * ====================================================================== */

// --- Applause: random clap bursts over a crowd wash --------------------
static int32_t gen_applause(Synth *s) {
  uint32_t ms = MS(s);
  int32_t dens;
  if (ms < 400) {
    dens = ramp(0, Q15_MAX, ms, 400);
  } else if (ms < 2000) {
    dens = Q15_MAX;
  } else {
    int32_t r = ramp(Q15_MAX, 0, ms - 2000, 1200);
    dens = q15mul(r, r);
  }
  // ~46 claps per second at full density
  int32_t thr = (dens * 190) >> 15;
  if (rnd15(s) < thr) {
    s->env[0] += 9000 + (rnd15(s) & 4095);
    if (s->env[0] > Q15_MAX) s->env[0] = Q15_MAX;
  }
  env_decay(&s->env[0], decay_coef(12));
  int32_t n = noise(s);
  int32_t hp = n - lp1(&s->lp[0], n, 6000);
  int32_t clap = q15mul(hp, s->env[0]);
  int32_t wash = q15mul(lp1(&s->lp[1], n, 2500), dens) >> 1;
  return clap + (clap >> 1) + wash;
}

// --- Fart: wobbly low buzz with flutter and sputter --------------------
static int32_t gen_fart(Synth *s) {
  uint32_t ms = MS(s);
  const uint32_t len = 1200;
  if ((s->t & 31) == 0) {
    s->var[0] += noise(s) >> 12;
    if (s->var[0] > 14) s->var[0] = 14;
    if (s->var[0] < -14) s->var[0] = -14;
  }
  int32_t f = ramp(95, 40, ms, len) + s->var[0];
  s->ph[0] += hz_inc(f);
  s->ph[1] += hz_inc(24 + (s->var[0] >> 1));
  int32_t tone = (osc_saw(s->ph[0]) * 5 + osc_sq(s->ph[0]) * 3) >> 3;
  tone = lp1(&s->lp[0], tone, 5500);
  int32_t am = 18000 + ((osc_sin(s->ph[1]) * 7) >> 4);
  if ((s->t % 160) == 0) {
    int32_t p_on = (ms < 600) ? Q15_MAX : ramp(Q15_MAX, 9000, ms - 600, len - 600);
    s->var[1] = (rnd15(s) < p_on) ? Q15_MAX : 0;
  }
  int32_t gate = lp1(&s->lp[1], s->var[1], 1200);
  int32_t env = ar_env(s->t, s->total, SYNTH_MS(25), SYNTH_MS(250));
  int32_t out = q15mul(tone, am) + (q15mul(noise(s), am) >> 3);
  out = q15mul(out, gate);
  return q15mul(out, env) << 1;
}

// --- Burp: pitch rises then sinks, gargling AM -------------------------
static int32_t gen_burp(Synth *s) {
  uint32_t ms = MS(s);
  int32_t f = (ms < 150) ? ramp(70, 118, ms, 150) : ramp(118, 55, ms - 150, 600);
  if ((s->t & 63) == 0) s->var[0] = noise(s) >> 12;
  f += s->var[0];
  s->ph[0] += hz_inc(f);
  s->ph[1] += hz_inc(31);
  int32_t tone = (osc_saw(s->ph[0]) + osc_sq(s->ph[0])) >> 1;
  tone = lp1(&s->lp[0], tone, 4200);
  int32_t am = 15000 + ((osc_sin(s->ph[1]) * 3) >> 3);
  int32_t out = q15mul(tone, am) + (q15mul(noise(s), am) >> 3);
  int32_t env = ar_env(s->t, s->total, SYNTH_MS(30), SYNTH_MS(160));
  int32_t v = q15mul(out, env);
  return v + (v >> 1) + (v >> 2);
}

// --- Fall: slide whistle down, then a thud -----------------------------
static int32_t gen_fall(Synth *s) {
  uint32_t ms = MS(s);
  const uint32_t whistle = 1100;
  if (ms < whistle) {
    int32_t x = ramp(Q15_MAX, 0, ms, whistle);
    int32_t f = 300 + ((1500 * q15mul(x, x)) >> 15);
    s->ph[1] += hz_inc(7);
    f += (osc_sin(s->ph[1]) * 14) >> 15;
    s->ph[0] += hz_inc(f);
    int32_t env = ar_env(s->t, SYNTH_MS(whistle), SYNTH_MS(30), SYNTH_MS(80));
    int32_t out = q15mul(osc_sin(s->ph[0]), env);
    return out - (out >> 2);
  }
  if (s->t == SYNTH_MS(whistle)) {
    env_trigger(&s->env[0]);
    env_trigger(&s->env[1]);
    s->ph[0] = 0;
  }
  int32_t f = 60 + ((s->env[0] * 220) >> 15);
  s->ph[0] += hz_inc(f);
  env_decay(&s->env[0], decay_coef(45));
  env_decay(&s->env[1], decay_coef(150));
  int32_t body = q15mul(osc_sin(s->ph[0]) + (osc_tri(s->ph[0]) >> 1), s->env[1]);
  int32_t nz = q15mul(lp1(&s->lp[0], noise(s), 3000), s->env[1]);
  return body - (body >> 2) + (nz >> 1);
}

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

// --- Drum roll with a crash at the end ---------------------------------
static int32_t gen_drumroll(Synth *s) {
  uint32_t ms = MS(s);
  const uint32_t roll = 1800;
  if (ms < roll) {
    if (s->t >= (uint32_t)s->var[0]) {
      env_trigger(&s->env[0]);
      env_trigger(&s->env[1]);
      s->ph[0] = 0;
      int32_t interval_ms = 52 - (14 * (int32_t)ms) / (int32_t)roll;
      s->var[0] = (int32_t)(s->t + (uint32_t)interval_ms * 16);
    }
  } else if (s->t == SYNTH_MS(roll)) {
    env_trigger(&s->env[2]);
    env_trigger(&s->env[3]);
    s->ph[1] = 0;
  }
  env_decay(&s->env[0], decay_coef(20));
  env_decay(&s->env[1], decay_coef(35));
  s->ph[0] += hz_inc(185);
  int32_t n = noise(s);
  int32_t snare = q15mul(n - lp1(&s->lp[0], n, 4000), s->env[0])
                + (q15mul(osc_sin(s->ph[0]), s->env[1]) >> 1);
  uint32_t pos = ms < roll ? ms : roll;
  int32_t gain = 11000 + (int32_t)((21000u * pos) / roll);
  int32_t out = q15mul(snare, gain);
  env_decay(&s->env[2], decay_coef(650));
  env_decay(&s->env[3], decay_coef(220));
  s->ph[1] += hz_inc(70);
  int32_t crash = q15mul(n - lp1(&s->lp[1], n, 9000), s->env[2]);
  int32_t boom = q15mul(osc_sin(s->ph[1]) + (osc_saw(s->ph[1]) >> 2), s->env[3]);
  return out + crash - (crash >> 2) + (boom >> 1);
}

// --- Ba-dum-tss: two toms and a crash ----------------------------------
static int32_t gen_badum(Synth *s) {
  const uint32_t t2 = SYNTH_MS(190), t3 = SYNTH_MS(380);
  if (s->t == 0 || s->t == t2) {
    env_trigger(&s->env[0]);
    env_trigger(&s->env[1]);
    s->ph[0] = 0;
    s->var[0] = (s->t == 0) ? 0 : 1;
  }
  if (s->t == t3) {
    env_trigger(&s->env[2]);
    env_trigger(&s->env[3]);
    s->ph[1] = 0;
  }
  env_decay(&s->env[0], decay_coef(170));
  env_decay(&s->env[1], decay_coef(35));
  int32_t base = s->var[0] ? 95 : 110;
  int32_t f = base + ((s->env[1] * 260) >> 15);
  s->ph[0] += hz_inc(f);
  int32_t tom = q15mul(osc_sin(s->ph[0]) + (osc_tri(s->ph[0]) >> 1), s->env[0]);
  int32_t n = noise(s);
  int32_t skin = q15mul(n, s->env[1]) >> 2;
  env_decay(&s->env[2], decay_coef(600));
  env_decay(&s->env[3], decay_coef(200));
  s->ph[1] += hz_inc(65);
  int32_t crash = q15mul(n - lp1(&s->lp[0], n, 10000), s->env[2]);
  int32_t boom = q15mul(osc_sin(s->ph[1]), s->env[3]);
  return tom - (tom >> 2) + skin + crash - (crash >> 2) + (boom >> 1);
}

// --- Boing: descending pitch with a decaying spring wobble -------------
static int32_t gen_boing(Synth *s) {
  uint32_t ms = MS(s);
  const uint32_t len = 1100;
  if (s->t == 0) env_trigger(&s->env[0]);
  int32_t x = ramp(Q15_MAX, 0, ms, len);
  int32_t x2 = q15mul(x, x);
  int32_t f = 140 + ((380 * x2) >> 15);
  s->ph[1] += hz_inc(11);
  f += (q15mul(osc_sin(s->ph[1]), x) * 90) >> 15;
  s->ph[0] += hz_inc(f);
  env_decay(&s->env[0], decay_coef(380));
  int32_t tone = ((osc_tri(s->ph[0]) * 5) >> 3)
               + ((osc_sin(s->ph[0] * 2) * 3) >> 3)
               + (osc_saw(s->ph[0] * 3) >> 3);
  int32_t atk = ar_env(s->t, s->total, SYNTH_MS(4), 0);
  return q15mul(q15mul(tone, s->env[0]), atk);
}

// --- Buzzer: detuned square/saw growl ----------------------------------
static int32_t gen_buzzer(Synth *s) {
  s->ph[0] += hz_inc(100);
  s->ph[1] += hz_inc(103);
  s->ph[2] += hz_inc(47);
  int32_t x = (osc_sq(s->ph[0]) >> 1) + (osc_saw(s->ph[1]) >> 1);
  x = lp1(&s->lp[0], x, 9000);
  int32_t am = 26000 + ((osc_sin(s->ph[2]) * 6) >> 5);
  x = q15mul(x, am);
  int32_t env = ar_env(s->t, s->total, SYNTH_MS(8), SYNTH_MS(50));
  return q15mul(x, env);
}

// --- Ka-ching: mechanical click, then bell partials --------------------
static int32_t gen_kaching(Synth *s) {
  if (s->t == 0) env_trigger(&s->env[0]);
  if (s->t == SYNTH_MS(130)) {
    env_trigger(&s->env[1]);
    env_trigger(&s->env[2]);
    env_trigger(&s->env[3]);
    s->ph[0] = s->ph[1] = s->ph[2] = 0;
  }
  env_decay(&s->env[0], decay_coef(9));
  env_decay(&s->env[1], decay_coef(550));
  env_decay(&s->env[2], decay_coef(380));
  env_decay(&s->env[3], decay_coef(200));
  int32_t n = noise(s);
  int32_t click = q15mul(lp1(&s->lp[0], n, 12000), s->env[0]);
  s->ph[0] += hz_inc(2093);
  s->ph[1] += hz_inc(3136);
  s->ph[2] += hz_inc(4209);
  int32_t bell = ((q15mul(osc_sin(s->ph[0]), s->env[1]) * 5) >> 3)
               + ((q15mul(osc_sin(s->ph[1]), s->env[2]) * 3) >> 3)
               + (q15mul(osc_sin(s->ph[2]), s->env[3]) >> 2);
  return click - (click >> 2) + bell;
}

// --- Laugh: six descending "ha" syllables ------------------------------
static int32_t gen_laugh(Synth *s) {
  uint32_t ms = MS(s);
  const uint32_t per = 250, dur = 190;
  uint32_t i = ms / per;
  uint32_t pos = ms % per;
  if (i >= 6 || pos >= dur) {
    return q15mul(lp1(&s->lp[1], noise(s), 3000), 2500);
  }
  int32_t f = 175 - (int32_t)i * 9 - (int32_t)((pos * 14) / dur);
  s->ph[1] += hz_inc(6);
  f += (osc_sin(s->ph[1]) * 4) >> 15;
  s->ph[0] += hz_inc(f);
  int32_t x = lp1(&s->lp[0], osc_saw(s->ph[0]), 5000);
  x += q15mul(noise(s), 3000);
  int32_t env = ar_env(s->t - SYNTH_MS(i * per), SYNTH_MS(dur), SYNTH_MS(30), SYNTH_MS(90));
  return q15mul(x, env);
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

// --- Explosion: bright noise darkening, overdriven, with a thump -------
static int32_t gen_explosion(Synth *s) {
  uint32_t ms = MS(s);
  if (s->t == 0) {
    env_trigger(&s->env[0]);
    env_trigger(&s->env[1]);
  }
  int32_t x = ramp(Q15_MAX, 0, ms, 1300);
  int32_t x2 = q15mul(x, x);
  int32_t coef = 600 + ((26000 * x2) >> 15);
  int32_t n = noise(s);
  int32_t body = lp1(&s->lp[0], n, coef);
  env_decay(&s->env[0], decay_coef(ms < 60 ? 2000 : 420));
  env_decay(&s->env[1], decay_coef(90));
  int32_t f = 40 + ((s->env[1] * 110) >> 15);
  s->ph[0] += hz_inc(f);
  int32_t thump = q15mul(osc_sin(s->ph[0]), s->env[0]);
  int32_t out = clip16(q15mul(body, s->env[0]) * 3 + thump);
  int32_t tail = ar_env(s->t, s->total, 0, SYNTH_MS(300));
  return q15mul(out, tail);
}

// --- Siren: slow triangle sweep -----------------------------------------
static int32_t gen_siren(Synth *s) {
  s->ph[1] += mhz_inc(800);
  int32_t f = 950 + ((osc_tri(s->ph[1]) * 300) >> 15);
  s->ph[0] += hz_inc(f);
  int32_t x = ((osc_sin(s->ph[0]) * 5) >> 3) + (osc_sq(s->ph[0]) >> 3);
  int32_t env = ar_env(s->t, s->total, SYNTH_MS(40), SYNTH_MS(120));
  return q15mul(x, env);
}

// --- Meow: "ee" to "ow" pitch and formant curve -------------------------
static int32_t gen_meow(Synth *s) {
  uint32_t ms = MS(s);
  int32_t f;
  if (ms < 160)      f = ramp(480, 820, ms, 160);
  else if (ms < 480) f = ramp(820, 720, ms - 160, 320);
  else               f = ramp(720, 360, ms - 480, 420);
  s->ph[1] += hz_inc(6);
  f += (osc_sin(s->ph[1]) * 12) >> 15;
  s->ph[0] += hz_inc(f);
  int32_t coef = ramp(12000, 2200, ms, 900);
  int32_t x = lp1(&s->lp[0], osc_saw(s->ph[0]), coef);
  x += q15mul(osc_sin(s->ph[0]), 6000);
  int32_t env = ar_env(s->t, s->total, SYNTH_MS(50), SYNTH_MS(160));
  return q15mul(x, env);
}

// --- Air horn: three detuned saw blasts ---------------------------------
static const Seg HORN[] = {
  {   0,  220, 0, 0 },
  { 300,  220, 0, 0 },
  { 600, 1150, 0, 0 },
};

static int32_t gen_airhorn(Synth *s) {
  uint32_t ms = MS(s);
  for (int i = 0; i < 3; i++) {
    const Seg *g = &HORN[i];
    if (ms < g->start || ms >= (uint32_t)g->start + g->dur) continue;
    uint32_t pos = ms - g->start;
    int32_t f = ramp(165, 233, pos, 60);
    s->ph[0] += hz_inc(f);
    s->ph[1] += hz_inc(f * 2 + 3);
    s->ph[2] += hz_inc(f + 2);
    int32_t x = (osc_saw(s->ph[0]) + osc_saw(s->ph[1]) + osc_saw(s->ph[2])) / 3;
    x = lp1(&s->lp[0], x, 14000);
    int32_t env = ar_env(s->t - SYNTH_MS(g->start), SYNTH_MS(g->dur), SYNTH_MS(15), SYNTH_MS(50));
    return q15mul(x, env);
  }
  return 0;
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

// Fanfare "Ta-daaa!" -- three voices
static const SpeakerNote FANFARE_MELODY[] = {
  N(67, SQ, 110, 110), N(72, SQ, 110, 110), N(76, SQ, 110, 110),
  N(79, SQ, 330, 120), N(76, SQ, 160, 110), N(79, SQ, 800, 127),
};
static const SpeakerNote FANFARE_HARMONY[] = {
  N(64, TRI, 110, 80), N(67, TRI, 110, 80), N(72, TRI, 110, 80),
  N(76, TRI, 330, 90), N(72, TRI, 160, 80), N(76, TRI, 800, 100),
};
static const SpeakerNote FANFARE_BASS[] = {
  N(48, SAW, 660, 70), N(55, SAW, 160, 70), N(48, SAW, 800, 80),
};
static const SpeakerTrack FANFARE[] = {
  { .notes = FANFARE_MELODY,  .num_notes = ARRAY_LENGTH(FANFARE_MELODY),  .sample = NULL },
  { .notes = FANFARE_HARMONY, .num_notes = ARRAY_LENGTH(FANFARE_HARMONY), .sample = NULL },
  { .notes = FANFARE_BASS,    .num_notes = ARRAY_LENGTH(FANFARE_BASS),    .sample = NULL },
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

// Cuckoo clock
static const SpeakerNote CUCKOO[] = {
  N(76, SIN, 250, 120), N(72, SIN, 400, 120), REST(350),
  N(76, SIN, 250, 120), N(72, SIN, 400, 120),
};

// Dramatic sting "Dun dun duuun" -- D minor, D minor, E diminished
static const SpeakerNote STING_1[] = {
  N(69, SAW, 280, 110), REST(120), N(69, SAW, 280, 110), REST(120), N(70, SAW, 1400, 127),
};
static const SpeakerNote STING_2[] = {
  N(65, SQ, 280, 80), REST(120), N(65, SQ, 280, 80), REST(120), N(67, SQ, 1400, 100),
};
static const SpeakerNote STING_3[] = {
  N(62, TRI, 280, 90), REST(120), N(62, TRI, 280, 90), REST(120), N(64, TRI, 1400, 110),
};
static const SpeakerTrack STING[] = {
  { .notes = STING_1, .num_notes = ARRAY_LENGTH(STING_1), .sample = NULL },
  { .notes = STING_2, .num_notes = ARRAY_LENGTH(STING_2), .sample = NULL },
  { .notes = STING_3, .num_notes = ARRAY_LENGTH(STING_3), .sample = NULL },
};

// Doorbell "ding dong" with an octave shimmer
static const SpeakerNote BELL_LOW[]  = { N(76, SIN, 450, 127), N(72, SIN, 800, 127) };
static const SpeakerNote BELL_HIGH[] = { N(88, SIN, 450, 45),  N(84, SIN, 800, 45) };
static const SpeakerTrack DOORBELL[] = {
  { .notes = BELL_LOW,  .num_notes = ARRAY_LENGTH(BELL_LOW),  .sample = NULL },
  { .notes = BELL_HIGH, .num_notes = ARRAY_LENGTH(BELL_HIGH), .sample = NULL },
};

// Magic sparkle -- two interleaved rising runs
static const SpeakerNote MAGIC_1[] = {
  N(84, SIN, 70, 110), N(88, SIN, 70, 110), N(91, SIN, 70, 110), N(95, SIN, 70, 110),
  N(98, SIN, 70, 120), N(102, SIN, 70, 120), N(105, SIN, 420, 127),
};
static const SpeakerNote MAGIC_2[] = {
  REST(35), N(79, TRI, 70, 70), N(83, TRI, 70, 70), N(86, TRI, 70, 70), N(90, TRI, 70, 70),
  N(93, TRI, 70, 80), N(97, TRI, 70, 80), N(100, TRI, 420, 90),
};
static const SpeakerTrack MAGIC[] = {
  { .notes = MAGIC_1, .num_notes = ARRAY_LENGTH(MAGIC_1), .sample = NULL },
  { .notes = MAGIC_2, .num_notes = ARRAY_LENGTH(MAGIC_2), .sample = NULL },
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

const Sound SOUNDS[] = {
  SYNTH ("Applaus",       "Standing Ovation",      GColorOrangeARGB8,             3200, gen_applause),
  TRACKS("Tusch",         "Ta-daaa!",              GColorChromeYellowARGB8,       FANFARE),
  SYNTH ("Furz",          "Pardon.",               GColorArmyGreenARGB8,          1200, gen_fart),
  SYNTH ("Ruelpser",      "Wohl bekomm's",         GColorWindsorTanARGB8,          750, gen_burp),
  SYNTH ("Fall",          "Pfiiiu ... bumm",       GColorVividCeruleanARGB8,      1700, gen_fall),
  SYNTH ("Sad Trombone",  "Wah wah waaah",         GColorCobaltBlueARGB8,         2500, gen_trombone),
  SYNTH ("Trommelwirbel", "Und der Gewinner ist",  GColorDarkCandyAppleRedARGB8,  2600, gen_drumroll),
  SYNTH ("Ba-Dum-Tss",    "Schlechter Witz",       GColorFollyARGB8,              1500, gen_badum),
  SYNTH ("Boing",         "Sprungfeder",           GColorJaegerGreenARGB8,        1100, gen_boing),
  SYNTH ("Buzzer",        "Falsch!",               GColorRedARGB8,                 900, gen_buzzer),
  SYNTH ("Ka-Ching",      "Kasse klingelt",        GColorIslamicGreenARGB8,       1300, gen_kaching),
  NOTES ("Level-Up",      "Power-Up!",             GColorBrightGreenARGB8,        LEVELUP),
  TRACKS("Game Over",     "Nochmal?",              GColorImperialPurpleARGB8,     GAMEOVER),
  SYNTH ("Lachen",        "Ha ha ha ha",           GColorRajahARGB8,              1500, gen_laugh),
  SYNTH ("Grille",        "Peinliche Stille",      GColorDarkGreenARGB8,          2200, gen_cricket),
  SYNTH ("Explosion",     "Kabumm!",               GColorSunsetOrangeARGB8,       1800, gen_explosion),
  SYNTH ("Sirene",        "Alarm!",                GColorBlueMoonARGB8,           2400, gen_siren),
  SYNTH ("Katze",         "Miau",                  GColorPurpleARGB8,              900, gen_meow),
  SYNTH ("Troete",        "Airhorn",               GColorVividVioletARGB8,        1800, gen_airhorn),
  SYNTH ("Pfiff",         "Wolf-Pfiff",            GColorPictonBlueARGB8,         1300, gen_whistle),
  NOTES ("Kuckuck",       "Kuckuck!",              GColorMayGreenARGB8,           CUCKOO),
  SYNTH ("Laser",         "Pew pew pew",           GColorElectricUltramarineARGB8, 780, gen_laser),
  TRACKS("Dun Dun Duuun", "Dramatisch",            GColorBulgarianRoseARGB8,      STING),
  TRACKS("Klingel",       "Ding dong",             GColorTiffanyBlueARGB8,        DOORBELL),
  TRACKS("Zauber",        "Bibbidi-Bobbidi",       GColorMagentaARGB8,            MAGIC),
};

const uint8_t NUM_SOUNDS = ARRAY_LENGTH(SOUNDS);
