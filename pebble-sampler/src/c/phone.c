#include "phone.h"

static PhoneSlot s_slots[PHONE_MAX_SLOTS];
static PhoneSlotChangedCb s_slot_cb;
static PhoneSettingsCb s_settings_cb;
static uint32_t s_inbox_size;

static void prv_reset_slot(int i) {
  if (s_slots[i].data) free(s_slots[i].data);
  s_slots[i].data = NULL;
  s_slots[i].total = 0;
  s_slots[i].received = 0;
  s_slots[i].state = PhoneSlotEmpty;
}

static void prv_send_hello(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  // Largest data chunk the phone may send: inbox minus dictionary overhead.
  uint32_t chunk = s_inbox_size > 300 ? s_inbox_size - 200 : 100;
  if (chunk > 2000) chunk = 2000;
  dict_write_uint32(out, MESSAGE_KEY_Hello, chunk);
  app_message_outbox_send();
}

static void prv_handle_transfer(DictionaryIterator *iter, Tuple *slot_t) {
  int slot = (int)slot_t->value->int32;
  if (slot < 0 || slot >= PHONE_MAX_SLOTS) return;
  PhoneSlot *s = &s_slots[slot];

  Tuple *err = dict_find(iter, MESSAGE_KEY_XferError);
  if (err) {
    prv_reset_slot(slot);
    if (err->value->int32 == 1) {
      s->state = PhoneSlotError;
      snprintf(s->name, sizeof(s->name), "Sample %d", slot + 1);
    }
    if (s_slot_cb) s_slot_cb(slot);
    return;
  }
  Tuple *done = dict_find(iter, MESSAGE_KEY_XferDone);
  if (done) {
    s->state = (s->data && s->received >= s->total && s->total > 0) ? PhoneSlotReady : PhoneSlotError;
    if (s_slot_cb) s_slot_cb(slot);
    return;
  }
  Tuple *total_t = dict_find(iter, MESSAGE_KEY_XferTotal);
  Tuple *offset_t = dict_find(iter, MESSAGE_KEY_XferOffset);
  Tuple *data_t = dict_find(iter, MESSAGE_KEY_XferData);
  if (!total_t || !offset_t || !data_t) return;

  uint32_t total = total_t->value->uint32;
  uint32_t offset = offset_t->value->uint32;
  if (offset == 0) {
    prv_reset_slot(slot);
    Tuple *name_t = dict_find(iter, MESSAGE_KEY_XferName);
    snprintf(s->name, sizeof(s->name), "%s", name_t ? name_t->value->cstring : "Sample");
    if (total == 0 || total > PHONE_MAX_SAMPLE_BYTES) {
      s->state = PhoneSlotError;
      if (s_slot_cb) s_slot_cb(slot);
      return;
    }
    s->data = malloc(total);
    if (!s->data) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "no memory for phone sample %d (%lu bytes)", slot, (unsigned long)total);
      s->state = PhoneSlotError;
      if (s_slot_cb) s_slot_cb(slot);
      return;
    }
    s->total = total;
    s->state = PhoneSlotLoading;
  }
  if (!s->data || total != s->total || offset != s->received) {
    return;  // out of order or stale chunk; the phone will restart on failure
  }
  uint32_t len = data_t->length;
  if (offset + len > s->total) len = s->total - offset;
  memcpy(s->data + offset, data_t->value->data, len);
  s->received = offset + len;
  if (s_slot_cb) s_slot_cb(slot);
}

static void prv_inbox_received(DictionaryIterator *iter, void *ctx) {
  if (dict_find(iter, MESSAGE_KEY_Ready)) {
    prv_send_hello();
    return;
  }
  Tuple *slot_t = dict_find(iter, MESSAGE_KEY_XferSlot);
  if (slot_t) {
    prv_handle_transfer(iter, slot_t);
    return;
  }
  Tuple *vol = dict_find(iter, MESSAGE_KEY_Volume);
  Tuple *shake = dict_find(iter, MESSAGE_KEY_Shake);
  Tuple *touch = dict_find(iter, MESSAGE_KEY_Touch);
  PhoneSettings st = {
    .volume = vol ? (int)vol->value->int32 : -1,
    .shake = shake ? (int)shake->value->int32 : -1,
    .touch = touch ? (int)touch->value->int32 : -1,
  };
  for (int i = 0; i < PHONE_MAX_ENABLED_BITS; i++) {
    Tuple *t = dict_find(iter, MESSAGE_KEY_Enabled + i);
    if (!t) break;
    st.has_enabled = true;
    if (t->value->int32) st.enabled_mask |= ((uint64_t)1 << i);
  }
  if (vol || shake || touch || st.has_enabled) {
    if (s_settings_cb) s_settings_cb(&st);
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "inbox dropped: %d", (int)reason);
}

void phone_init(PhoneSlotChangedCb slot_cb, PhoneSettingsCb settings_cb) {
  s_slot_cb = slot_cb;
  s_settings_cb = settings_cb;
  for (int i = 0; i < PHONE_MAX_SLOTS; i++) {
    s_slots[i] = (PhoneSlot) { .state = PhoneSlotEmpty };
  }
  s_inbox_size = app_message_inbox_size_maximum();
  if (s_inbox_size > 4096) s_inbox_size = 4096;   // keep RAM for the samples themselves
  if (s_inbox_size < 512) s_inbox_size = 512;
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  AppMessageResult r = app_message_open(s_inbox_size, 64);
  if (r != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "app_message_open failed: %d", (int)r);
  }
  // In case the phone side is already up and waiting.
  prv_send_hello();
}

void phone_deinit(void) {
  app_message_deregister_callbacks();
  for (int i = 0; i < PHONE_MAX_SLOTS; i++) prv_reset_slot(i);
}

const PhoneSlot *phone_slot(int slot) {
  if (slot < 0 || slot >= PHONE_MAX_SLOTS) return NULL;
  return &s_slots[slot];
}

int phone_active_slots(void) {
  int n = 0;
  for (int i = 0; i < PHONE_MAX_SLOTS; i++) {
    if (s_slots[i].state != PhoneSlotEmpty) n = i + 1;
  }
  return n;
}
