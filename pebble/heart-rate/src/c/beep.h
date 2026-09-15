/*
 * The per-beat beep, on watches that have a speaker. Does nothing on the others.
 *
 * The sample is handed to the speaker once per beat. Holding a PCM stream open across the beats
 * was tried instead, to stop the amplifier powering down between them, and was worse on both
 * counts: the amplifier hisses for as long as a stream is open, and this app redraws often enough
 * to starve the stream, which stutters. A short beep per beat is what the speaker is good at.
 */
#pragma once

#include <pebble.h>

#include "settings.h"

//! Prepare playback for the current settings. Safe to call again after they change.
void beep_setup(const Settings *settings);

//! Sound one beat.
void beep_play(void);

//! Release the speaker.
void beep_teardown(void);
