/*
 * The per-beat beep, on watches that have a speaker. Does nothing on the others.
 *
 * Two ways of playing it, chosen in the settings:
 *
 *   BeepModeSingle    hands the sample to the speaker once per beat and lets it close again.
 *                     The default, and what the watch has always done. Each close is a chance for
 *                     the amplifier to click on its way out.
 *   BeepModeStream    keeps a PCM stream open for the whole session and writes silence between
 *                     the beats, so the amplifier never powers down. Untried on real hardware:
 *                     none of this can be heard from a build machine.
 */
#pragma once

#include <pebble.h>

#include "settings.h"

//! Prepare playback for the current settings. Safe to call again after they change.
void beep_setup(const Settings *settings);

//! Sound one beat.
void beep_play(void);

//! Keep the stream fed. Driven by its own timer; exposed so a beat can top it up straight away.
void beep_pump(void);

//! Release the speaker.
void beep_teardown(void);
