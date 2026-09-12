#pragma once
/*
 * User samples loaded from the phone at runtime (see src/pkjs/index.js).
 * They live in RAM only: the watch has just 4 KB of persistent storage.
 */
#include <pebble.h>

#define PHONE_MAX_SLOTS       4
#define PHONE_MAX_SAMPLE_BYTES 24000   // 3 s of 16 kHz IMA ADPCM; must match index.js
#define PHONE_NAME_LEN        21

typedef enum {
  PhoneSlotEmpty,      // not configured
  PhoneSlotLoading,    // chunks arriving
  PhoneSlotReady,      // complete, playable
  PhoneSlotError,      // download or transfer failed
} PhoneSlotState;

typedef struct {
  PhoneSlotState state;
  char name[PHONE_NAME_LEN];
  uint8_t *data;
  uint32_t total;
  uint32_t received;
} PhoneSlot;

typedef struct {
  int volume;      // -1 when not present
  int shake;       // -1 when not present
  int touch;       // -1 when not present
} PhoneSettings;

// Called on the app task whenever a slot changes (progress, ready, error).
typedef void (*PhoneSlotChangedCb)(int slot);
// Called when the phone sends settings.
typedef void (*PhoneSettingsCb)(const PhoneSettings *settings);

void phone_init(PhoneSlotChangedCb slot_cb, PhoneSettingsCb settings_cb);
void phone_deinit(void);

const PhoneSlot *phone_slot(int slot);
// Number of slots that are configured (loading, ready or failed), counting from the top.
int phone_active_slots(void);
