#pragma once

#include <pebble.h>

// Shared application state: the current timer, the cached project list, the
// recently used entries, favourites and the reminder configuration. Everything
// is filled by the phone-side JavaScript through AppMessage (see comm.c).

#define MAX_PROJECTS  24
#define MAX_RECENT    12
#define MAX_FAVORITES 4
#define NAME_LEN      32
#define DESC_LEN      64
#define CLIENT_LEN    48
#define MESSAGE_LEN   64
#define HINT_LEN      48

// Reminder flag bits (REMIND_FLAGS), shared with src/pkjs/index.js
#define REMIND_RUNNING  1   // "still running?" after N hours / late in the evening
#define REMIND_NO_TIMER 2   // "nothing running" on weekday mornings

typedef struct {
  int32_t id;
  char name[NAME_LEN];
  uint8_t color;            // GColor8 .argb value, 0 = no colour known
} Project;

typedef struct {
  int32_t project_id;       // 0 = no project
  char description[DESC_LEN];
  char project_name[NAME_LEN];
  uint8_t color;
  int32_t today_seconds;    // time booked on this entry today
} RecentEntry;

typedef RecentEntry Favorite;

typedef struct {
  bool valid;               // false until the phone sent a status at least once
  bool running;
  time_t start_time;        // UTC unix seconds of the running entry
  char description[DESC_LEN];
  char project_name[NAME_LEN];
  uint8_t color;
  int32_t today_seconds;    // total booked today (running entry included)
  char client_line[CLIENT_LEN];   // "Kunde · tag1, tag2"
} TimerStatus;

typedef struct {
  uint8_t flags;            // REMIND_* bits
  uint8_t max_hours;        // running reminder after this many hours
  uint8_t late_hour;        // ... or at this local hour, whichever is first
  uint8_t start_hour;       // no-timer reminder at this hour on weekdays
} ReminderConfig;

typedef enum {
  HINT_NONE = 0,
  HINT_STILL_RUNNING,
  HINT_NO_TIMER,
} HintKind;

typedef enum {
  PENDING_NONE = 0,
  PENDING_START,
  PENDING_STOP,
} PendingAction;

typedef struct {
  TimerStatus status;
  Project projects[MAX_PROJECTS];
  uint8_t project_count;
  RecentEntry recent[MAX_RECENT];
  uint8_t recent_count;
  Favorite favorites[MAX_FAVORITES];
  uint8_t favorite_count;
  ReminderConfig config;
  char message[MESSAGE_LEN]; // footer text (progress or error), "" = none
  bool message_is_error;
  char hint[HINT_LEN];       // sticky footer text (reminders), cleared by a button
  HintKind hint_kind;
  PendingAction pending;     // action whose confirmation is still outstanding
  bool stop_on_connect;      // launched from the timeline pin's "Stop" action
} AppModel;

AppModel *model_get(void);
void model_load(void);    // restore status + config from persistent storage
void model_save(void);    // persist the current status + config
void model_set_message(const char *text, bool is_error);
void model_clear_message(void);
void model_set_hint(const char *text, HintKind kind);
void model_clear_hint(void);
