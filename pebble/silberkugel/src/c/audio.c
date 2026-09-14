#include "audio.h"
#include "config.h"
#include "tone.h"
#include "e1clock.h"

static AudioStats s_st;
static AppTimer *s_tick;
static int16_t s_block[AUDIO_BLOCK_SAMPLES];
static uint32_t s_pend_off;
static uint32_t s_pend_len;
static uint32_t s_t0;              // Zeitpunkt des ersten Writes (Startpunkt der Abspieluhr)
static uint32_t s_next_open_ms;    // fruehester Zeitpunkt fuer (Wieder-)Oeffnen
static uint32_t s_open_ms;         // Zeitpunkt der letzten Oeffnung
static bool s_zero_active;         // seit s_zero_since_ms wird nichts angenommen
static uint32_t s_zero_since_ms;
static uint32_t s_stall_streak;    // Staus kurz nach der Oeffnung hintereinander
static uint32_t s_target_ms = AUDIO_TARGET_QUEUE_MS;
static AudioSource s_source = AudioSourceTone;
static bool s_calibrate;
static uint32_t s_last_tick_ms;

static const char *s_reason_names[] = { "Done", "Stopped", "Preempted", "Error" };

static void prv_reset_stream_state(void) {
  s_st.open = false;
  s_pend_off = 0;
  s_pend_len = 0;
  s_t0 = 0;
  s_st.written_bytes = 0;
  s_st.played_bytes_est = 0;
  s_st.queue_bytes_est = 0;
  s_st.queue_ms_est = 0;
  s_st.queue_min_bytes = 0xFFFFFFFFu;
  s_zero_active = false;
}

static void prv_finish(SpeakerFinishReason reason, void *ctx) {
  uint32_t now = e1clock_now_ms();
  const char *name = (unsigned)reason < 4 ? s_reason_names[reason] : "?";
  int status = (int)speaker_get_status();
  if (reason == SpeakerFinishReasonDone) {
    s_st.finish_done++;
  } else if (reason == SpeakerFinishReasonStopped) {
    s_st.finish_stopped++;
  }
  if ((reason == SpeakerFinishReasonDone || reason == SpeakerFinishReasonStopped) &&
      s_st.open && status != (int)SpeakerStatusIdle) {
    // Verspaeteter Callback eines frueheren Streams (z. B. nach Probe oder Stau-Neustart):
    // der aktuelle Stream spielt noch, nichts zuruecksetzen.
    APP_LOG(APP_LOG_LEVEL_INFO, "[P1][AUDIO] Ende %s (Status %d), veraltet, ignoriert", name, status);
    return;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "[P1][AUDIO] Ende %s (Status %d) bei t=%lu ms", name, status,
          (unsigned long)now);
  prv_reset_stream_state();
  switch (reason) {
    case SpeakerFinishReasonPreempted:
      s_st.preempted++;
      s_next_open_ms = now + 500;
      APP_LOG(APP_LOG_LEVEL_INFO, "[P1][AUDIO] preempted (Systemton), Neustart in 500 ms");
      break;
    case SpeakerFinishReasonError:
      s_st.errors++;
      s_next_open_ms = now + 1000;
      APP_LOG(APP_LOG_LEVEL_WARNING, "[P1][AUDIO] Fehler, Neustart in 1000 ms");
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
  s_open_ms = e1clock_now_ms();
  APP_LOG(APP_LOG_LEVEL_INFO, "[P1][AUDIO] Stream offen (16 kHz, 16 Bit), Oeffnung Nr. %lu",
          (unsigned long)s_st.open_count);
  return true;
}

// Fuellstand nachfuehren und bis target_bytes Vorlauf schreiben.
static void prv_fill(uint32_t now, uint32_t target_bytes, uint32_t max_blocks) {
  if (s_t0 != 0) {
    uint32_t el = now - s_t0;
    // Der Kalibrierzuschlag laesst die Schaetzung bewusst vorlaufen (+0,4 %),
    // damit der Ring irgendwann Backpressure meldet und wir den wahren
    // Fuellstand erfahren. Nur in den Diagnose-Bildschirmen aktiv.
    uint32_t played = el * AUDIO_BYTES_PER_MS + (s_calibrate ? (el >> 3) : 0);
    if (played > s_st.written_bytes) {
      if (s_st.queue_bytes_est > 0) {
        s_st.underruns++;
      }
      played = s_st.written_bytes;
    }
    s_st.played_bytes_est = played;
  }
  uint32_t queue = s_st.written_bytes - s_st.played_bytes_est;
  bool wanted_write = queue < target_bytes;
  uint32_t accepted = 0;
  uint32_t blocks = 0;
  while (queue < target_bytes && blocks < max_blocks) {
    if (s_pend_len == 0) {
      if (s_source == AudioSourceTone) {
        tone_render(s_block, AUDIO_BLOCK_SAMPLES);
      }   // Stille: s_block wurde beim Umschalten einmal gefuellt
      s_pend_off = 0;
      s_pend_len = AUDIO_BLOCK_BYTES;
    }
    uint32_t want = s_pend_len - s_pend_off;
    uint32_t w = speaker_stream_write((const uint8_t *)s_block + s_pend_off, want);
    s_pend_off += w;
    s_st.written_bytes += w;
    queue += w;
    accepted += w;
    if (s_t0 == 0 && w > 0) {
      s_t0 = now;
    }
    if (w < want) {
      // Backpressure ist die einzige Rueckmeldung der Firmware ueber den
      // wahren Fuellstand: der Ring ist jetzt exakt voll. Damit korrigieren
      // wir eine ueber die Zeit weggelaufene Schaetzung (Taktdrift, verlorene
      // Sekunden), die sonst niemand bemerken koennte.
      s_st.short_writes++;
      if (s_st.written_bytes >= AUDIO_RING_BYTES) {
        s_st.played_bytes_est = s_st.written_bytes - AUDIO_RING_BYTES;
        queue = AUDIO_RING_BYTES;
        s_st.recalibrations++;
      }
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
  if (queue < s_st.queue_min_bytes) {
    s_st.queue_min_bytes = queue;   // Tiefstand: trennt eigene Fehler von denen unter uns
  }

  // Stau-Erkennung: der Systemring (8 KB = 256 ms) leert sich normalerweise
  // mit 32 B/ms, auch bei Unterlauf (die Firmware schiebt dann Stille nach).
  // Nimmt er trotz leerem Vorlauf laenger als AUDIO_STALL_MS nichts an, steht
  // der Abfluss. Im QEMU-Emulator passiert das reproduzierbar; auf der Uhr
  // sollte es nie vorkommen. Neustart mit wachsender Pause.
  if (accepted > 0 || !wanted_write) {
    s_zero_active = false;
  } else if (!s_zero_active) {
    s_zero_active = true;
    s_zero_since_ms = now;
  } else if ((now - s_zero_since_ms) >= AUDIO_STALL_MS) {
    s_st.stalls++;
    s_stall_streak = (now - s_open_ms) < 5000 ? s_stall_streak + 1 : 1;
    uint32_t backoff = 200 * s_stall_streak;
    if (backoff > 5000) {
      backoff = 5000;
    }
    APP_LOG(APP_LOG_LEVEL_WARNING, "[P1][AUDIO] Stau Nr. %lu: %lu ms nichts angenommen, Neustart in %lu ms",
            (unsigned long)s_st.stalls, (unsigned long)(now - s_zero_since_ms), (unsigned long)backoff);
    speaker_stop();
    prv_reset_stream_state();
    s_next_open_ms = now + backoff;
  }
}

static void prv_tick(void *data) {
  s_tick = app_timer_register(AUDIO_TICK_MS, prv_tick, NULL);
  uint32_t now = e1clock_now_ms();
  if (s_last_tick_ms != 0) {
    uint32_t gap = now - s_last_tick_ms;
    if (gap > s_st.tick_gap_max_ms) {
      s_st.tick_gap_max_ms = gap;   // misst direkt, wie lange der App-Task stand
    }
  }
  s_last_tick_ms = now;
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
  prv_fill(now, s_target_ms * AUDIO_BYTES_PER_MS, AUDIO_MAX_BLOCKS_PER_TICK);
  uint32_t dur = e1clock_now_ms() - now;
  if (dur > s_st.fill_ms_max) {
    s_st.fill_ms_max = dur;
  }
}

void audio_set_source(AudioSource src) {
  s_source = src;
  s_pend_off = 0;
  s_pend_len = 0;
  if (src == AudioSourceSilence) {
    tone_render_silence(s_block, AUDIO_BLOCK_SAMPLES);
  }
}

AudioSource audio_source(void) {
  return s_source;
}

void audio_set_calibrate(bool on) {
  s_calibrate = on;
}

void audio_set_target_ms(uint32_t ms) {
  if (ms < AUDIO_LEAD_MIN_MS) {
    ms = AUDIO_LEAD_MIN_MS;
  }
  if (ms > AUDIO_LEAD_MAX_MS) {
    ms = AUDIO_LEAD_MAX_MS;
  }
  s_target_ms = ms;
}

uint32_t audio_target_ms(void) {
  return s_target_ms;
}

void audio_top_up(uint32_t ms) {
  if (!s_st.open) {
    return;
  }
  prv_fill(e1clock_now_ms(), ms * AUDIO_BYTES_PER_MS, 16);
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

// Sofort beenden: stream_close laesst den Stream erst 80 ms ausklingen, und
// solange er das tut, lehnt die Firmware gleichrangige Toene ab. Fuer den
// Referenzton im TON-Bildschirm muss der Lautsprecher wirklich frei sein.
void audio_stop_now(void) {
  s_st.wanted = false;
  speaker_stop();
  prv_reset_stream_state();
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
    APP_LOG(APP_LOG_LEVEL_WARNING, "[P1][PROBE] Lautsprecher stumm, keine Probe moeglich");
    return;
  }
  if (!speaker_stream_open(SpeakerPcmFormat_16kHz_16bit, AUDIO_VOLUME)) {
    s_st.open_failures++;
    APP_LOG(APP_LOG_LEVEL_WARNING, "[P1][PROBE] Stream liess sich nicht oeffnen");
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
  // Waehrend der Schleife spielt der Stream schon: Abfluss (Obergrenze) abziehen
  uint32_t drain = (t_end - t_start + 1) * AUDIO_BYTES_PER_MS;
  uint32_t cap = total > drain ? total - drain : total;
  s_st.capacity_raw_bytes = total;
  s_st.probe_drain_bytes = drain;
  s_st.capacity_bytes = cap;
  s_st.capacity_ms = cap / AUDIO_BYTES_PER_MS;
  s_st.probe_writes = writes;
  APP_LOG(APP_LOG_LEVEL_INFO,
          "[P1][PROBE] roh %lu B, Abfluss %lu B, Puffer %lu B = %lu ms, %lu Writes in %lu ms",
          (unsigned long)total, (unsigned long)drain, (unsigned long)cap,
          (unsigned long)s_st.capacity_ms, (unsigned long)writes, (unsigned long)(t_end - t_start));
  speaker_stop();               // Stille verwerfen statt ausspielen
  prv_reset_stream_state();
  s_next_open_ms = t_end + 300;
  s_st.wanted = was_wanted;
}

const AudioStats *audio_stats(void) {
  return &s_st;
}
