/*
 * AppMessage transport.
 *
 * Only one AppMessage can be in flight at any time, and Bluetooth LE round
 * trips are slow (~50-150 ms). Motion control produces a new pan/tilt value
 * every 100 ms, so movement updates are coalesced: we only ever keep the
 * newest values and send them as soon as the previous message has been
 * acknowledged and a minimum interval has elapsed. Discrete commands
 * (presets, focus, ...) go through a small FIFO so none of them is lost.
 */
#include "comm.h"

#define INBOX_SIZE   256
#define OUTBOX_SIZE  128
#define MOVE_MIN_INTERVAL_MS 90
/* While the camera is moving the last MOVE is repeated at this interval so
 * the bridge's watchdog (default 1.5 s) knows the watch is still alive. If
 * the Bluetooth link drops, the keepalives stop and the bridge halts the
 * camera on its own. */
#define MOVE_KEEPALIVE_MS 500
#define CMD_QUEUE_LEN 8

typedef struct {
  PtzCommand cmd;
  int8_t cam;
  int16_t a;   /* PAN / PRESET / focus speed */
  int16_t b;   /* TILT */
  int16_t c;   /* ZOOM */
} QueuedMsg;

static struct {
  bool in_flight;
  bool throttle_active;
  AppTimer *throttle_timer;
  AppTimer *keepalive_timer;

  /* movement slot: newest values always win */
  bool move_dirty;
  int8_t move_cam;
  int8_t move_pan, move_tilt, move_zoom;

  /* discrete command FIFO */
  QueuedMsg queue[CMD_QUEUE_LEN];
  uint8_t q_head, q_len;

  bool connected;
  uint8_t consecutive_failures;

  CommCamerasChangedCallback cameras_cb;
  CommStatusChangedCallback status_cb;
  CommSettingsChangedCallback settings_cb;
} s_comm;

static void prv_flush(void);

static void prv_keepalive_fired(void *ctx) {
  s_comm.keepalive_timer = NULL;
  if (s_comm.move_pan || s_comm.move_tilt || s_comm.move_zoom) {
    s_comm.move_dirty = true;
    prv_flush();
  }
}

/* ------------------------------------------------------------------ */
/* Sending                                                             */
/* ------------------------------------------------------------------ */

static bool prv_begin(DictionaryIterator **iter) {
  AppMessageResult r = app_message_outbox_begin(iter);
  if (r != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "outbox_begin failed: %d", (int)r);
    return false;
  }
  return true;
}

static void prv_send_move_now(void) {
  DictionaryIterator *iter;
  if (!prv_begin(&iter)) return;
  dict_write_uint8(iter, MESSAGE_KEY_CMD, CMD_MOVE);
  dict_write_int8(iter, MESSAGE_KEY_CAM_INDEX, s_comm.move_cam);
  dict_write_int8(iter, MESSAGE_KEY_PAN, s_comm.move_pan);
  dict_write_int8(iter, MESSAGE_KEY_TILT, s_comm.move_tilt);
  dict_write_int8(iter, MESSAGE_KEY_ZOOM, s_comm.move_zoom);
  dict_write_end(iter);
  if (app_message_outbox_send() == APP_MSG_OK) {
    s_comm.in_flight = true;
    s_comm.move_dirty = false;
  }

  if (s_comm.keepalive_timer) {
    app_timer_cancel(s_comm.keepalive_timer);
    s_comm.keepalive_timer = NULL;
  }
  if (s_comm.move_pan || s_comm.move_tilt || s_comm.move_zoom) {
    s_comm.keepalive_timer = app_timer_register(MOVE_KEEPALIVE_MS, prv_keepalive_fired, NULL);
  }
}

static void prv_send_queued_now(const QueuedMsg *m) {
  DictionaryIterator *iter;
  if (!prv_begin(&iter)) return;
  dict_write_uint8(iter, MESSAGE_KEY_CMD, (uint8_t)m->cmd);
  dict_write_int8(iter, MESSAGE_KEY_CAM_INDEX, m->cam);
  switch (m->cmd) {
    case CMD_PRESET_RECALL:
    case CMD_PRESET_STORE:
      dict_write_uint8(iter, MESSAGE_KEY_PRESET, (uint8_t)m->a);
      break;
    case CMD_FOCUS:
      dict_write_int8(iter, MESSAGE_KEY_ZOOM, (int8_t)m->a);
      break;
    case CMD_MOVE:
      dict_write_int8(iter, MESSAGE_KEY_PAN, (int8_t)m->a);
      dict_write_int8(iter, MESSAGE_KEY_TILT, (int8_t)m->b);
      dict_write_int8(iter, MESSAGE_KEY_ZOOM, (int8_t)m->c);
      break;
    default:
      break;
  }
  dict_write_end(iter);
  if (app_message_outbox_send() == APP_MSG_OK) {
    s_comm.in_flight = true;
  }
}

static void prv_throttle_fired(void *ctx) {
  s_comm.throttle_timer = NULL;
  s_comm.throttle_active = false;
  prv_flush();
}

static void prv_flush(void) {
  if (s_comm.in_flight) return;

  /* discrete commands first - they are rare and must not be starved */
  if (s_comm.q_len > 0) {
    QueuedMsg m = s_comm.queue[s_comm.q_head];
    s_comm.q_head = (s_comm.q_head + 1) % CMD_QUEUE_LEN;
    s_comm.q_len--;
    prv_send_queued_now(&m);
    return;
  }

  if (!s_comm.move_dirty) return;

  bool is_stop = (s_comm.move_pan == 0 && s_comm.move_tilt == 0 && s_comm.move_zoom == 0);
  if (s_comm.throttle_active && !is_stop) return;  /* wait for the timer */

  if (s_comm.throttle_timer) {
    app_timer_cancel(s_comm.throttle_timer);
    s_comm.throttle_timer = NULL;
    s_comm.throttle_active = false;
  }
  prv_send_move_now();
  s_comm.throttle_active = true;
  s_comm.throttle_timer = app_timer_register(MOVE_MIN_INTERVAL_MS, prv_throttle_fired, NULL);
}

static void prv_enqueue(QueuedMsg m) {
  if (s_comm.q_len >= CMD_QUEUE_LEN) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "command queue full, dropping oldest");
    s_comm.q_head = (s_comm.q_head + 1) % CMD_QUEUE_LEN;
    s_comm.q_len--;
  }
  s_comm.queue[(s_comm.q_head + s_comm.q_len) % CMD_QUEUE_LEN] = m;
  s_comm.q_len++;
  prv_flush();
}

void comm_send_move(int8_t cam, int8_t pan, int8_t tilt, int8_t zoom) {
  if (cam < 0) return;
  s_comm.move_cam = cam;
  s_comm.move_pan = pan;
  s_comm.move_tilt = tilt;
  s_comm.move_zoom = zoom;
  s_comm.move_dirty = true;
  prv_flush();
}

void comm_send_stop(int8_t cam) {
  if (cam < 0) return;
  /* A stop is expressed as a zero move so it naturally supersedes any
   * pending movement update that has not been sent yet. */
  comm_send_move(cam, 0, 0, 0);
}

void comm_send_focus(int8_t cam, int8_t speed) {
  if (cam < 0) return;
  prv_enqueue((QueuedMsg){ .cmd = CMD_FOCUS, .cam = cam, .a = speed });
}

void comm_send_simple(PtzCommand cmd, int8_t cam) {
  prv_enqueue((QueuedMsg){ .cmd = cmd, .cam = cam });
}

void comm_send_preset(PtzCommand cmd, int8_t cam, uint8_t preset) {
  if (cam < 0) return;
  prv_enqueue((QueuedMsg){ .cmd = cmd, .cam = cam, .a = preset });
}

void comm_request_cameras(void) {
  comm_send_simple(CMD_LIST_CAMERAS, -1);
}

void comm_request_refresh(void) {
  comm_send_simple(CMD_REFRESH, -1);
}

bool comm_is_connected(void) {
  return s_comm.connected;
}

/* ------------------------------------------------------------------ */
/* AppMessage callbacks                                                */
/* ------------------------------------------------------------------ */

static void prv_outbox_sent(DictionaryIterator *iter, void *ctx) {
  s_comm.in_flight = false;
  s_comm.consecutive_failures = 0;
  prv_flush();
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "outbox failed: %d", (int)reason);
  s_comm.in_flight = false;
  if (s_comm.consecutive_failures < 255) s_comm.consecutive_failures++;
  /* Movement values are still marked dirty if they changed meanwhile;
   * a failed stop must be retried, so re-mark it. */
  if (s_comm.move_pan == 0 && s_comm.move_tilt == 0 && s_comm.move_zoom == 0) {
    s_comm.move_dirty = true;
  }
  prv_flush();
}

static void prv_inbox_dropped(AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "inbox dropped: %d", (int)reason);
}

static void prv_inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t;
  bool cameras_changed = false;
  bool status_changed = false;
  bool settings_changed = false;

  s_comm.connected = true;

  if ((t = dict_find(iter, MESSAGE_KEY_CAM_COUNT))) {
    int32_t n = t->value->int32;
    if (n < 0) n = 0;
    if (n > MAX_CAMERAS) n = MAX_CAMERAS;
    g_app.camera_count = (uint8_t)n;
    for (int i = 0; i < MAX_CAMERAS; i++) {
      g_app.cameras[i].name[0] = '\0';
      g_app.cameras[i].ptz = false;
    }
    if (g_app.selected_camera >= (int8_t)n) g_app.selected_camera = -1;
    cameras_changed = true;
  }

  if ((t = dict_find(iter, MESSAGE_KEY_CAM_INDEX))) {
    int32_t idx = t->value->int32;
    Tuple *name = dict_find(iter, MESSAGE_KEY_CAM_NAME);
    if (name && idx >= 0 && idx < MAX_CAMERAS) {
      strncpy(g_app.cameras[idx].name, name->value->cstring, CAMERA_NAME_LEN - 1);
      g_app.cameras[idx].name[CAMERA_NAME_LEN - 1] = '\0';
      Tuple *ptz = dict_find(iter, MESSAGE_KEY_CAM_PTZ);
      g_app.cameras[idx].ptz = ptz ? (ptz->value->int32 != 0) : true;
      if (idx >= g_app.camera_count) g_app.camera_count = (uint8_t)(idx + 1);
      cameras_changed = true;
    }
  }

  if ((t = dict_find(iter, MESSAGE_KEY_STATUS))) {
    g_app.status = (PtzStatus)t->value->int32;
    Tuple *txt = dict_find(iter, MESSAGE_KEY_STATUS_TEXT);
    if (txt) {
      strncpy(g_app.status_text, txt->value->cstring, STATUS_TEXT_LEN - 1);
      g_app.status_text[STATUS_TEXT_LEN - 1] = '\0';
    } else {
      g_app.status_text[0] = '\0';
    }
    status_changed = true;
  }

  if ((t = dict_find(iter, MESSAGE_KEY_CFG_INVERT_PAN))) {
    g_app.settings.invert_pan = t->value->int32 != 0;
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CFG_INVERT_TILT))) {
    g_app.settings.invert_tilt = t->value->int32 != 0;
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CFG_SENSITIVITY))) {
    int32_t v = t->value->int32;
    if (v < 1) v = 1;
    if (v > 10) v = 10;
    g_app.settings.sensitivity = (uint8_t)v;
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CFG_DEADZONE))) {
    int32_t v = t->value->int32;
    if (v < 0) v = 0;
    if (v > 30) v = 30;
    g_app.settings.deadzone = (uint8_t)v;
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CFG_SPEED))) {
    int32_t v = t->value->int32;
    if (v < 0) v = 0;
    if (v >= SPEED_COUNT) v = SPEED_COUNT - 1;
    g_app.settings.speed = (PtzSpeed)v;
    settings_changed = true;
  }

  if (settings_changed) {
    settings_save();
    if (s_comm.settings_cb) s_comm.settings_cb();
  }
  if (cameras_changed && s_comm.cameras_cb) s_comm.cameras_cb();
  if (status_changed && s_comm.status_cb) s_comm.status_cb();
}

/* ------------------------------------------------------------------ */

void comm_set_cameras_callback(CommCamerasChangedCallback cb) { s_comm.cameras_cb = cb; }
void comm_set_status_callback(CommStatusChangedCallback cb) { s_comm.status_cb = cb; }
void comm_set_settings_callback(CommSettingsChangedCallback cb) { s_comm.settings_cb = cb; }

void comm_init(void) {
  memset(&s_comm, 0, sizeof(s_comm));
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_sent(prv_outbox_sent);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_open(INBOX_SIZE, OUTBOX_SIZE);
}

void comm_deinit(void) {
  if (s_comm.throttle_timer) {
    app_timer_cancel(s_comm.throttle_timer);
    s_comm.throttle_timer = NULL;
  }
  if (s_comm.keepalive_timer) {
    app_timer_cancel(s_comm.keepalive_timer);
    s_comm.keepalive_timer = NULL;
  }
  app_message_deregister_callbacks();
}
