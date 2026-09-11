#include "model.h"

// Key 1 held the v1.0 status layout; v1.1 grew the struct, so use fresh keys.
#define PERSIST_KEY_STATUS 2
#define PERSIST_KEY_CONFIG 3

_Static_assert(sizeof(TimerStatus) <= PERSIST_DATA_MAX_LENGTH,
               "TimerStatus must fit into one persist slot");

static AppModel s_model;

AppModel *model_get(void) {
  return &s_model;
}

void model_load(void) {
  memset(&s_model, 0, sizeof(s_model));
  if (persist_exists(PERSIST_KEY_STATUS)) {
    int read = persist_read_data(PERSIST_KEY_STATUS, &s_model.status, sizeof(TimerStatus));
    if (read != (int)sizeof(TimerStatus)) {
      memset(&s_model.status, 0, sizeof(TimerStatus));
    }
    s_model.status.description[DESC_LEN - 1] = '\0';
    s_model.status.project_name[NAME_LEN - 1] = '\0';
    s_model.status.client_line[CLIENT_LEN - 1] = '\0';
  }
  // Defaults match src/pkjs/config.js until the phone sends its settings.
  s_model.config = (ReminderConfig) { .flags = REMIND_RUNNING, .max_hours = 4, .late_hour = 22, .start_hour = 9 };
  if (persist_exists(PERSIST_KEY_CONFIG)) {
    ReminderConfig c;
    if (persist_read_data(PERSIST_KEY_CONFIG, &c, sizeof(c)) == (int)sizeof(c)) {
      s_model.config = c;
    }
  }
}

void model_save(void) {
  persist_write_data(PERSIST_KEY_STATUS, &s_model.status, sizeof(TimerStatus));
  persist_write_data(PERSIST_KEY_CONFIG, &s_model.config, sizeof(ReminderConfig));
}

void model_set_message(const char *text, bool is_error) {
  strncpy(s_model.message, text ? text : "", MESSAGE_LEN - 1);
  s_model.message[MESSAGE_LEN - 1] = '\0';
  s_model.message_is_error = is_error;
}

void model_clear_message(void) {
  s_model.message[0] = '\0';
  s_model.message_is_error = false;
}

void model_set_hint(const char *text, HintKind kind) {
  strncpy(s_model.hint, text ? text : "", HINT_LEN - 1);
  s_model.hint[HINT_LEN - 1] = '\0';
  s_model.hint_kind = kind;
}

void model_clear_hint(void) {
  s_model.hint[0] = '\0';
  s_model.hint_kind = HINT_NONE;
}
