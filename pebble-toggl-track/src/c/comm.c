#include "comm.h"
#include "model.h"
#include "status_window.h"
#include "list_window.h"

// Protocol (values of MESSAGE_KEY_CMD). Keep in sync with src/pkjs/index.js.
enum {
  // watch -> phone
  CMD_REFRESH = 1,
  CMD_START   = 2,   // + PROJECT_ID, DESCRIPTION
  CMD_STOP    = 3,
  // phone -> watch
  CMD_STATUS  = 10,  // + RUNNING, DESCRIPTION, PROJECT_NAME, PROJECT_COLOR, START_TIME
  CMD_PROJECT = 11,  // + INDEX, COUNT, PROJECT_ID, PROJECT_NAME, PROJECT_COLOR
  CMD_RECENT  = 12,  // + INDEX, COUNT, PROJECT_ID, DESCRIPTION, PROJECT_NAME, PROJECT_COLOR
  CMD_ERROR   = 13,  // + MESSAGE
  CMD_INFO    = 14,  // + MESSAGE
};

#define INBOX_SIZE  1024
#define OUTBOX_SIZE 256

static void prv_copy_string(char *dst, size_t dst_len, DictionaryIterator *iter, uint32_t key) {
  Tuple *t = dict_find(iter, key);
  if (t && t->type == TUPLE_CSTRING) {
    strncpy(dst, t->value->cstring, dst_len - 1);
    dst[dst_len - 1] = '\0';
  } else {
    dst[0] = '\0';
  }
}

static int32_t prv_get_int(DictionaryIterator *iter, uint32_t key, int32_t fallback) {
  Tuple *t = dict_find(iter, key);
  if (!t) {
    return fallback;
  }
  if (t->type == TUPLE_INT) {
    switch (t->length) {
      case 1: return t->value->int8;
      case 2: return t->value->int16;
      default: return t->value->int32;
    }
  }
  if (t->type == TUPLE_UINT) {
    switch (t->length) {
      case 1: return t->value->uint8;
      case 2: return t->value->uint16;
      default: return (int32_t)t->value->uint32;
    }
  }
  return fallback;
}

static void prv_handle_status(DictionaryIterator *iter) {
  TimerStatus *s = &model_get()->status;
  s->running = prv_get_int(iter, MESSAGE_KEY_RUNNING, 0) != 0;
  s->start_time = (time_t)prv_get_int(iter, MESSAGE_KEY_START_TIME, 0);
  prv_copy_string(s->description, DESC_LEN, iter, MESSAGE_KEY_DESCRIPTION);
  prv_copy_string(s->project_name, NAME_LEN, iter, MESSAGE_KEY_PROJECT_NAME);
  s->color = (uint8_t)prv_get_int(iter, MESSAGE_KEY_PROJECT_COLOR, 0);
  s->valid = true;
  model_clear_message();
  model_save();
  status_window_refresh();
}

static void prv_handle_project(DictionaryIterator *iter) {
  AppModel *m = model_get();
  int32_t index = prv_get_int(iter, MESSAGE_KEY_INDEX, 0);
  int32_t count = prv_get_int(iter, MESSAGE_KEY_COUNT, 0);
  if (index == 0) {
    m->project_count = 0;   // a new list starts
  }
  if (count > 0 && index >= 0 && index < MAX_PROJECTS) {
    Project *p = &m->projects[index];
    p->id = prv_get_int(iter, MESSAGE_KEY_PROJECT_ID, 0);
    prv_copy_string(p->name, NAME_LEN, iter, MESSAGE_KEY_PROJECT_NAME);
    p->color = (uint8_t)prv_get_int(iter, MESSAGE_KEY_PROJECT_COLOR, 0);
    if (index + 1 > m->project_count) {
      m->project_count = index + 1;
    }
  }
  list_window_refresh();
}

static void prv_handle_recent(DictionaryIterator *iter) {
  AppModel *m = model_get();
  int32_t index = prv_get_int(iter, MESSAGE_KEY_INDEX, 0);
  int32_t count = prv_get_int(iter, MESSAGE_KEY_COUNT, 0);
  if (index == 0) {
    m->recent_count = 0;
  }
  if (count > 0 && index >= 0 && index < MAX_RECENT) {
    RecentEntry *e = &m->recent[index];
    e->project_id = prv_get_int(iter, MESSAGE_KEY_PROJECT_ID, 0);
    prv_copy_string(e->description, DESC_LEN, iter, MESSAGE_KEY_DESCRIPTION);
    prv_copy_string(e->project_name, NAME_LEN, iter, MESSAGE_KEY_PROJECT_NAME);
    e->color = (uint8_t)prv_get_int(iter, MESSAGE_KEY_PROJECT_COLOR, 0);
    if (index + 1 > m->recent_count) {
      m->recent_count = index + 1;
    }
  }
  list_window_refresh();
}

static void prv_handle_message(DictionaryIterator *iter, bool is_error) {
  AppModel *m = model_get();
  prv_copy_string(m->message, MESSAGE_LEN, iter, MESSAGE_KEY_MESSAGE);
  m->message_is_error = is_error;
  if (is_error) {
    vibes_short_pulse();
  }
  status_window_refresh();
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  int32_t cmd = prv_get_int(iter, MESSAGE_KEY_CMD, 0);
  switch (cmd) {
    case CMD_STATUS:  prv_handle_status(iter); break;
    case CMD_PROJECT: prv_handle_project(iter); break;
    case CMD_RECENT:  prv_handle_recent(iter); break;
    case CMD_ERROR:   prv_handle_message(iter, true); break;
    case CMD_INFO:    prv_handle_message(iter, false); break;
    default:
      APP_LOG(APP_LOG_LEVEL_WARNING, "Unknown command %d", (int)cmd);
      break;
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
  model_set_message("Keine Verbindung zum Handy", true);
  status_window_refresh();
}

static void prv_send(int32_t cmd, int32_t project_id, const char *description) {
  DictionaryIterator *iter = NULL;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_BUSY) {
    model_set_message("Bitte warten…", false);
    status_window_refresh();
    return;
  }
  if (result != APP_MSG_OK || !iter) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox begin failed: %d", (int)result);
    model_set_message("Sendefehler", true);
    status_window_refresh();
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_CMD, cmd);
  if (cmd == CMD_START) {
    dict_write_int32(iter, MESSAGE_KEY_PROJECT_ID, project_id);
    dict_write_cstring(iter, MESSAGE_KEY_DESCRIPTION, description ? description : "");
  }
  dict_write_end(iter);
  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", (int)result);
    model_set_message("Sendefehler", true);
    status_window_refresh();
  }
}

void comm_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_open(INBOX_SIZE, OUTBOX_SIZE);
}

void comm_send_refresh(void) {
  prv_send(CMD_REFRESH, 0, NULL);
}

void comm_send_start(int32_t project_id, const char *description) {
  prv_send(CMD_START, project_id, description);
}

void comm_send_stop(void) {
  prv_send(CMD_STOP, 0, NULL);
}
