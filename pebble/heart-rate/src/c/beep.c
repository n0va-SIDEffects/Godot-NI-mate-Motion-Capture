#include "beep.h"

#if defined(PBL_SPEAKER)

#include "audio_pump.h"
#include "beep_sample.h"

#define BEEP_SAMPLES  ((uint32_t)(sizeof(s_beep_pcm) / sizeof(s_beep_pcm[0])))
//! How long the stream stays open after the last beat before it is closed again. The amplifier
//! hisses faintly while a stream is open, so it is not left open once the pulse is gone; longer
//! than any plausible beat interval, so it stays open while a pulse is being followed.
#define GATE_MS       2500

static const Settings *s_settings;   // owned by the app, read afresh at every beat
static uint32_t s_pos_q16;           // position in the sample, 16.16; past the end means silent
static uint32_t s_step_q16;          // how far to advance per output sample: the pitch
static uint32_t s_last_beat_ms;

static uint32_t now_ms(void) {
  time_t seconds;
  uint16_t millis;
  time_ms(&seconds, &millis);
  return (uint32_t)seconds * 1000u + millis;
}

//! Playback speed for a MIDI note, as 16.16, relative to the note the sample was recorded at.
static uint32_t pitch_step_q16(uint8_t note) {
  // 2^(n/12) in 16.16 for n = 0 to 11.
  static const uint32_t semitone[12] = {
    65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193, 104032, 110218, 116772, 123715,
  };
  int delta = (int)note - BEEP_MIDI_NOTE;
  int octave = 0;
  while (delta < 0) {
    delta += 12;
    octave--;
  }
  while (delta >= 12) {
    delta -= 12;
    octave++;
  }
  uint32_t step = semitone[delta];
  if (octave > 0) {
    step <<= (octave > 4 ? 4 : octave);
  } else if (octave < 0) {
    step >>= (-octave > 4 ? 4 : -octave);
  }
  return step ? step : 1;
}

//! Fill a block for the stream: the beep where one is due, silence the rest of the time.
static void render(int16_t *out, uint16_t count) {
  for (uint16_t i = 0; i < count; i++) {
    const uint32_t index = s_pos_q16 >> 16;
    if (index + 1 >= BEEP_SAMPLES) {
      out[i] = 0;
      continue;
    }
    const int32_t a = s_beep_pcm[index];
    const int32_t b = s_beep_pcm[index + 1];
    const int32_t frac = (int32_t)(s_pos_q16 & 0xFFFF);
    out[i] = (int16_t)(a + (((b - a) * frac) >> 16));
    s_pos_q16 += s_step_q16;
  }
}

static bool sound_wanted(void) {
  return s_settings && s_settings->sound_on && s_settings->volume > 0 && !speaker_is_muted();
}

void beep_setup(const Settings *settings) {
  s_settings = settings;
  s_step_q16 = pitch_step_q16(settings->pitch_note);
  if (audio_pump_is_running()) {
    if (sound_wanted()) {
      audio_pump_set_volume(s_settings->volume);
    } else {
      audio_pump_stop();
    }
  }
}

void beep_play(void) {
  if (!sound_wanted()) {
    return;
  }
  s_last_beat_ms = now_ms();

  if (!audio_pump_is_running()) {
    s_pos_q16 = BEEP_SAMPLES << 16;   // open silent; the beep is queued right after
    audio_pump_reset_stats();   // the count belongs to this stream, not to every past one
    if (!audio_pump_start(render, s_settings->volume)) {
      return;   // the speaker is busy with something else; the next beat tries again
    }
  }
  s_pos_q16 = 0;
}

void beep_tick(void) {
  // Close the stream once the pulse has been gone for a while, so the amplifier does not hiss
  // into an empty room.
  if (audio_pump_is_running() && (now_ms() - s_last_beat_ms) > GATE_MS) {
    audio_pump_stop();
  }
}

uint16_t beep_underruns(void) {
  return audio_pump_underruns();
}

void beep_teardown(void) {
  audio_pump_stop();
  speaker_stop();
}

#else   // no speaker on this watch

void beep_setup(const Settings *settings) { (void)settings; }
void beep_play(void) {}
void beep_tick(void) {}
uint16_t beep_underruns(void) { return 0; }
void beep_teardown(void) {}

#endif
