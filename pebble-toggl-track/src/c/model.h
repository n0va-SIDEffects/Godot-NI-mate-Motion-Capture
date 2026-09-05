#pragma once

#include <pebble.h>

// Shared application state: the current timer, the cached project list and
// the list of recently used entries. Everything is filled by the phone-side
// JavaScript through AppMessage (see comm.c).

#define MAX_PROJECTS 24
#define MAX_RECENT   12
#define NAME_LEN     32
#define DESC_LEN     64
#define MESSAGE_LEN  64

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
} RecentEntry;

typedef struct {
  bool valid;               // false until the phone sent a status at least once
  bool running;
  time_t start_time;        // UTC unix seconds of the running entry
  char description[DESC_LEN];
  char project_name[NAME_LEN];
  uint8_t color;
} TimerStatus;

typedef struct {
  TimerStatus status;
  Project projects[MAX_PROJECTS];
  uint8_t project_count;
  RecentEntry recent[MAX_RECENT];
  uint8_t recent_count;
  char message[MESSAGE_LEN]; // footer text (progress or error), "" = none
  bool message_is_error;
} AppModel;

AppModel *model_get(void);
void model_load(void);    // restore the last known status from persistent storage
void model_save(void);    // persist the current status
void model_set_message(const char *text, bool is_error);
void model_clear_message(void);
