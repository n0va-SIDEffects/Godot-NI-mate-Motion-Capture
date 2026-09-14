#pragma once
#include <pebble.h>

// Glasgarten, Etappe 1: Messgeruest fuer die Pebble Time 2 (emery).
// Alle Frequenzen in Centi-Hertz (1 Hz = 100), alle Zeiten in Millisekunden.

#define E1_VERSION "0.1"

// Display
#define SCR_W 200
#define SCR_H 228
#define HUD_H 34                     // obere HUD-Zeilen 0..33
#define SKY_Y0 HUD_H                 // Himmel 34..99
#define FLOOR_Y0 100                 // Kaustik-Boden 100..227
#define FLOOR_ROWS (SCR_H - FLOOR_Y0)
#define FLOWER_CX 100
#define FLOWER_CY 92
#define FLOWER_R 34
#define FLOWER_PETALS 8
#define BLOOM_R 40

// Audio
#define AUDIO_RATE_HZ 16000
#define AUDIO_BYTES_PER_MS 32        // 16 kHz * 2 Byte
#define AUDIO_BLOCK_SAMPLES 256      // 16 ms pro Block
#define AUDIO_BLOCK_BYTES (AUDIO_BLOCK_SAMPLES * 2)
#define AUDIO_TICK_MS 8
#define AUDIO_TARGET_QUEUE_MS 160    // Vorlauf: die Firmware holt 1024-B-Bloecke (32 ms) im
                                     // Systemtask (niedrigste Prioritaet); 56 ms waren zu knapp     // Ziel-Vorlauf im Systempuffer
#define AUDIO_MAX_BLOCKS_PER_TICK 6
#define AUDIO_VOLUME 85
#define AUDIO_TOPUP_MS 130           // Auffuellen vor blockierenden Aufrufen (vibes_cancel)
#define AUDIO_PIPELINE_ASSUMED_MS 80 // Annahme fuer Treiberpuffer, nur Anzeige; die Firmware
                                     // rechnet selbst mit 80 ms (SPEAKER_PIPELINE_DRAIN_SAMPLES)
#define AUDIO_RING_BYTES 8192        // pcm_stream der Firmware: 8 KB = 256 ms
#define AUDIO_STALL_MS 300           // so lange nichts angenommen trotz leerem Vorlauf = Stau
#define AUDIO_LEAD_MIN_MS 24         // Klick-Diagnose: Vorlauf live per Up/Down in LATENZ
#define AUDIO_LEAD_MAX_MS 240        // Ring hat 256 ms
#define AUDIO_LEAD_STEP_MS 16

// Klick-Diagnose TON: Referenzton der Firmware gegen unseren Stream
#define TON_TEST_FREQ_HZ 440
#define TON_TEST_MS 5000
#define TON_FLAT_CHZ 44000           // 440,00 Hz, gleiche Frequenz wie der Firmware-Ton

// Stimmen
#define FORK_MIN_CHZ    30000
#define FORK_MAX_CHZ   170000
#define FORK_START_CHZ  44000
#define WINDOW_CHZ       4000        // Resonanzfenster 40 Hz
#define CLEAR_CHZ        1200        // "deutlich" ab 12 Hz
#define LOCK_CHZ           50        // Einrasten unter 0,5 Hz
#define LOCK_AVG_MS      1000
#define KARENZ_MS        1000
#define BREAK_MS         3000
#define WARN_MS          1200
#define RESPAWN_MS       2000
#define FLOWER_MIDI_MIN    67        // G4
#define FLOWER_MIDI_MAX    91        // G6
#define FLOWER_MIN_SEMITONES 3

// Touch
#define PAN_SLOW_CHZ_PER_PX  25      // 0,25 Hz pro Pixel
#define PAN_FAST_CHZ_PER_PX 200      // hoechstens 2 Hz pro Pixel
#define PAN_FAST_AT_PX_S    600
#define PAN_DEAD_ZONE_PX      2
#define CLUTCH_MS           300
#define BUTTON_FREEZE_MS    200
#define FINE_STEP_CHZ        25

// Haptik
#define LRA_PULSE_MS           30
#define LRA_BUZZ_ON_MS        230     // Rauheit: 230 ms an, alle 250 ms
#define LRA_COUNTABLE_MAX_CHZ 500     // bis 5 Hz zaehlbar
#define LRA_BUZZ_MAX_CHZ      600     // bis 6 Hz Rauheit, darueber stumm
#define LRA_RATE_HYST_PCT      15

// Backlight
#define BL_BREATH_MAX_CHZ 300         // Atmen nur bis 3 Hz
#define BL_UPDATE_MS       50
#define BL_KEEPALIVE_MS   500
#define BL_HYST             6

// Timer
#define GAME_TICK_MS    20
#define RENDER_TICK_MS  33
#define LOG_TICK_MS   1000
#define PANEL_TEST_FRAMES 300
