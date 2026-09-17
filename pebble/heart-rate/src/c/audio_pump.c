// Uebernommen aus dem Skill "pebble-audio" (Theremin-App). Unveraendert bis auf diesen Hinweis
// und die Klammer fuer Uhren ohne Lautsprecher, wo die Speaker-Aufrufe leere Makros sind.
// Dort auf echter Hardware gemessen und erprobt.
#include "audio_pump.h"

#if defined(PBL_SPEAKER)

static AudioRenderFn s_render = NULL;
static AppTimer *s_timer = NULL;
static bool s_running = false;

static int16_t  s_chunk[AUDIO_CHUNK_SAMPLES];
static uint16_t s_chunk_len = 0, s_chunk_pos = 0;    // in Bytes
static uint32_t s_written = 0;                       // Samples, die wir abgegeben haben

// Bereinigte Stream-Uhr
static uint32_t s_clock_ms = 0, s_last_raw_ms = 0;

// Vorlauf und Diagnose
static uint16_t s_lead_ms = AUDIO_LEAD_MS;
static uint16_t s_underruns = 0, s_tight = 0;

#define SAMPLES_PER_MS (AUDIO_SAMPLE_RATE / 1000)
#define WRITES_PER_TICK 8

static uint32_t now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (uint32_t)s * 1000u + ms;
}

// time_ms() der Firmware liefert um Sekundengrenzen herum gelegentlich Werte,
// die um genau +/-1000 ms danebenliegen. Nur diesen Fall korrigieren: wer jedes
// dt > 900 ms als Sprung behandelt, verbucht eine echte Stockung von rund einer
// Sekunde als "keine Zeit vergangen" und laeuft dauerhaft hinterher.
static uint32_t stream_clock_ms(void) {
  uint32_t raw = now_ms();
  int32_t dt = (int32_t)(raw - s_last_raw_ms);
  s_last_raw_ms = raw;
  if (dt > 880 && dt < 1120)   dt -= 1000;
  if (dt < -880 && dt > -1120) dt += 1000;
  if (dt < 0)    dt = 0;
  if (dt > 2000) dt = 2000;
  s_clock_ms += (uint32_t)dt;
  return s_clock_ms;
}

static void audio_tick(void *ctx) {
  s_timer = NULL;
  if (!s_running) return;

  uint32_t clock = stream_clock_ms();
  uint32_t lead_samples = (uint32_t)s_lead_ms * SAMPLES_PER_MS;
  uint32_t allowed = (clock + s_lead_ms) * SAMPLES_PER_MS;
  uint32_t played  = clock * SAMPLES_PER_MS;

  // Was noch ungehoert im Geraetepuffer liegt.
  if (s_written > lead_samples / 2) {
    int32_t fill = (int32_t)s_written - (int32_t)played;
    if (fill <= 0) {
      s_underruns++;
      if (s_lead_ms < AUDIO_LEAD_MAX_MS) s_lead_ms += 30;   // kuenftig mehr Luft
    } else if (fill < (int32_t)(30 * SAMPLES_PER_MS)) {
      s_tight++;
    }
  }

  // Nach einer laengeren Stockung nicht das Versaeumte nachschieben -- das
  // waere laengst veraltetes Audio. Lieber neu ansetzen.
  if (allowed > s_written + 3 * lead_samples) {
    s_written = allowed - lead_samples;
  }

  int budget = WRITES_PER_TICK;
  while (s_written < allowed && budget-- > 0) {
    if (s_chunk_pos >= s_chunk_len) {
      s_render(s_chunk, AUDIO_CHUNK_SAMPLES);
      s_chunk_len = AUDIO_CHUNK_SAMPLES * sizeof(int16_t);
      s_chunk_pos = 0;
    }
    uint32_t n = speaker_stream_write((const uint8_t *)s_chunk + s_chunk_pos,
                                      s_chunk_len - s_chunk_pos);
    if (n == 0) break;                       // Geraetepuffer voll, naechster Tick
    s_chunk_pos += n;
    s_written += n / sizeof(int16_t);
  }

  s_timer = app_timer_register(AUDIO_TICK_MS, audio_tick, NULL);
}

bool audio_pump_start(AudioRenderFn render, uint8_t volume) {
  if (s_running || render == NULL) return false;
  if (!speaker_stream_open(SpeakerPcmFormat_16kHz_16bit, volume)) return false;
  s_render = render;
  s_running = true;
  s_last_raw_ms = now_ms();
  s_clock_ms = 0;
  s_written = 0;
  s_chunk_len = s_chunk_pos = 0;
  audio_tick(NULL);
  return true;
}

void audio_pump_stop(void) {
  if (!s_running) return;
  s_running = false;
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  speaker_stream_close();
}

bool audio_pump_is_running(void) { return s_running; }

void audio_pump_set_volume(uint8_t volume) {
  if (s_running) speaker_set_volume(volume);
}

uint16_t audio_pump_underruns(void) { return s_underruns; }
uint16_t audio_pump_tight(void)     { return s_tight; }
uint16_t audio_pump_lead_ms(void)   { return s_lead_ms; }

void audio_pump_reset_stats(void) {
  s_underruns = 0;
  s_tight = 0;
  s_lead_ms = AUDIO_LEAD_MS;
}

#else   // keine Speaker-API auf dieser Uhr

bool audio_pump_start(AudioRenderFn render, uint8_t volume) { (void)render; (void)volume; return false; }
void audio_pump_stop(void) {}
bool audio_pump_is_running(void) { return false; }
void audio_pump_set_volume(uint8_t volume) { (void)volume; }
uint16_t audio_pump_underruns(void) { return 0; }
uint16_t audio_pump_tight(void) { return 0; }
uint16_t audio_pump_lead_ms(void) { return 0; }
void audio_pump_reset_stats(void) {}

#endif
