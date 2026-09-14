#include "synth.h"
#include "config.h"

#define LUT_BITS 10
#define LUT_N (1 << LUT_BITS)
#define ENV_ONE 65536

typedef struct {
  uint32_t phase;
  uint32_t inc;
  int32_t env;      // 16.16
  int32_t target;   // 0 oder ENV_ONE
} Osc;

typedef struct {
  uint32_t phase;
  uint32_t inc;
  int32_t amp;      // 16.16
  uint8_t decay_shift;
} Partial;

static int16_t s_sin[LUT_N];
static Osc s_fork;
static Osc s_flower;
static int32_t s_fork_chz = FORK_START_CHZ;
static uint8_t s_flower_w;
static bool s_octave;
static bool s_force_gate;
static bool s_flat;
static Partial s_ping[3];
static uint32_t s_lfsr = 0xACE1u;
static int32_t s_crack_amp;   // 16.16
static int32_t s_crack_lp;    // Tiefpass-Zustand (Sample)
static int32_t s_crack_cut;   // 16.16 Koeffizient

static uint32_t prv_inc(int32_t chz) {
  if (chz < 100) {
    chz = 100;
  }
  return (uint32_t)(((uint64_t)(uint32_t)chz << 32) / (100ULL * AUDIO_RATE_HZ));
}

static inline int32_t prv_sin(uint32_t phase) {
  return s_sin[phase >> (32 - LUT_BITS)];
}

static inline void prv_env_step(Osc *o) {
  int32_t d = o->target - o->env;
  if (d > 0) {
    o->env += (d >> 6) + 1;        // Attack ~ 4 ms, erreicht das Ziel garantiert
    if (o->env > o->target) o->env = o->target;
  } else if (d < 0) {
    o->env -= ((-d) >> 9) + 1;     // Release ~ 32 ms, erreicht 0 garantiert
    if (o->env < o->target) o->env = o->target;
  }
}

void synth_init(void) {
  for (int i = 0; i < LUT_N; i++) {
    int32_t v = sin_lookup((int32_t)(((int64_t)i * TRIG_MAX_ANGLE) / LUT_N));
    s_sin[i] = (int16_t)(v / 2);
  }
  s_fork = (Osc){ .phase = 0, .inc = prv_inc(FORK_START_CHZ), .env = 0, .target = 0 };
  s_flower = (Osc){ .phase = 0, .inc = prv_inc(44000), .env = 0, .target = 0 };
  s_flower_w = 0;
  s_octave = false;
  for (int i = 0; i < 3; i++) {
    s_ping[i] = (Partial){ .phase = 0, .inc = 0, .amp = 0, .decay_shift = 12 };
  }
  s_crack_amp = 0;
}

void synth_set_fork(int32_t chz, bool gate) {
  s_fork_chz = chz;
  if (!s_flat) {   // im Flachtest bleibt die Frequenz fest, sonst zieht das Spiel sie mit
    s_fork.inc = prv_inc(s_octave ? chz * 2 : chz);
  }
  s_fork.target = (gate || s_force_gate) ? ENV_ONE : 0;
}

void synth_set_flower(int32_t chz, uint8_t weight_pct) {
  s_flower.inc = prv_inc(chz);
  s_flower_w = weight_pct > 100 ? 100 : weight_pct;
}

void synth_set_force_gate(bool on) {
  s_force_gate = on;
  if (on) {
    s_fork.target = ENV_ONE;
  }
}

void synth_set_octave_jump(bool on) {
  s_octave = on;
  if (!s_flat) {
    s_fork.inc = prv_inc(on ? s_fork_chz * 2 : s_fork_chz);
  }
}

void synth_ping(int32_t chz) {
  s_ping[0] = (Partial){ .phase = 0, .inc = prv_inc(chz), .amp = ENV_ONE, .decay_shift = 12 };
  s_ping[1] = (Partial){ .phase = 0, .inc = prv_inc(chz * 232 / 100), .amp = ENV_ONE * 3 / 4, .decay_shift = 11 };
  s_ping[2] = (Partial){ .phase = 0, .inc = prv_inc(chz * 425 / 100), .amp = ENV_ONE / 2, .decay_shift = 10 };
}

void synth_crack(void) {
  s_crack_amp = ENV_ONE;
  s_crack_cut = 39322;   // 0,6
  s_crack_lp = 0;
}

void synth_set_flat(bool on) {
  s_flat = on;
  if (on) {
    s_fork.inc = prv_inc(TON_FLAT_CHZ);
  } else {
    s_fork.inc = prv_inc(s_octave ? s_fork_chz * 2 : s_fork_chz);
  }
}

// 500 Hz bei 16 kHz: 1024 LUT-Schritte * 500 / 16000 = 32 je Sample, also acht
// volle Perioden je 256-Sample-Block. Der Block laesst sich nahtlos wiederholen,
// deshalb ist jedes Knacken im Ergebnis garantiert nicht von uns.
void synth_render_loop_block(int16_t *out) {
  for (uint32_t i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
    out[i] = (int16_t)((s_sin[(i * 32) & (LUT_N - 1)] * 18) >> 6);
  }
}

void synth_render(int16_t *out, uint32_t num_samples) {
  if (s_flat) {
    // Nackter Sinus mit fester Amplitude: keine Huellkurve, keine Blume, kein Glas.
    // Trennt Transport (Stream, Firmware, Treiber) von unserer Klangerzeugung.
    for (uint32_t i = 0; i < num_samples; i++) {
      int32_t v = prv_sin(s_fork.phase);
      s_fork.phase += s_fork.inc;
      out[i] = (int16_t)((v * 18) >> 6);
    }
    return;
  }
  const int32_t flower_target = s_fork.target ? (ENV_ONE / 100) * s_flower_w : 0;
  s_flower.target = flower_target;
  for (uint32_t i = 0; i < num_samples; i++) {
    prv_env_step(&s_fork);
    prv_env_step(&s_flower);

    int32_t fs = prv_sin(s_fork.phase);
    s_fork.phase += s_fork.inc;
    int32_t fl = prv_sin(s_flower.phase);
    s_flower.phase += s_flower.inc;

    // Gabel und Blume je ~0,28 Vollaussteuerung; Summe plus Glasklang bleibt unter 32767
    int32_t acc = (((fs * (s_fork.env >> 4)) >> 12) * 18) >> 6;
    acc += (((fl * (s_flower.env >> 4)) >> 12) * 18) >> 6;

    // Glasklang
    for (int p = 0; p < 3; p++) {
      Partial *pp = &s_ping[p];
      if (pp->amp > 0) {
        int32_t v = prv_sin(pp->phase);
        pp->phase += pp->inc;
        acc += (((v * (pp->amp >> 4)) >> 12) * 7) >> 6;
        pp->amp -= (pp->amp >> pp->decay_shift) + 1;
        if (pp->amp < 64) {
          pp->amp = 0;
        }
      }
    }

    // Splitterrauschen: LFSR durch fallenden Einpol-Tiefpass
    if (s_crack_amp > 0) {
      uint32_t bit = s_lfsr & 1u;
      s_lfsr >>= 1;
      if (bit) {
        s_lfsr ^= 0xB400u;
      }
      int32_t noise = bit ? 32767 : -32767;
      s_crack_lp += ((noise - s_crack_lp) * (s_crack_cut >> 8)) >> 8;
      acc += (((s_crack_lp * (s_crack_amp >> 4)) >> 12) * 6) >> 6;
      s_crack_amp -= (s_crack_amp >> 12) + 1;
      s_crack_cut -= (s_crack_cut >> 13) + 1;
      if (s_crack_cut < 1300) {
        s_crack_cut = 1300;
      }
      if (s_crack_amp < 64) {
        s_crack_amp = 0;
      }
    }

    if (acc > 32767) {
      acc = 32767;
    } else if (acc < -32768) {
      acc = -32768;
    }
    out[i] = (int16_t)acc;
  }
}
