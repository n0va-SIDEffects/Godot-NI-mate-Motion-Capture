#pragma once
#include <pebble.h>

// PCM-Stream mit eigener Fuellstandsbuchhaltung: Es wird nur so viel
// nachgeschrieben, dass rund AUDIO_TARGET_QUEUE_MS im Systempuffer liegen.
// Wer einfach schreibt, bis Backpressure kommt, hoert jede Aenderung erst
// nach der vollen Puffergroesse.
typedef struct {
  bool wanted;
  bool open;
  bool muted;
  uint32_t capacity_bytes;      // aus der Probe, um den Abfluss waehrend der Schleife korrigiert
  uint32_t capacity_ms;
  uint32_t capacity_raw_bytes;  // unkorrigiert angenommene Bytes
  uint32_t probe_drain_bytes;   // geschaetzter Abfluss waehrend der Probe (Obergrenze)
  uint32_t probe_writes;
  uint32_t written_bytes;
  uint32_t played_bytes_est;
  uint32_t queue_bytes_est;
  uint32_t queue_ms_est;
  uint32_t underruns;
  uint32_t short_writes;
  uint32_t open_count;
  uint32_t open_failures;
  uint32_t preempted;
  uint32_t errors;
  uint32_t stalls;          // Stream nahm laenger als AUDIO_STALL_MS nichts an -> Neustart
  uint32_t finish_done;
  uint32_t finish_stopped;
  uint32_t ticks;
  uint32_t max_blocks_per_tick;
} AudioStats;

void audio_init(void);
void audio_deinit(void);
void audio_start(void);                // Stream gewuenscht (oeffnet bei Bedarf neu)
void audio_stop(void);                 // Stream schliessen
void audio_probe_capacity(void);       // Stille schreiben bis Backpressure, dann Neustart
void audio_top_up(uint32_t ms);        // sofort bis auf ms Vorlauf auffuellen (vor blockierenden Aufrufen)
const AudioStats *audio_stats(void);
