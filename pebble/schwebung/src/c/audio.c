#include "audio.h"
#include "config.h"
#include "synth.h"
#include "e1clock.h"

static AudioStats s_st;
static AppTimer *s_tick;
static int16_t s_block[AUDIO_BLOCK_SAMPLES];
static uint32_t s_pend_off;
static uint32_t s_pend_len;
static uint32_t s_t0;              // Zeitpunkt des ersten Writes (Startpunkt der Abspieluhr)
static uint32_t s_next_open_ms;    // fruehester Zeitpunkt fuer (Wieder-)Oeffnen

static void prv_reset_stream_state(void) {
  s_st.open = false;
  s_pend_off = 0;
  s_pend_len = 0;
  s_t0 = 0;
  s_st.written_bytes = 0;
  s_st.played_bytes_est = 0;
  s_st.queue_bytes_est = 0;
  s_st.queue_ms_est = 0;
}

static void prv_finish(SpeakerFinishReason reason, void *ctx) {
  uint32_t now = e1clock_now_ms();
  if ((reason == SpeakerFinishReasonDone || reason == SpeakerFinishReasonStopped) &&
      s_st.open && speaker_get_status() != SpeakerStatusIdle) {
    // Verspaeteter Callback eines frueheren Streams (z. B. nach der Probe):
    // der aktuelle Stream spielt noch, nichts zuruecksetzen.
    return;
  }
  prv_reset_stream_state();
  switch (reason) {
    case SpeakerFinishReasonPreempted:
      s_st.preempted++;
      s_next_open_ms = now + 500;
      APP_LOG(APP_LOG_LEVEL_INFO, "[E1][AUDIO] preempted (Systemton), Neustart in 500 ms");
      break;
    case SpeakerFinishReasonError:
      s_st.errors++;
      s_next_open_ms = now + 1000;
      APP_LOG(APP_LOG_LEVEL_WARNING, "[E1][AUDIO] Fehler, Neustart in 1000 ms");
      break;
    default:
      // Done oder Stopped: von uns ausgeloest
      break;
  }
}

static bool prv_open(void) {
  if (speaker_is_muted()) {
    s_st.muted = true;
    return false;
  }
  s_st.muted = false;
  if (!speaker_stream_open(SpeakerPcmFormat_16kHz_16bit, AUDIO_VOLUME)) {
    s_st.open_failures++;
    return false;
  }
  prv_reset_stream_state();
  s_st.open = true;
  s_st.open_count++;
  APP_LOG(APP_LOG_LEVEL_INFO, "[E1][AUDIO] Stream offen (16 kHz, 16 Bit), Oeffnung Nr. %lu",
          (unsigned long)s_st.open_count);
  return true;
}

static void prv_tick(void *data) {
  s_tick = app_timer_register(AUDIO_TICK_MS, prv_tick, NULL);
  uint32_t now = e1clock_now_ms();
  s_st.ticks++;
  if (!s_st.wanted) {
    return;
  }
  if (!s_st.open) {
    if ((int32_t)(now - s_next_open_ms) >= 0) {
      if (!prv_open()) {
        s_next_open_ms = now + 500;
      }
    }
    if (!s_st.open) {
      return;
    }
  }

  // Abspieluhr: seit dem ersten Write laufen 32 Byte pro Millisekunde ab.
  if (s_t0 != 0) {
    uint32_t played = (now - s_t0) * AUDIO_BYTES_PER_MS;
    if (played > s_st.written_bytes) {
      if (s_st.queue_bytes_est > 0) {
        s_st.underruns++;
      }
      played = s_st.written_bytes;
    }
    s_st.played_bytes_est = played;
  }
  uint32_t queue = s_st.written_bytes - s_st.played_bytes_est;
  const uint32_t target = AUDIO_TARGET_QUEUE_MS * AUDIO_BYTES_PER_MS;
  uint32_t blocks = 0;
  while (queue < target && blocks < AUDIO_MAX_BLOCKS_PER_TICK) {
    if (s_pend_len == 0) {
      synth_render(s_block, AUDIO_BLOCK_SAMPLES);
      s_pend_off = 0;
      s_pend_len = AUDIO_BLOCK_BYTES;
    }
    uint32_t want = s_pend_len - s_pend_off;
    uint32_t w = speaker_stream_write((const uint8_t *)s_block + s_pend_off, want);
    s_pend_off += w;
    s_st.written_bytes += w;
    queue += w;
    if (s_t0 == 0 && w > 0) {
      s_t0 = now;
    }
    if (w < want) {
      s_st.short_writes++;   // Backpressure: Rest im naechsten Tick
      break;
    }
    s_pend_len = 0;
    blocks++;
  }
  if (blocks > s_st.max_blocks_per_tick) {
    s_st.max_blocks_per_tick = blocks;
  }
  s_st.queue_bytes_est = queue;
  s_st.queue_ms_est = queue / AUDIO_BYTES_PER_MS;
}

void audio_init(void) {
  s_st = (AudioStats){ 0 };
  s_next_open_ms = 0;
  speaker_set_finish_callback(prv_finish, NULL);
  s_tick = app_timer_register(AUDIO_TICK_MS, prv_tick, NULL);
}

void audio_deinit(void) {
  if (s_tick) {
    app_timer_cancel(s_tick);
    s_tick = NULL;
  }
  if (s_st.open) {
    speaker_stop();
    prv_reset_stream_state();
  }
  speaker_set_finish_callback(NULL, NULL);
}

void audio_start(void) {
  s_st.wanted = true;
}

void audio_stop(void) {
  s_st.wanted = false;
  if (s_st.open) {
    speaker_stream_close();     // spielt den Rest aus, Finish-Callback kommt mit Done
    s_st.open = false;
  }
}

void audio_probe_capacity(void) {
  static const int16_t zeros[AUDIO_BLOCK_SAMPLES] = { 0 };
  bool was_wanted = s_st.wanted;
  if (s_st.open) {
    speaker_stop();
    prv_reset_stream_state();
  }
  if (speaker_is_muted()) {
    s_st.muted = true;
    APP_LOG(APP_LOG_LEVEL_WARNING, "[E1][PROBE] Lautsprecher stumm, keine Probe moeglich");
    return;
  }
  if (!speaker_stream_open(SpeakerPcmFormat_16kHz_16bit, AUDIO_VOLUME)) {
    s_st.open_failures++;
    APP_LOG(APP_LOG_LEVEL_WARNING, "[E1][PROBE] Stream liess sich nicht oeffnen");
    return;
  }
  uint32_t t_start = e1clock_now_ms();
  uint32_t total = 0;
  uint32_t writes = 0;
  while (writes < 1024) {
    uint32_t w = speaker_stream_write(zeros, AUDIO_BLOCK_BYTES);
    total += w;
    writes++;
    if (w < AUDIO_BLOCK_BYTES) {
      break;
    }
  }
  uint32_t t_end = e1clock_now_ms();
  s_st.capacity_bytes = total;
  s_st.capacity_ms = total / AUDIO_BYTES_PER_MS;
  s_st.probe_writes = writes;
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[E1][PROBE] Systempuffer nimmt %lu Byte = %lu ms an (%lu Schreibvorgaenge in %lu ms)",
          (unsigned long)total, (unsigned long)s_st.capacity_ms, (unsigned long)writes,
          (unsigned long)(t_end - t_start));
  speaker_stop();               // Stille verwerfen statt ausspielen
  prv_reset_stream_state();
  s_next_open_ms = t_end + 300;
  s_st.wanted = was_wanted;
}

const AudioStats *audio_stats(void) {
  return &s_st;
}
