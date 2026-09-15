#include "beep.h"

#if defined(PBL_SPEAKER)

#include "beep_sample.h"

static const SpeakerSample s_sample = {
  .data = s_beep_pcm,
  .num_bytes = sizeof(s_beep_pcm),
  .format = SpeakerPcmFormat_16kHz_16bit,
  .base_midi_note = BEEP_MIDI_NOTE,
  .loop = false,
};

static Settings s_settings;

void beep_setup(const Settings *settings) {
  s_settings = *settings;
}

void beep_play(void) {
  if (!s_settings.sound_on || s_settings.volume == 0 || speaker_is_muted()) {
    return;
  }
  // Playing the sample at a note other than its own shifts the pitch by resampling. The speaker
  // refuses a second call while it is still busy with the last beep; nothing useful can be done
  // about that from here, so just ask and let it decide.
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

void beep_teardown(void) {
  speaker_stop();
}

#else   // no speaker on this watch

void beep_setup(const Settings *settings) { (void)settings; }
void beep_play(void) {}
void beep_teardown(void) {}

#endif
