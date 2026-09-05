#include "model.h"

#define PERSIST_KEY_STATUS 1

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
  }
}

void model_save(void) {
  persist_write_data(PERSIST_KEY_STATUS, &s_model.status, sizeof(TimerStatus));
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
