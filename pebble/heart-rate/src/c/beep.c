#include "beep.h"

#if defined(PBL_SPEAKER)

#include "beep_sample.h"

#define BEEP_SAMPLES      ((uint32_t)(sizeof(s_beep_pcm) / sizeof(s_beep_pcm[0])))
#define STREAM_FORMAT     SpeakerPcmFormat_16kHz_16bit
#define BYTES_PER_MS      (BEEP_SAMPLE_RATE_HZ / 1000 * 2)   // 16 kHz, 16 bit, mono
//! How far ahead of the speaker the stream is kept filled. Run it dry and it clicks.
#define STREAM_LEAD_MS    70
//! How often the stream is topped up. Well below the lead, so a single late pump cannot empty it.
#define PUMP_MS           10
//! Biggest slice written in one go, and the size of the working buffer: 128 ms of audio.
#define CHUNK_SAMPLES     2048

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
static AppTimer *s_pump_timer;
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

//! One sample of the beep at the wanted pitch, or silence once it has run out.
static int16_t sample_at(uint32_t pos_q16) {
  const uint32_t index = pos_q16 >> 16;
  if (index >= BEEP_SAMPLES) {
    return 0;
  }
  const int32_t a = s_beep_pcm[index];
  const int32_t b = (index + 1 < BEEP_SAMPLES) ? s_beep_pcm[index + 1] : 0;
  const int32_t frac = (int32_t)(pos_q16 & 0xFFFF);
  return (int16_t)(a + (((b - a) * frac) >> 16));
}

static void pump_timer_callback(void *context);

static void stream_close(void) {
  if (s_pump_timer) {
    app_timer_cancel(s_pump_timer);
    s_pump_timer = NULL;
  }
  if (s_stream_open) {
    speaker_stream_close();
    s_stream_open = false;
  }
}

static void stream_open(void) {
  if (s_stream_open) {
    return;
  }
  // The speaker refuses a new session while it is still busy with the previous one.
  if (speaker_get_status() != SpeakerStatusIdle) {
    return;
  }
  if (!speaker_stream_open(STREAM_FORMAT, s_settings.volume)) {
    return;   // beep_play falls back to handing over one sample per beat
  }
  s_stream_open = true;
  s_stream_start_ms = now_ms();
  s_written_bytes = 0;
  s_pos_q16 = BEEP_SAMPLES << 16;   // start silent
  if (!s_pump_timer) {
    s_pump_timer = app_timer_register(PUMP_MS, pump_timer_callback, NULL);
  }
}

void beep_setup(const Settings *settings) {
  const bool was_stream = s_stream_open;
  s_settings = *settings;
  s_step_q16 = pitch_step_q16(s_settings.pitch_note);

  const bool want_stream = (s_settings.sound_mode == BeepModeStream) && s_settings.sound_on &&
                           s_settings.volume > 0;
  if (!want_stream) {
    stream_close();
    return;
  }
  if (was_stream) {
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
  // The speaker refuses a second call while it is still busy with the last beep. Nothing useful
  // can be done about that from here, so just ask and let it decide.
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

  const uint32_t start_pos = s_pos_q16;
  for (uint32_t i = 0; i < samples; i++) {
    s_chunk[i] = sample_at(start_pos + i * s_step_q16);
  }
  const uint32_t accepted = speaker_stream_write(s_chunk, samples * 2);
  s_written_bytes += accepted;
  // Advance by exactly what the speaker took, so a partial write neither skips nor repeats any of
  // the beep. Getting this wrong replayed fragments of it over and over.
  if ((start_pos >> 16) < BEEP_SAMPLES) {
    s_pos_q16 = start_pos + (accepted / 2) * s_step_q16;
  }
}

static void pump_timer_callback(void *context) {
  s_pump_timer = NULL;
  beep_pump();
  if (s_stream_open) {
    s_pump_timer = app_timer_register(PUMP_MS, pump_timer_callback, NULL);
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
