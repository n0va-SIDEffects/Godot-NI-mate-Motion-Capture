#include "beep.h"

#if defined(PBL_SPEAKER)

#include "beep_sample.h"

#define BEEP_SAMPLES      ((uint32_t)(sizeof(s_beep_pcm) / sizeof(s_beep_pcm[0])))
#define STREAM_FORMAT     SpeakerPcmFormat_16kHz_16bit
#define BYTES_PER_MS      (BEEP_SAMPLE_RATE_HZ / 1000 * 2)   // 16 kHz, 16 bit, mono
//! How far ahead of the speaker the stream is kept filled. Long enough that a late pump cannot
//! run it dry, which would click, and short enough that a beat is not heard noticeably late.
#define STREAM_LEAD_MS    70
//! Biggest slice written in one go; also the size of the working buffer.
#define CHUNK_SAMPLES     640

static const SpeakerSample s_sample = {
  .data = s_beep_pcm,
  .num_bytes = sizeof(s_beep_pcm),
  .format = STREAM_FORMAT,
  .base_midi_note = BEEP_MIDI_NOTE,
  .loop = false,
};

static Settings s_settings;
static bool s_stream_open;
static uint32_t s_stream_start_ms;   // when the stream was opened
static uint32_t s_written_bytes;     // bytes handed over since then
static uint32_t s_pos_q16;           // position in the sample, 16.16; past the end means silent
static uint32_t s_step_q16;          // how far to advance per output sample: the pitch
static int16_t s_chunk[CHUNK_SAMPLES];

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

//! One sample of the beep, resampled to the wanted pitch, or silence once it has run out.
static int16_t next_sample(void) {
  const uint32_t index = s_pos_q16 >> 16;
  if (index >= BEEP_SAMPLES) {
    return 0;
  }
  const int32_t a = s_beep_pcm[index];
  const int32_t b = (index + 1 < BEEP_SAMPLES) ? s_beep_pcm[index + 1] : 0;
  const int32_t frac = (int32_t)(s_pos_q16 & 0xFFFF);
  s_pos_q16 += s_step_q16;
  return (int16_t)(a + (((b - a) * frac) >> 16));
}

static void stream_close(void) {
  if (s_stream_open) {
    speaker_stream_close();
    s_stream_open = false;
  }
}

static void stream_open(void) {
  if (s_stream_open) {
    return;
  }
  if (!speaker_stream_open(STREAM_FORMAT, s_settings.volume)) {
    return;   // the caller falls back to playing each beat on its own
  }
  s_stream_open = true;
  s_stream_start_ms = now_ms();
  s_written_bytes = 0;
  s_pos_q16 = BEEP_SAMPLES << 16;   // start silent
}

void beep_setup(const Settings *settings) {
  const bool was_stream = s_settings.sound_mode == BeepModeStream;
  s_settings = *settings;
  s_step_q16 = pitch_step_q16(s_settings.pitch_note);

  const bool want_stream = (s_settings.sound_mode == BeepModeStream) && s_settings.sound_on &&
                           s_settings.volume > 0;
  if (!want_stream) {
    stream_close();
    return;
  }
  if (s_stream_open && was_stream) {
    speaker_set_volume(s_settings.volume);
    return;
  }
  stream_open();
}

void beep_play(void) {
  if (!s_settings.sound_on || s_settings.volume == 0 || speaker_is_muted()) {
    return;
  }
  if (s_stream_open) {
    s_pos_q16 = 0;   // the pump picks this up and writes the beep into the stream
    beep_pump();
    return;
  }
  const SpeakerNote note = {
    .midi_note = s_settings.pitch_note,
    .waveform = SpeakerWaveformSine,   // ignored while a sample is attached
    .duration_ms = BEEP_LEN_MS + 5,    // a little headroom so the release is not cut off
    .velocity = 0,
    .reserved = 0,
  };
  const SpeakerTrack track = {.notes = &note, .num_notes = 1, .sample = &s_sample};
  speaker_play_tracks(&track, 1, s_settings.volume);
}

void beep_pump(void) {
  if (!s_stream_open) {
    return;
  }
  // The speaker consumes at a fixed rate, so the clock says how much of what was written is left.
  const uint32_t consumed = (now_ms() - s_stream_start_ms) * BYTES_PER_MS;
  const int32_t buffered = (int32_t)(s_written_bytes - consumed);
  int32_t wanted = (int32_t)(STREAM_LEAD_MS * BYTES_PER_MS) - buffered;
  if (wanted <= 0) {
    return;
  }
  uint32_t samples = (uint32_t)wanted / 2;
  if (samples > CHUNK_SAMPLES) {
    samples = CHUNK_SAMPLES;
  }
  for (uint32_t i = 0; i < samples; i++) {
    s_chunk[i] = next_sample();
  }
  const uint32_t accepted = speaker_stream_write(s_chunk, samples * 2);
  s_written_bytes += accepted;
  const uint32_t rejected = samples - accepted / 2;
  if (rejected > 0 && (s_pos_q16 >> 16) < BEEP_SAMPLES + rejected) {
    // The buffer took less than offered: wind the beep back so nothing is skipped.
    const uint32_t back = rejected * s_step_q16;
    s_pos_q16 = (s_pos_q16 > back) ? s_pos_q16 - back : 0;
  }
}

void beep_teardown(void) {
  stream_close();
  speaker_stop();
}

#else   // no speaker on this watch

void beep_setup(const Settings *settings) { (void)settings; }
void beep_play(void) {}
void beep_pump(void) {}
void beep_teardown(void) {}

#endif
