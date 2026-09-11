/*
 * Playback front-end. Note based sounds go straight to the speaker API;
 * synthesized sounds are rendered chunk by chunk into the PCM stream,
 * driven by an app timer so the UI stays responsive.
 */
#include "player.h"

#define CHUNK_BYTES          1024    // 32 ms of 16-bit audio, 64 ms of 8-bit audio
#define CHUNK_SAMPLES        (CHUNK_BYTES / 2)
#define PUMP_INTERVAL_MS     10
#define MAX_CHUNKS_PER_PUMP  12      // upper bound on work per timer tick

typedef enum { SourceNone, SourceSynth, SourceResource } StreamSource;

static StreamSource s_source;
static Synth s_synth;
static ResHandle s_res;
static uint32_t s_res_size;
static uint32_t s_res_off;
static union { int16_t pcm16[CHUNK_SAMPLES]; uint8_t bytes[CHUNK_BYTES]; } s_chunk;
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

static bool prv_source_done(void) {
  switch (s_source) {
    case SourceSynth:    return synth_done(&s_synth);
    case SourceResource: return s_res_off >= s_res_size;
    default:             return true;
  }
}

// Fills s_chunk from the active source; returns the number of bytes produced.
static uint32_t prv_fill_chunk(void) {
  switch (s_source) {
    case SourceSynth:
      return synth_render(&s_synth, s_chunk.pcm16, CHUNK_SAMPLES) * sizeof(int16_t);
    case SourceResource: {
      uint32_t want = s_res_size - s_res_off;
      if (want > CHUNK_BYTES) want = CHUNK_BYTES;
      uint32_t got = resource_load_byte_range(s_res, s_res_off, s_chunk.bytes, want);
      if (got == 0) s_res_off = s_res_size;  // read error: end the sound
      s_res_off += got;
      return got;
    }
    default:
      return 0;
  }
}

static void prv_pump(void *ctx) {
  s_timer = NULL;
  if (!s_streaming) return;

  for (int i = 0; i < MAX_CHUNKS_PER_PUMP; i++) {
    if (s_chunk_off >= s_chunk_bytes) {
      if (prv_source_done()) {
        // Everything has been handed over; let the buffer drain.
        s_streaming = false;
        speaker_stream_close();
        return;
      }
      s_chunk_bytes = prv_fill_chunk();
      s_chunk_off = 0;
    }
    uint32_t written = speaker_stream_write(s_chunk.bytes + s_chunk_off,
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

static bool prv_start_stream(StreamSource source, SpeakerPcmFormat format, uint8_t volume) {
  if (!speaker_stream_open(format, volume)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "speaker_stream_open failed");
    s_source = SourceNone;
    return false;
  }
  s_source = source;
  s_streaming = true;
  s_chunk_bytes = 0;
  s_chunk_off = 0;
  prv_pump(NULL);  // pre-fill the stream buffer right away
  return true;
}

bool player_play(const Sound *sound, uint8_t volume) {
  player_stop();
  switch (sound->kind) {
    case SoundKindSynth:
      synth_start(&s_synth, sound->render, sound->duration_ms, (uint32_t)rand() | 1u);
      return prv_start_stream(SourceSynth, SpeakerPcmFormat_16kHz_16bit, volume);
    case SoundKindSample:
      s_res = resource_get_handle(sound->resource_id);
      s_res_size = resource_size(s_res);
      s_res_off = 0;
      if (s_res_size == 0) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "sample resource %s is empty", sound->name);
        return false;
      }
      return prv_start_stream(SourceResource, (SpeakerPcmFormat)sound->pcm_format, volume);
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
