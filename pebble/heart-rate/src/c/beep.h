/*
 * The per-beat beep, on watches that have a speaker. Does nothing on the others.
 *
 * Two ways of playing it, chosen in the settings:
 *
 *   BeepModeStream    keeps a PCM stream open for the whole session and writes silence between
 *                     the beats. The amplifier never powers down, so it cannot click on its way
 *                     out. This is the default.
 *   BeepModeSingle    hands the sample to the speaker once per beat and lets it close again.
 *                     Simpler and cheaper, but each close is a chance for the amplifier to click.
 */
#pragma once

#include <pebble.h>

#include "settings.h"

//! Prepare playback for the current settings. Safe to call again after they change.
void beep_setup(const Settings *settings);

//! Sound one beat.
void beep_play(void);

//! Keep the stream fed. Call regularly (every couple of frames) while the app runs.
void beep_pump(void);

//! Release the speaker.
void beep_teardown(void);
