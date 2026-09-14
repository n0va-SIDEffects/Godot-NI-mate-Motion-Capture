#include "tone.h"
#include "config.h"

#define LUT_BITS 8
#define LUT_SIZE (1 << LUT_BITS)

static int16_t s_lut[LUT_SIZE];
static uint32_t s_phase;
static uint32_t s_inc;

void tone_init(void) {
  for (int i = 0; i < LUT_SIZE; i++) {
    int32_t a = (int32_t)(((int64_t)i * TRIG_MAX_ANGLE) / LUT_SIZE);
    s_lut[i] = (int16_t)((sin_lookup(a) * 5000) / TRIG_MAX_RATIO);
  }
  s_phase = 0;
  tone_set_hz(TONE_HZ);
}

void tone_set_hz(uint16_t hz) {
  // Phasenzuwachs pro Sample in 32-Bit-Festkomma: volle Umdrehung = 2^32.
  s_inc = (uint32_t)(((uint64_t)hz << 32) / AUDIO_RATE_HZ);
}

void tone_render(int16_t *out, uint32_t num_samples) {
  uint32_t phase = s_phase;
  for (uint32_t i = 0; i < num_samples; i++) {
    out[i] = s_lut[phase >> (32 - LUT_BITS)];
    phase += s_inc;
  }
  s_phase = phase;
}

void tone_render_silence(int16_t *out, uint32_t num_samples) {
  for (uint32_t i = 0; i < num_samples; i++) {
    out[i] = 0;
  }
}
