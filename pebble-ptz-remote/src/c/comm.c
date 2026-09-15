#include "comm.h"
#include "settings.h"
#include "preview.h"

#define TICK_MS          120   // Taktung der Sendeschleife
#define REPEAT_MS        400   // Wiederholung, solange eine Achse laeuft
#define QUEUE_LEN          6
#define MSG_LEN           64
#define CAM_NAMES_LEN    (PTZ_MAX_CAMERAS * (PTZ_NAME_LEN + 1))

typedef struct {
  int8_t pan, tilt, zoom, focus;
} AxisState;

typedef struct {
  PtzCommand cmd;
  int32_t    value;
} QueuedCommand;

static AxisState s_desired;          // was die Bedienung will
static AxisState s_confirmed;        // was zuletzt zugestellt wurde
static bool      s_axes_dirty;       // Sollzustand noch nicht bestaetigt
static bool      s_outbox_busy;
static time_t    s_last_sent_s;
static uint16_t  s_last_sent_ms;
static uint8_t   s_stop_repeats;     // Not-Stopp mehrfach nachschieben

static QueuedCommand s_queue[QUEUE_LEN];
static uint8_t s_queue_head, s_queue_count;

static AppTimer *s_timer;
static PtzStatus s_status = PTZ_STATUS_UNKNOWN;
static char s_message[MSG_LEN] = "Verbinde ...";
static char s_cam_names[CAM_NAMES_LEN];
static uint8_t s_cam_count;
static CommUpdateHandler s_update_handler;

// ---------------------------------------------------------------------------
// Hilfen
// ---------------------------------------------------------------------------

static int8_t clamp_speed(int v) {
  if (v >  PTZ_SPEED_MAX) return  PTZ_SPEED_MAX;
  if (v < -PTZ_SPEED_MAX) return -PTZ_SPEED_MAX;
  return (int8_t)v;
}

static bool axes_equal(const AxisState *a, const AxisState *b) {
  return a->pan == b->pan && a->tilt == b->tilt
      && a->zoom == b->zoom && a->focus == b->focus;
}

static bool axes_moving(const AxisState *a) {
  return a->pan || a->tilt || a->zoom || a->focus;
}

static uint32_t ms_since_last_send(void) {
  time_t now_s; uint16_t now_ms;
  time_ms(&now_s, &now_ms);
  int32_t diff = (int32_t)(now_s - s_last_sent_s) * 1000
               + ((int32_t)now_ms - (int32_t)s_last_sent_ms);
  // Die Uhr der Firmware springt gelegentlich um eine Sekunde. Negative
  // Abstaende als "gerade eben" werten statt als Ewigkeit.
  return diff < 0 ? 0 : (uint32_t)diff;
}

static void mark_sent(void) {
  time_ms(&s_last_sent_s, &s_last_sent_ms);
}

static void notify_update(void) {
  if (s_update_handler) {
    s_update_handler();
  }
}

static void queue_push(PtzCommand cmd, int32_t value) {
  if (s_queue_count >= QUEUE_LEN) {
    // Aelteste Anweisung verwerfen; ein veralteter Preset-Abruf nuetzt
    // niemandem mehr.
    s_queue_head = (s_queue_head + 1) % QUEUE_LEN;
    s_queue_count--;
  }
  uint8_t slot = (s_queue_head + s_queue_count) % QUEUE_LEN;
  s_queue[slot].cmd = cmd;
  s_queue[slot].value = value;
  s_queue_count++;
}

// ---------------------------------------------------------------------------
// Senden
// ---------------------------------------------------------------------------

static bool send_move(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    return false;
  }
  uint8_t cmd = PTZ_CMD_MOVE;
  // Aus -49..+49 wird der Panasonic-Bereich 01..99 mit 50 als Stillstand.
  uint8_t pan   = (uint8_t)(PTZ_NEUTRAL + s_desired.pan);
  uint8_t tilt  = (uint8_t)(PTZ_NEUTRAL + s_desired.tilt);
  uint8_t zoom  = (uint8_t)(PTZ_NEUTRAL + s_desired.zoom);
  uint8_t focus = (uint8_t)(PTZ_NEUTRAL + s_desired.focus);

  dict_write_uint8(out, MESSAGE_KEY_CMD,   cmd);
  dict_write_uint8(out, MESSAGE_KEY_PAN,   pan);
  dict_write_uint8(out, MESSAGE_KEY_TILT,  tilt);
  dict_write_uint8(out, MESSAGE_KEY_ZOOM,  zoom);
  dict_write_uint8(out, MESSAGE_KEY_FOCUS, focus);
  dict_write_uint8(out, MESSAGE_KEY_CAM,   settings_get()->active_cam);

  if (app_message_outbox_send() != APP_MSG_OK) {
    return false;
  }
  s_outbox_busy = true;
  mark_sent();
  return true;
}

static bool send_queued(void) {
  if (s_queue_count == 0) {
    return false;
  }
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    return false;
  }
  QueuedCommand *q = &s_queue[s_queue_head];
  dict_write_uint8(out, MESSAGE_KEY_CMD,   (uint8_t)q->cmd);
  dict_write_int32(out, MESSAGE_KEY_VALUE, q->value);
  dict_write_uint8(out, MESSAGE_KEY_CAM,   settings_get()->active_cam);
  if (q->cmd == PTZ_CMD_HELLO) {
    // Wie gross ein Haeppchen sein darf, weiss nur die Uhr. Das Telefon
    // richtet seine Bilduebertragung danach.
    dict_write_uint32(out, MESSAGE_KEY_MAXRX, app_message_inbox_size_maximum());
  }

  if (app_message_outbox_send() != APP_MSG_OK) {
    return false;
  }
  s_queue_head = (s_queue_head + 1) % QUEUE_LEN;
  s_queue_count--;
  s_outbox_busy = true;
  mark_sent();
  return true;
}

static void tick(void *ctx) {
  s_timer = NULL;

  if (!s_outbox_busy) {
    // Reihenfolge mit Bedacht: ein Stopp geht allem anderen vor, danach
    // Achsbewegungen, erst dann Presets und aehnliches.
    bool moving = axes_moving(&s_desired);
    bool need_axes = s_axes_dirty
                  || (moving && ms_since_last_send() >= REPEAT_MS);

    if (s_stop_repeats > 0) {
      if (send_move()) {
        s_stop_repeats--;
      }
    } else if (need_axes) {
      send_move();
    } else {
      send_queued();
    }
  }

  s_timer = app_timer_register(TICK_MS, tick, NULL);
}

// ---------------------------------------------------------------------------
// AppMessage-Rueckmeldungen
// ---------------------------------------------------------------------------

static void outbox_sent(DictionaryIterator *iter, void *ctx) {
  s_outbox_busy = false;
  s_confirmed = s_desired;
  s_axes_dirty = false;
}

static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *ctx) {
  s_outbox_busy = false;
  // Nicht bestaetigt heisst: beim naechsten Takt noch einmal. Der Sollzustand
  // bleibt stehen, damit auch ein verlorenes Stopp erneut hinausgeht.
  if (reason != APP_MSG_OK) {
    s_axes_dirty = true;
  }
  if (reason == APP_MSG_NOT_CONNECTED) {
    s_status = PTZ_STATUS_ERROR;
    strncpy(s_message, "Kein Telefon", MSG_LEN - 1);
    notify_update();
  }
}

static void inbox_received(DictionaryIterator *iter, void *ctx) {
  bool changed = false;
  Tuple *t;

  // Bilddaten machen den Grossteil des Verkehrs aus und haben mit dem
  // uebrigen Zustand nichts zu tun.
  if (preview_handle_message(iter)) {
    notify_update();
    return;
  }

  if (settings_apply_message(iter)) {
    changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_STATUS))) {
    PtzStatus st = (PtzStatus)t->value->int32;
    if (st != s_status) {
      if (st == PTZ_STATUS_ERROR && s_status != PTZ_STATUS_ERROR
          && settings_get()->vibrate) {
        vibes_short_pulse();
      }
      s_status = st;
      changed = true;
    }
  }
  if ((t = dict_find(iter, MESSAGE_KEY_MSG))) {
    strncpy(s_message, t->value->cstring, MSG_LEN - 1);
    s_message[MSG_LEN - 1] = '\0';
    changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CAM_NAMES))) {
    strncpy(s_cam_names, t->value->cstring, CAM_NAMES_LEN - 1);
    s_cam_names[CAM_NAMES_LEN - 1] = '\0';
    changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CAM_COUNT))) {
    s_cam_count = (uint8_t)t->value->int32;
    if (s_cam_count > PTZ_MAX_CAMERAS) s_cam_count = PTZ_MAX_CAMERAS;
    if (settings_get()->active_cam >= s_cam_count && s_cam_count > 0) {
      settings_get()->active_cam = 0;
      settings_save();
    }
    changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CAM_ACTIVE))) {
    uint8_t idx = (uint8_t)t->value->int32;
    if (idx < PTZ_MAX_CAMERAS) {
      settings_get()->active_cam = idx;
      settings_save();
      changed = true;
    }
  }

  if (changed) {
    notify_update();
  }
}

static void inbox_dropped(AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Nachricht verworfen: %d", (int)reason);
}

// ---------------------------------------------------------------------------
// Schnittstelle
// ---------------------------------------------------------------------------

void comm_init(void) {
  memset(&s_desired, 0, sizeof(s_desired));
  memset(&s_confirmed, 0, sizeof(s_confirmed));
  s_cam_names[0] = '\0';

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_sent(outbox_sent);
  app_message_register_outbox_failed(outbox_failed);
  // Die Eingangsseite so gross wie moeglich: durch sie kommen die Bilder.
  app_message_open(app_message_inbox_size_maximum(), 256);

  mark_sent();
  queue_push(PTZ_CMD_HELLO, 0);
  s_timer = app_timer_register(TICK_MS, tick, NULL);
}

void comm_deinit(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  // Beim Beenden darf keine Achse weiterlaufen. Der Stopp geht direkt und
  // ohne Umweg ueber die Warteschlange hinaus.
  memset(&s_desired, 0, sizeof(s_desired));
  s_outbox_busy = false;
  send_move();
}

void comm_set_move(int8_t pan, int8_t tilt, int8_t zoom) {
  s_desired.pan  = clamp_speed(pan);
  s_desired.tilt = clamp_speed(tilt);
  s_desired.zoom = clamp_speed(zoom);
  if (!axes_equal(&s_desired, &s_confirmed)) {
    s_axes_dirty = true;
  }
}

void comm_set_focus(int8_t focus) {
  s_desired.focus = clamp_speed(focus);
  if (!axes_equal(&s_desired, &s_confirmed)) {
    s_axes_dirty = true;
  }
}

void comm_stop_all(void) {
  memset(&s_desired, 0, sizeof(s_desired));
  s_axes_dirty = true;
  s_stop_repeats = 2;   // einmal ist gut, zweimal ist sicher
}

void comm_send_command(PtzCommand cmd, int32_t value) {
  queue_push(cmd, value);
}

PtzStatus comm_get_status(void) {
  return s_status;
}

const char *comm_get_message(void) {
  return s_message;
}

uint8_t comm_get_cam_count(void) {
  return s_cam_count;
}

const char *comm_get_cam_name(uint8_t index) {
  // Die Namen kommen als eine mit '|' getrennte Zeichenkette vom Telefon.
  static char name[PTZ_NAME_LEN + 1];
  const char *p = s_cam_names;
  uint8_t current = 0;

  while (*p && current < index) {
    if (*p == '|') current++;
    p++;
  }
  if (current != index || !*p) {
    snprintf(name, sizeof(name), "Kamera %d", index + 1);
    return name;
  }
  uint8_t i = 0;
  while (*p && *p != '|' && i < PTZ_NAME_LEN) {
    name[i++] = *p++;
  }
  name[i] = '\0';
  return i > 0 ? name : "Kamera";
}

bool comm_is_moving(void) {
  return axes_moving(&s_desired);
}

void comm_set_update_handler(CommUpdateHandler handler) {
  s_update_handler = handler;
}
