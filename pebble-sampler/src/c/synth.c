#include "synth.h"

void synth_start(Synth *s, SynthRenderFn fn, uint32_t duration_ms, uint32_t seed) {
  memset(s, 0, sizeof(*s));
  s->fn = fn;
  s->total = SYNTH_MS(duration_ms);
  s->rng = seed ? seed : 0x9E3779B9u;
}

uint32_t synth_render(Synth *s, int16_t *out, uint32_t max_samples) {
  uint32_t n = 0;
  while (n < max_samples && s->t < s->total) {
    out[n++] = (int16_t)clip16(s->fn(s));
    s->t++;
  }
  return n;
}

bool synth_done(const Synth *s) {
  return s->t >= s->total;
}
