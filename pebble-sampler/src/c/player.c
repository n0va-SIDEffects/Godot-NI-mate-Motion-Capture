/*
 * Playback front-end. Note based sounds go straight to the speaker API;
 * synthesized sounds are rendered chunk by chunk into the PCM stream,
 * driven by an app timer so the UI stays responsive.
 */
#include "player.h"

#define CHUNK_SAMPLES        512     // 32 ms of audio per chunk
#define PUMP_INTERVAL_MS     10
#define MAX_CHUNKS_PER_PUMP  12      // upper bound on work per timer tick

static Synth s_synth;
static int16_t s_chunk[CHUNK_SAMPLES];
static uint32_t s_chunk_bytes;
static uint32_t s_chunk_off;
static AppTimer *s_timer;
static bool s_streaming;
static PlayerFinishedCb s_finished_cb;

static void prv_pump(void *ctx);

static void prv_cancel_timer(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
}

static void prv_pump(void *ctx) {
  s_timer = NULL;
  if (!s_streaming) return;

  for (int i = 0; i < MAX_CHUNKS_PER_PUMP; i++) {
    if (s_chunk_off >= s_chunk_bytes) {
      if (synth_done(&s_synth)) {
        // Everything has been handed over; let the buffer drain.
        s_streaming = false;
        speaker_stream_close();
        return;
      }
      uint32_t n = synth_render(&s_synth, s_chunk, CHUNK_SAMPLES);
      s_chunk_bytes = n * sizeof(int16_t);
      s_chunk_off = 0;
    }
    uint32_t written = speaker_stream_write((const uint8_t *)s_chunk + s_chunk_off,
                                            s_chunk_bytes - s_chunk_off);
    s_chunk_off += written;
    if (s_chunk_off < s_chunk_bytes) {
      break;  // stream buffer is full, try again on the next tick
    }
  }
  s_timer = app_timer_register(PUMP_INTERVAL_MS, prv_pump, NULL);
}

static void prv_speaker_finished(SpeakerFinishReason reason, void *ctx) {
  // Ignore stale notifications for a sound we already replaced.
  if (speaker_get_status() != SpeakerStatusIdle) return;
  s_streaming = false;
  prv_cancel_timer();
  if (s_finished_cb) s_finished_cb(reason);
}

void player_init(PlayerFinishedCb cb) {
  s_finished_cb = cb;
  speaker_set_finish_callback(prv_speaker_finished, NULL);
}

void player_deinit(void) {
  player_stop();
  speaker_set_finish_callback(NULL, NULL);
  s_finished_cb = NULL;
}

void player_stop(void) {
  prv_cancel_timer();
  s_streaming = false;
  speaker_stop();
}

bool player_is_playing(void) {
  return s_streaming || speaker_get_status() != SpeakerStatusIdle;
}

bool player_play(const Sound *sound, uint8_t volume) {
  player_stop();
  switch (sound->kind) {
    case SoundKindSynth: {
      synth_start(&s_synth, sound->render, sound->duration_ms, (uint32_t)rand() | 1u);
      if (!speaker_stream_open(SpeakerPcmFormat_16kHz_16bit, volume)) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "speaker_stream_open failed");
        return false;
      }
      s_streaming = true;
      s_chunk_bytes = 0;
      s_chunk_off = 0;
      prv_pump(NULL);  // pre-fill the stream buffer right away
      return true;
    }
    case SoundKindNotes: {
      bool ok = speaker_play_notes(sound->notes, sound->count, volume);
      if (!ok) APP_LOG(APP_LOG_LEVEL_ERROR, "speaker_play_notes failed for %s", sound->name);
      return ok;
    }
    case SoundKindTracks: {
      bool ok = speaker_play_tracks(sound->tracks, sound->count, volume);
      if (!ok) APP_LOG(APP_LOG_LEVEL_ERROR, "speaker_play_tracks failed for %s", sound->name);
      return ok;
    }
  }
  return false;
}
