/*
 * The per-beat beep, on watches that have a speaker. Does nothing on the others.
 *
 * Two ways of getting the beep out, chosen in the settings:
 *
 *   BeepModeSingle  hands the sample to the speaker once per beat and lets it close the session
 *                   again. Simple, silent between beats, but each close is a chance for the
 *                   amplifier to click on its way out, which is what the watch does occasionally.
 *   BeepModeStream  keeps a PCM stream open while a pulse is being followed, writing silence
 *                   between the beats, so the amplifier never switches off mid-pulse. The stream
 *                   is fed by the pump from the pebble-audio skill, which was measured on real
 *                   hardware. The cost is a faint hiss while the stream is open, and the beep
 *                   lands about a tenth of a second behind its spike, because that is how far the
 *                   stream runs ahead of the speaker.
 */
#pragma once

#include <pebble.h>

#include "settings.h"

//! Point playback at the app's settings. They are read at each beat, so nothing has to be told
//! about a change: an earlier version kept its own copy, and the button that switches the sound
//! never reached it.
void beep_setup(const Settings *settings);

//! Sound one beat.
void beep_play(void);

//! Housekeeping; call a few times a second. Closes the stream once the pulse has been gone a while.
void beep_tick(void);

//! How often the stream ran dry. Stays at zero when the audio chain keeps up.
uint16_t beep_underruns(void);

//! Release the speaker.
void beep_teardown(void);
