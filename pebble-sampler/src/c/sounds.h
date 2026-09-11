#pragma once
#include <pebble.h>
#include "synth.h"

typedef enum {
  SoundKindSynth,   // procedurally rendered PCM stream
  SoundKindNotes,   // speaker_play_notes()
  SoundKindTracks,  // speaker_play_tracks() (polyphonic)
  SoundKindSample,  // raw PCM resource streamed from flash
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
  uint32_t resource_id;         // SoundKindSample only
  uint8_t pcm_format;           // SoundKindSample only: format handed to the speaker (SpeakerPcmFormat)
  uint8_t codec;                // SoundKindSample only: SampleCodec
} Sound;

typedef enum {
  SampleCodecRaw = 0,       // resource holds PCM exactly in pcm_format
  SampleCodecImaAdpcm = 1,  // resource holds IMA ADPCM (4 bit/sample), decoded to 16-bit
} SampleCodec;

extern const Sound SOUNDS[];
extern const uint8_t NUM_SOUNDS;
