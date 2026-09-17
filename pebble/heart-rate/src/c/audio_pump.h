// Uebernommen aus dem Skill "pebble-audio" (Theremin-App), unveraendert bis auf diesen
// Hinweis. Dort auf echter Hardware gemessen und erprobt.
// Audio-Pumpe fuer die Pebble Time 2 (Plattform emery).
//
// Haelt den Lautsprecher-Stream gefuellt, ohne mehr Latenz als noetig, und
// kommt nach einer Stockung von selbst wieder in den Tritt. Die App liefert
// nur die Samples.
//
// Benutzung:
//
//   static void render(int16_t *out, uint16_t n) {
//     for (uint16_t i = 0; i < n; i++) out[i] = naechstes_sample();
//   }
//   ...
//   audio_pump_start(render, 60);        // Lautstaerke 0..100
//   ...
//   audio_pump_stop();
//
// Die Render-Funktion laeuft im App-Thread, nicht in einem Interrupt. Sie darf
// alles, was schnell ist -- aber nichts, was blockiert: kein persist_write,
// kein app_message_outbox_send, kein vibes_*.
#pragma once
#include <pebble.h>

#define AUDIO_SAMPLE_RATE   16000
#define AUDIO_CHUNK_SAMPLES 256      // 512 Bytes, Vielfaches der 256-Byte-Granularitaet
#define AUDIO_LEAD_MS       150      // Vorlauf vor der Wiedergabe
#define AUDIO_LEAD_MAX_MS   300      // so weit darf er wachsen
#define AUDIO_TICK_MS       8

//! Fuellt n Samples. Wird aus dem App-Thread aufgerufen.
typedef void (*AudioRenderFn)(int16_t *out, uint16_t n);

//! Oeffnet den Stream und startet die Pumpe. Liefert false, wenn der
//! Lautsprecher nicht verfuegbar ist.
bool audio_pump_start(AudioRenderFn render, uint8_t volume);

//! Haelt die Pumpe an und schliesst den Stream.
void audio_pump_stop(void);

//! Laeuft die Pumpe gerade?
bool audio_pump_is_running(void);

//! Lautstaerke waehrend des Betriebs aendern (0..100).
void audio_pump_set_volume(uint8_t volume);

//! Diagnose: wie oft der Puffer leerlief, wie oft es knapp wurde, und wie
//! viel Vorlauf gerade verwendet wird. Bleibt die erste Zahl beim Spielen
//! auf null, haelt die Audiokette durch.
uint16_t audio_pump_underruns(void);
uint16_t audio_pump_tight(void);
uint16_t audio_pump_lead_ms(void);
void     audio_pump_reset_stats(void);
