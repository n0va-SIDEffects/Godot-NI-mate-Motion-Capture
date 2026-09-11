#include "comm.h"
#include "model.h"
#include "status_window.h"
#include "list_window.h"
#include "favorites_window.h"
#include "i18n.h"

// Protocol (values of MESSAGE_KEY_CMD). Keep in sync with src/pkjs/index.js.
enum {
  // watch -> phone
  CMD_REFRESH  = 1,
  CMD_START    = 2,   // + PROJECT_ID, DESCRIPTION
  CMD_STOP     = 3,
  CMD_PREVIOUS = 4,   // stop the running entry, restart the one before it
  // phone -> watch
  CMD_STATUS   = 10,  // + RUNNING, DESCRIPTION, PROJECT_NAME, PROJECT_COLOR, START_TIME, TODAY_SECONDS, CLIENT_NAME
  CMD_PROJECT  = 11,  // + INDEX, COUNT, PROJECT_ID, PROJECT_NAME, PROJECT_COLOR
  CMD_RECENT   = 12,  // + INDEX, COUNT, PROJECT_ID, DESCRIPTION, PROJECT_NAME, PROJECT_COLOR, TODAY_SECONDS
  CMD_ERROR    = 13,  // + MESSAGE
  CMD_INFO     = 14,  // + MESSAGE
  CMD_FAVORITE = 15,  // + INDEX, COUNT, PROJECT_ID, DESCRIPTION, PROJECT_NAME, PROJECT_COLOR
  CMD_CONFIG   = 16,  // + REMIND_FLAGS, REMIND_MAX_HOURS, REMIND_LATE_HOUR, REMIND_START_HOUR
};

#define INBOX_SIZE  1024
#define OUTBOX_SIZE 256
#define CONFIRM_MS  2500

static AppTimer *s_confirm_timer;

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

static void prv_confirm_timeout(void *context) {
  s_confirm_timer = NULL;
  model_clear_message();
  status_window_refresh();
}

// Short vibe + "Gestartet: …" / "Gestoppt" once the phone confirmed our action.
static void prv_confirm_action(const TimerStatus *s) {
  AppModel *m = model_get();
  char text[MESSAGE_LEN + DESC_LEN];   // model_set_message() clips to MESSAGE_LEN
  if (m->pending == PENDING_START && s->running) {
    snprintf(text, sizeof(text), STR(S_STARTED),
             s->description[0] ? s->description : (s->project_name[0] ? s->project_name : STR(S_TIMER)));
  } else if (m->pending == PENDING_STOP && !s->running) {
    snprintf(text, sizeof(text), "%s", STR(S_STOPPED));
  } else {
    return;   // status does not match the action yet; wait for the next one
  }
  m->pending = PENDING_NONE;
  vibes_short_pulse();
  model_set_message(text, false);
  if (s_confirm_timer) {
    app_timer_reschedule(s_confirm_timer, CONFIRM_MS);
  } else {
    s_confirm_timer = app_timer_register(CONFIRM_MS, prv_confirm_timeout, NULL);
  }
}

static void prv_handle_status(DictionaryIterator *iter) {
  AppModel *m = model_get();
  TimerStatus *s = &m->status;
  s->running = prv_get_int(iter, MESSAGE_KEY_RUNNING, 0) != 0;
  s->start_time = (time_t)prv_get_int(iter, MESSAGE_KEY_START_TIME, 0);
  prv_copy_string(s->description, DESC_LEN, iter, MESSAGE_KEY_DESCRIPTION);
  prv_copy_string(s->project_name, NAME_LEN, iter, MESSAGE_KEY_PROJECT_NAME);
  prv_copy_string(s->client_line, CLIENT_LEN, iter, MESSAGE_KEY_CLIENT_NAME);
  s->color = (uint8_t)prv_get_int(iter, MESSAGE_KEY_PROJECT_COLOR, 0);
  s->today_seconds = prv_get_int(iter, MESSAGE_KEY_TODAY_SECONDS, 0);
  s->valid = true;
  model_clear_message();
  // A reminder hint is obsolete once reality moved on.
  if ((s->running && m->hint_kind == HINT_NO_TIMER) || (!s->running && m->hint_kind == HINT_STILL_RUNNING)) {
    model_clear_hint();
  }
  prv_confirm_action(s);
  model_save();
  status_window_refresh();
}

static void prv_read_entry(DictionaryIterator *iter, RecentEntry *e) {
  e->project_id = prv_get_int(iter, MESSAGE_KEY_PROJECT_ID, 0);
  prv_copy_string(e->description, DESC_LEN, iter, MESSAGE_KEY_DESCRIPTION);
  prv_copy_string(e->project_name, NAME_LEN, iter, MESSAGE_KEY_PROJECT_NAME);
  e->color = (uint8_t)prv_get_int(iter, MESSAGE_KEY_PROJECT_COLOR, 0);
  e->today_seconds = prv_get_int(iter, MESSAGE_KEY_TODAY_SECONDS, 0);
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
    prv_read_entry(iter, &m->recent[index]);
    if (index + 1 > m->recent_count) {
      m->recent_count = index + 1;
    }
  }
  list_window_refresh();
}

static void prv_handle_favorite(DictionaryIterator *iter) {
  AppModel *m = model_get();
  int32_t index = prv_get_int(iter, MESSAGE_KEY_INDEX, 0);
  int32_t count = prv_get_int(iter, MESSAGE_KEY_COUNT, 0);
  if (index == 0) {
    m->favorite_count = 0;
  }
  if (count > 0 && index >= 0 && index < MAX_FAVORITES) {
    prv_read_entry(iter, &m->favorites[index]);
    if (index + 1 > m->favorite_count) {
      m->favorite_count = index + 1;
    }
  }
  favorites_window_refresh();
}

static void prv_handle_config(DictionaryIterator *iter) {
  AppModel *m = model_get();
  m->config.flags = (uint8_t)prv_get_int(iter, MESSAGE_KEY_REMIND_FLAGS, m->config.flags);
  m->config.max_hours = (uint8_t)prv_get_int(iter, MESSAGE_KEY_REMIND_MAX_HOURS, m->config.max_hours);
  m->config.late_hour = (uint8_t)prv_get_int(iter, MESSAGE_KEY_REMIND_LATE_HOUR, m->config.late_hour);
  m->config.start_hour = (uint8_t)prv_get_int(iter, MESSAGE_KEY_REMIND_START_HOUR, m->config.start_hour);
  model_save();
}

static void prv_handle_message(DictionaryIterator *iter, bool is_error) {
  AppModel *m = model_get();
  prv_copy_string(m->message, MESSAGE_LEN, iter, MESSAGE_KEY_MESSAGE);
  m->message_is_error = is_error;
  if (is_error) {
    m->pending = PENDING_NONE;
    vibes_short_pulse();
  }
  status_window_refresh();
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  int32_t cmd = prv_get_int(iter, MESSAGE_KEY_CMD, 0);
  switch (cmd) {
    case CMD_STATUS:   prv_handle_status(iter); break;
    case CMD_PROJECT:  prv_handle_project(iter); break;
    case CMD_RECENT:   prv_handle_recent(iter); break;
    case CMD_FAVORITE: prv_handle_favorite(iter); break;
    case CMD_CONFIG:   prv_handle_config(iter); break;
    case CMD_ERROR:    prv_handle_message(iter, true); break;
    case CMD_INFO:     prv_handle_message(iter, false); break;
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
  model_get()->pending = PENDING_NONE;
  model_set_message(STR(S_NO_PHONE), true);
  status_window_refresh();
}

static void prv_send(int32_t cmd, int32_t project_id, const char *description) {
  DictionaryIterator *iter = NULL;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_BUSY) {
    model_set_message(STR(S_PLEASE_WAIT), false);
    status_window_refresh();
    return;
  }
  if (result != APP_MSG_OK || !iter) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox begin failed: %d", (int)result);
    model_set_message(STR(S_SEND_ERROR), true);
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
    model_set_message(STR(S_SEND_ERROR), true);
    status_window_refresh();
  }
}

void comm_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_open(INBOX_SIZE, OUTBOX_SIZE);
}

void comm_deinit(void) {
  if (s_confirm_timer) {
    app_timer_cancel(s_confirm_timer);
    s_confirm_timer = NULL;
  }
}

void comm_send_refresh(void) {
  prv_send(CMD_REFRESH, 0, NULL);
}

void comm_send_start(int32_t project_id, const char *description) {
  model_get()->pending = PENDING_START;
  prv_send(CMD_START, project_id, description);
}

void comm_send_stop(void) {
  model_get()->pending = PENDING_STOP;
  prv_send(CMD_STOP, 0, NULL);
}

void comm_send_previous(void) {
  model_get()->pending = PENDING_START;
  prv_send(CMD_PREVIOUS, 0, NULL);
}
