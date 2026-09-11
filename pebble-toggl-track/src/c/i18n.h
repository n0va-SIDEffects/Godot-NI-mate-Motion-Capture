#pragma once

// Watch-side strings in the watch's system language (de, en, fr, it, es;
// English is the fallback). Use STR(id) wherever text reaches the screen.

typedef enum {
  S_APP_NAME,
  S_NO_PROJECT,
  S_NO_TIMER,
  S_CONNECTING,
  S_NOT_CONNECTED,
  S_NO_DESCRIPTION,
  S_SELECT_TO_START,
  S_SINCE,            // "seit %s"
  S_TODAY_H,          // "heute %s h"
  S_TODAY_BOOKED,     // "Heute %s h gebucht"
  S_REFRESHING,
  S_STOPPING,
  S_STARTING,
  S_PLEASE_WAIT,
  S_SEND_ERROR,
  S_NO_PHONE,
  S_STARTED,          // "Gestartet: %s"
  S_STOPPED,
  S_TIMER,
  S_PREVIOUS_ENTRY,
  S_RECENT,
  S_PROJECTS,
  S_PICK_PROJECT,
  S_START_EMPTY,
  S_DICTATE,
  S_LIST,
  S_FAVORITE,
  S_NO_FAVORITES,
  S_DICT_NO_CONNECTION,
  S_DICT_DISABLED,
  S_DICT_NOTHING,
  S_DICT_CANCELLED,
  S_DICT_UNAVAILABLE,
  S_NO_MICROPHONE,
  S_HINT_STILL_RUNNING,
  S_HINT_NO_TIMER,
  S_GLANCE_IDLE,
  S_COUNT
} StringId;

void i18n_init(void);
const char *STR(StringId id);
