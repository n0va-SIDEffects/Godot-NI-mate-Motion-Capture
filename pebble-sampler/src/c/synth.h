#pragma once
/*
 * Tiny fixed-point PCM synthesizer for the Pebble speaker.
 *
 * Everything runs in 32-bit integer math (no floats) at 16 kHz mono,
 * 16-bit signed samples. Each sound is a render function that produces
 * one sample per call from a small shared state block.
 */
#include <pebble.h>

#define SYNTH_RATE        16000
#define SYNTH_MS(ms)      ((uint32_t)(ms) * (SYNTH_RATE / 1000))
#define Q15               32768
#define Q15_MAX           32767

typedef struct Synth Synth;
typedef int32_t (*SynthRenderFn)(Synth *s);

struct Synth {
  uint32_t t;          // current sample index
  uint32_t total;      // total samples of this sound
  uint32_t ph[4];      // phase accumulators (wrap at 2^32 == one period)
  uint32_t rng;        // xorshift32 state
  int32_t  lp[4];      // one-pole filter states
  int32_t  env[4];     // envelopes (q15)
  int32_t  var[4];     // free scratch registers
  SynthRenderFn fn;
};

void     synth_start(Synth *s, SynthRenderFn fn, uint32_t duration_ms, uint32_t seed);
uint32_t synth_render(Synth *s, int16_t *out, uint32_t max_samples);
bool     synth_done(const Synth *s);

/* ---- inline DSP helpers ------------------------------------------------ */

// Milliseconds elapsed (16 samples per ms at 16 kHz).
#define SYNTH_NOW_MS(s) ((s)->t >> 4)

// Phase increment for a frequency in Hz: 2^32 / 16000 = 268435.456
static inline uint32_t hz_inc(int32_t hz) {
  if (hz < 0) hz = 0;
  return (uint32_t)hz * 268435u;
}

// Phase increment for a frequency in millihertz (for slow LFOs).
static inline uint32_t mhz_inc(uint32_t mhz) {
  return (uint32_t)(((uint64_t)mhz * 268435u) / 1000u);
}

// Oscillators: all return roughly -32767..32767 (q15).
static inline int32_t osc_sin(uint32_t ph) { return sin_lookup(ph >> 16) >> 1; }
static inline int32_t osc_saw(uint32_t ph) { return (int32_t)(ph >> 16) - 32768; }
static inline int32_t osc_sq(uint32_t ph)  { return (ph & 0x80000000u) ? -Q15_MAX : Q15_MAX; }
static inline int32_t osc_tri(uint32_t ph) {
  int32_t v = osc_saw(ph);
  if (v < 0) v = -v;
  return (v << 1) - Q15;
}

// White noise, -32768..32767.
static inline int32_t noise(Synth *s) {
  uint32_t x = s->rng;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s->rng = x;
  return (int32_t)(int16_t)(x >> 16);
}

// Uniform random 0..32767.
static inline int32_t rnd15(Synth *s) { return noise(s) & Q15_MAX; }

// q15 multiply; both operands must stay within about +-40000.
static inline int32_t q15mul(int32_t a, int32_t b) { return (a * b) >> 15; }

// Hard clip to 16-bit range.
static inline int32_t clip16(int32_t v) {
  if (v > Q15_MAX) return Q15_MAX;
  if (v < -Q15) return -Q15;
  return v;
}

// One-pole low-pass. coef (q15) ~ 2*pi*fc/rate -> coef 3000 ~ 730 Hz.
static inline int32_t lp1(int32_t *state, int32_t x, int32_t coef) {
  *state += q15mul(x - *state, coef);
  return *state;
}

// Linear ramp from a to b over len, clamped.
static inline int32_t ramp(int32_t a, int32_t b, uint32_t pos, uint32_t len) {
  if (pos >= len) return b;
  return a + (int32_t)(((int64_t)(b - a) * (int64_t)pos) / (int64_t)len);
}

// Multiplier (q15) for an exponential decay with the given time constant in ms.
static inline int32_t decay_coef(uint32_t ms) {
  if (ms == 0) ms = 1;
  int32_t c = Q15 - (int32_t)(2048 / ms);
  return c < 0 ? 0 : c;
}

static inline void env_decay(int32_t *env, int32_t coef) { *env = q15mul(*env, coef); }

static inline void env_trigger(int32_t *env) { *env = Q15_MAX; }

// Attack/release gain (q15) for a segment of `len` samples starting at pos 0.
static inline int32_t ar_env(uint32_t pos, uint32_t len, uint32_t atk, uint32_t rel) {
  if (pos >= len) return 0;
  int32_t g = Q15_MAX;
  if (atk && pos < atk) g = (int32_t)(((int64_t)Q15_MAX * pos) / atk);
  if (rel && pos + rel > len) {
    int32_t r = (int32_t)(((int64_t)Q15_MAX * (len - pos)) / rel);
    if (r < g) g = r;
  }
  return g;
}
