#pragma once
#include <pebble.h>
#include "synth.h"

typedef enum {
  SoundKindSynth,   // procedurally rendered PCM stream
  SoundKindNotes,   // speaker_play_notes()
  SoundKindTracks,  // speaker_play_tracks() (polyphonic)
} SoundKind;

typedef struct {
  const char *name;
  const char *hint;
  uint8_t argb;                 // GColor .argb value used for the menu accent
  SoundKind kind;
  uint16_t duration_ms;         // SoundKindSynth only
  SynthRenderFn render;         // SoundKindSynth only
  const SpeakerNote *notes;     // SoundKindNotes only
  const SpeakerTrack *tracks;   // SoundKindTracks only
  uint8_t count;                // number of notes / tracks
} Sound;

extern const Sound SOUNDS[];
extern const uint8_t NUM_SOUNDS;
