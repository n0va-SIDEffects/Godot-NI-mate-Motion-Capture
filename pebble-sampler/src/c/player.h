#pragma once
#include <pebble.h>
#include "sounds.h"

// Called on the app task whenever playback ends for any reason.
typedef void (*PlayerFinishedCb)(SpeakerFinishReason reason);

void player_init(PlayerFinishedCb cb);
void player_deinit(void);

// Starts playing `sound` at `volume` (0-100), replacing anything playing.
bool player_play(const Sound *sound, uint8_t volume);
// Plays a 16 kHz IMA ADPCM sample held in RAM (e.g. loaded from the phone).
bool player_play_memory(const uint8_t *data, uint32_t size, uint8_t volume);
void player_stop(void);
bool player_is_playing(void);
