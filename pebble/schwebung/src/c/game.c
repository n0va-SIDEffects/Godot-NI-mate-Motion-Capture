#include "game.h"
#include "config.h"
#include "input.h"
#include "synth.h"
#include "haptics.h"
#include "backlight.h"
#include "render.h"

#define LOCK_RING (LOCK_AVG_MS / GAME_TICK_MS)

static GameView s_v;
static uint32_t s_lcg;
static int32_t s_ring[LOCK_RING];
static uint32_t s_ring_n;
static uint32_t s_ring_head;
static uint32_t s_t_lock;
static uint32_t s_t_event;
static uint32_t s_vib_phase;
static const char *s_state_names[] = { "SUCHEN", "EINGERASTET", "LOSGELASSEN", "ZERSPRUNGEN" };
static const char *s_note_names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H" };
// Centi-Hertz der Oktave 4 (MIDI 60..71)
static const int32_t s_oct4_chz[12] = { 26163, 27718, 29366, 31113, 32963, 34923,
                                        36999, 39200, 41530, 44000, 46616, 49388 };

static uint32_t prv_rand(void) {
  s_lcg = s_lcg * 1103515245u + 12345u;
  return (s_lcg >> 8) & 0xFFFFFF;
}

int32_t game_midi_to_chz(int midi) {
  int oct = midi / 12 - 5;
  int32_t base = s_oct4_chz[midi % 12];
  if (oct >= 0) {
    return base << oct;
  }
  return base >> (-oct);
}

static int prv_chz_to_midi(int32_t chz) {
  int best = 60;
  int32_t best_d = 0x7FFFFFFF;
  for (int m = 48; m <= 100; m++) {
    int32_t d = game_midi_to_chz(m) - chz;
    if (d < 0) d = -d;
    if (d < best_d) {
      best_d = d;
      best = m;
    }
  }
  return best;
}

void game_note_name(int32_t chz, char *buf, size_t len) {
  int m = prv_chz_to_midi(chz);
  snprintf(buf, len, "%s%d", s_note_names[m % 12], m / 12 - 1);
}

void game_respawn_flower(void) {
  int fork_midi = prv_chz_to_midi(input_fork_chz());
  int midi = FLOWER_MIDI_MIN;
  for (int tries = 0; tries < 16; tries++) {
    midi = FLOWER_MIDI_MIN + (int)(prv_rand() % (FLOWER_MIDI_MAX - FLOWER_MIDI_MIN + 1));
    int d = midi - fork_midi;
    if (d < 0) d = -d;
    if (d >= FLOWER_MIN_SEMITONES) {
      break;
    }
  }
  s_v.flower_midi = (uint8_t)midi;
  s_v.flower_chz = game_midi_to_chz(midi);
  s_v.state = GameSearching;
  s_v.hold_ms = 0;
  s_v.hold_pct = 0;
  s_ring_n = 0;
  s_ring_head = 0;
  char name[8];
  game_note_name(s_v.flower_chz, name, sizeof(name));
  APP_LOG(APP_LOG_LEVEL_INFO, "[E1][GAME] Neue Blume: MIDI %d = %s = %ld.%02ld Hz", midi, name,
          (long)(s_v.flower_chz / 100), (long)(s_v.flower_chz % 100));
}

void game_init(void) {
  s_v = (GameView){ 0 };
  s_lcg = (uint32_t)time(NULL) ^ 0x5EED1234u;
  s_vib_phase = 0;
  game_respawn_flower();
}

static void prv_enter(GameState st, uint32_t now) {
  s_v.state = st;
  s_t_event = now;
}

void game_tick(uint32_t now, uint32_t dt) {
  const int32_t fork = input_fork_chz();
  const bool finger = input_finger_down();
  int32_t df = fork - s_v.flower_chz;
  if (df < 0) df = -df;
  const bool in_window = df <= WINDOW_CHZ;
  const bool near = df <= CLEAR_CHZ;
  const uint8_t weight = in_window ? (uint8_t)(((WINDOW_CHZ - df) * 100) / WINDOW_CHZ) : 0;

  s_v.fork_chz = fork;
  s_v.finger = finger;
  s_v.in_window = in_window;
  s_v.near = near;
  s_v.beat_chz = df;

  synth_set_fork(fork, finger);
  synth_set_flower(s_v.flower_chz, weight);

  switch (s_v.state) {
    case GameSearching: {
      if (finger && in_window) {
        s_ring[s_ring_head] = df;
        s_ring_head = (s_ring_head + 1) % LOCK_RING;
        if (s_ring_n < LOCK_RING) s_ring_n++;
        if (s_ring_n >= LOCK_RING) {
          int64_t sum = 0;
          for (uint32_t i = 0; i < LOCK_RING; i++) sum += s_ring[i];
          if (sum / LOCK_RING < LOCK_CHZ) {
            prv_enter(GameLocked, now);
            s_t_lock = now;
            s_v.locks++;
            s_v.hold_ms = 0;
            haptics_lock();
            synth_ping(s_v.flower_chz);
            APP_LOG(APP_LOG_LEVEL_INFO, "[E1][GAME] Eingerastet bei %ld.%02ld Hz (Blume %ld.%02ld Hz)",
                    (long)(fork / 100), (long)(fork % 100), (long)(s_v.flower_chz / 100),
                    (long)(s_v.flower_chz % 100));
          }
        }
      } else {
        s_ring_n = 0;
        s_ring_head = 0;
      }
      haptics_set_beat(df, finger && in_window);
      break;
    }
    case GameLocked: {
      if (!finger) {
        prv_enter(GameReleased, now);
        s_v.releases++;
        haptics_set_beat(0, false);
        APP_LOG(APP_LOG_LEVEL_INFO, "[E1][GAME] Losgelassen nach %lu ms Halten", (unsigned long)s_v.hold_ms);
      } else if (df > CLEAR_CHZ) {
        prv_enter(GameSearching, now);
        s_ring_n = 0;
        APP_LOG(APP_LOG_LEVEL_INFO, "[E1][GAME] Entglitten");
      } else {
        uint32_t el = now - s_t_lock;
        if (el > KARENZ_MS) {
          s_v.hold_ms = el - KARENZ_MS;
          if (s_v.hold_ms >= BREAK_MS) {
            prv_enter(GameBroken, now);
            s_v.breaks++;
            synth_crack();
            haptics_break();
            APP_LOG(APP_LOG_LEVEL_INFO, "[E1][GAME] Zersprungen");
          } else if (s_v.hold_ms >= BREAK_MS - WARN_MS) {
            haptics_warn(BREAK_MS - s_v.hold_ms);
          } else {
            haptics_set_beat(0, false);
          }
        } else {
          haptics_set_beat(0, false);
        }
      }
      break;
    }
    case GameReleased:
      haptics_set_beat(0, false);
      if (now - s_t_event > 1500) {
        game_respawn_flower();
      }
      break;
    case GameBroken:
      haptics_set_beat(0, false);
      if (now - s_t_event > RESPAWN_MS) {
        game_respawn_flower();
      }
      break;
  }
  s_v.hold_pct = (uint8_t)(s_v.hold_ms * 100 / BREAK_MS > 100 ? 100 : s_v.hold_ms * 100 / BREAK_MS);

  backlight_set_pitch(fork, s_v.flower_chz);
  backlight_set_state(df, in_window, near, s_v.state == GameLocked);

  // Sichtbarkeit der Blume
  FlowerVis vis = { .visible = s_v.state != GameBroken, .vib_amp16 = 0, .vib_phase = 0, .glow16 = 0 };
  if (s_v.state == GameSearching && in_window && finger) {
    vis.vib_amp16 = ((WINDOW_CHZ - df) * 32) / WINDOW_CHZ;
    int32_t rate = df > BL_BREATH_MAX_CHZ ? BL_BREATH_MAX_CHZ : df;
    if (rate < LOCK_CHZ) rate = LOCK_CHZ;
    s_vib_phase += (uint32_t)(((uint64_t)(uint32_t)rate * dt * TRIG_MAX_ANGLE) / 100000u);
    vis.vib_phase = s_vib_phase;
    if (near) {
      uint32_t angle = (uint32_t)(((uint64_t)(uint32_t)rate * now * TRIG_MAX_ANGLE) / 200000u);
      int32_t c = cos_lookup((int32_t)(angle & (TRIG_MAX_ANGLE - 1)));
      if (c < 0) c = -c;
      vis.glow16 = (uint8_t)(4 + (8 * c) / TRIG_MAX_RATIO);
    }
  } else if (s_v.state == GameLocked) {
    vis.glow16 = (uint8_t)(12 + (4 * s_v.hold_pct) / 100);
  } else if (s_v.state == GameReleased) {
    vis.glow16 = 8;
  }
  render_set_flower(&vis);
}

const GameView *game_view(void) {
  return &s_v;
}

const char *game_state_name(void) {
  return s_state_names[s_v.state];
}
