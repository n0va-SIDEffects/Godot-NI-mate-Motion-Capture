#pragma once
#include <pebble.h>

// Einstellungen als richtiges Menue statt versteckter Klickfolgen. Up und Down
// blaettern, Select aendert den Wert oder loest die Aktion aus, Back schliesst.
// Gespeichert wird erst beim Schliessen: persist_write haelt die App einige
// Millisekunden an, und genau dann fehlt dem Lautsprecher Nachschub.

typedef enum {
  ScreenSpiel = 0,
  ScreenMagnet,
  ScreenPanel,
  ScreenMess,
  ScreenCount,
} ScreenId;

typedef enum {
  AudioOff = 0,     // Stream geschlossen: kein Verstaerkerrauschen
  AudioSilence,     // Stream laeuft mit voller Rate, schreibt Nullen
  AudioTone,        // hoerbarer Dauerton
  AudioModeCount,
} AudioMode;

typedef struct {
  uint8_t screen;
  uint8_t rot90;
  uint8_t camera;
  uint8_t speed_idx;
  uint8_t thumb_mode;
  uint8_t audio_mode;
  uint8_t quiet_log;
  uint8_t show_tip;      // Umriss der Fingerkuppe zeichnen
} Settings;

typedef struct {
  void (*apply)(const Settings *s);   // nach jeder Aenderung
  void (*action)(uint8_t which);      // 0 Physik-Test, 1 Panel-Test, 2 Zaehler zurueck
} SettingsHooks;

void settings_init(const SettingsHooks *hooks);
void settings_deinit(void);
void settings_show(void);
bool settings_is_open(void);
Settings *settings_get(void);
void settings_save(void);
