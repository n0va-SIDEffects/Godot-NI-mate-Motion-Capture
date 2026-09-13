#pragma once
#include <pebble.h>

// Spiellogik der Etappe 1: eine Blume, Suchen, Einrasten, Karenz, Halten,
// Bruch, Neustart. Verbindet Eingabe, Synth, Haptik, Backlight und Renderer.
typedef enum {
  GameSearching = 0,
  GameLocked,
  GameReleased,
  GameBroken,
} GameState;

typedef struct {
  int32_t fork_chz;
  int32_t flower_chz;
  int32_t beat_chz;
  bool finger;
  bool in_window;
  bool near;
  GameState state;
  uint32_t hold_ms;
  uint8_t hold_pct;
  uint32_t locks;
  uint32_t breaks;
  uint32_t releases;
  uint8_t flower_midi;
} GameView;

void game_init(void);
void game_tick(uint32_t now_ms, uint32_t dt_ms);
void game_respawn_flower(void);
const GameView *game_view(void);
const char *game_state_name(void);
int32_t game_midi_to_chz(int midi);
void game_note_name(int32_t chz, char *buf, size_t len);
