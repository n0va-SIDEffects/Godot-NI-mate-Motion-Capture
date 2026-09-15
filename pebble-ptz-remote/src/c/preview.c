#include "preview.h"
#include "comm.h"
#include "settings.h"

#define PREVIEW_MAX_W     200
#define PREVIEW_MAX_H     160
#define PREVIEW_TIMEOUT_MS 12000

static uint8_t *s_pixels;        // ein GColor8-Byte je Bildpunkt
static uint16_t s_width, s_height;
static uint32_t s_expected;      // erwartete Bytes in gepackter Form
static uint32_t s_received;
static GColor   s_palette[16];
static bool     s_loading;
static bool     s_complete;
static AppTimer *s_timeout;
static AppTimer *s_delayed;
static uint8_t  s_failures;      // erfolglose Abrufe in Folge

static void drop_image(void) {
  if (s_pixels) {
    free(s_pixels);
    s_pixels = NULL;
  }
  s_width = s_height = 0;
  s_complete = false;
}

static void stop_timeout(void) {
  if (s_timeout) {
    app_timer_cancel(s_timeout);
    s_timeout = NULL;
  }
}

static void on_timeout(void *ctx) {
  s_timeout = NULL;
  // Der Rest des Bildes kommt nicht mehr. Ein halbes Bild ist besser als
  // ein Ladebalken, der nie fertig wird.
  s_loading = false;
  if (s_received > 0) {
    s_complete = true;
    s_failures = 0;
  } else {
    s_failures++;
  }
}

static void arm_timeout(void) {
  stop_timeout();
  s_timeout = app_timer_register(PREVIEW_TIMEOUT_MS, on_timeout, NULL);
}

void preview_init(void) {
  s_pixels = NULL;
  s_loading = false;
  s_complete = false;
  s_failures = 0;
  for (int i = 0; i < 16; i++) {
    s_palette[i] = GColorBlack;
  }
}

void preview_deinit(void) {
  if (s_delayed) {
    app_timer_cancel(s_delayed);
    s_delayed = NULL;
  }
  stop_timeout();
  drop_image();
}

bool preview_enabled(void) {
  return settings_get()->preview_on;
}

void preview_set_enabled(bool on) {
  settings_get()->preview_on = on;
  settings_save();
  s_failures = 0;
  if (!on) {
    preview_cancel();
    drop_image();
  }
}

void preview_request(void) {
  if (!preview_enabled() || s_loading) {
    return;
  }
  // Nach zwei vergeblichen Anlaeufen nicht weiter von selbst nachfragen.
  // Eine nicht erreichbare Kamera soll die Funkstrecke nicht dauerhaft
  // belegen; ueber das Menue laesst sich jederzeit neu anstossen.
  if (s_failures >= 2) {
    return;
  }
  comm_send_command(PTZ_CMD_PREVIEW, 1);
  s_loading = true;
  s_received = 0;
  arm_timeout();
}

static void on_delayed(void *ctx) {
  s_delayed = NULL;
  preview_request();
}

void preview_request_delayed(uint32_t delay_ms) {
  if (!preview_enabled()) return;
  if (s_delayed) {
    app_timer_cancel(s_delayed);
  }
  s_delayed = app_timer_register(delay_ms, on_delayed, NULL);
}

void preview_cancel(void) {
  if (s_delayed) {
    app_timer_cancel(s_delayed);
    s_delayed = NULL;
  }
  if (s_loading) {
    comm_send_command(PTZ_CMD_PREVIEW, 0);
    s_loading = false;
    stop_timeout();
  }
}

bool preview_has_image(void) {
  return s_pixels != NULL && s_complete;
}

bool preview_is_loading(void) {
  return s_loading;
}

uint8_t preview_progress(void) {
  if (s_expected == 0) return 0;
  uint32_t pct = s_received * 100 / s_expected;
  return pct > 100 ? 100 : (uint8_t)pct;
}

// --- Empfang ---------------------------------------------------------------

static bool start_image(DictionaryIterator *iter) {
  Tuple *tw = dict_find(iter, MESSAGE_KEY_IMG_W);
  Tuple *th = dict_find(iter, MESSAGE_KEY_IMG_H);
  Tuple *tl = dict_find(iter, MESSAGE_KEY_IMG_LEN);
  Tuple *tp = dict_find(iter, MESSAGE_KEY_IMG_PAL);
  if (!tw || !th || !tl) {
    return false;
  }

  uint16_t w = (uint16_t)tw->value->int32;
  uint16_t h = (uint16_t)th->value->int32;
  if (w == 0 || h == 0 || w > PREVIEW_MAX_W || h > PREVIEW_MAX_H) {
    return false;
  }

  drop_image();
  s_pixels = malloc((size_t)w * h);
  if (!s_pixels) {
    s_loading = false;
    return false;
  }
  memset(s_pixels, 0, (size_t)w * h);
  s_width = w;
  s_height = h;
  s_expected = (uint32_t)tl->value->int32;
  s_received = 0;
  s_loading = true;
  arm_timeout();

  if (tp && tp->type == TUPLE_BYTE_ARRAY) {
    // Ueber einen eigenen Zeiger gehen: das Datenfeld einer Nachricht ist
    // als Feld der Laenge null deklariert, und der Uebersetzer kann seine
    // wahre Groesse nicht kennen.
    const uint8_t *pal = tp->value->data;
    uint16_t n = tp->length < 16 ? tp->length : 16;
    for (uint16_t i = 0; i < n; i++) {
      s_palette[i].argb = pal[i];
    }
  }
  return true;
}

static bool receive_chunk(DictionaryIterator *iter) {
  Tuple *to = dict_find(iter, MESSAGE_KEY_IMG_OFF);
  Tuple *td = dict_find(iter, MESSAGE_KEY_IMG_DATA);
  if (!to || !td || td->type != TUPLE_BYTE_ARRAY || !s_pixels) {
    return false;
  }

  const uint32_t offset = (uint32_t)to->value->int32;
  const uint8_t *data = td->value->data;
  const uint16_t len = td->length;
  const uint16_t row_bytes = (s_width + 1) / 2;

  // Zwei Bildpunkte je Byte auspacken und ueber die Palette einfaerben.
  for (uint16_t i = 0; i < len; i++) {
    uint32_t byte_index = offset + i;
    uint16_t y = (uint16_t)(byte_index / row_bytes);
    if (y >= s_height) break;
    uint16_t x = (uint16_t)((byte_index % row_bytes) * 2);

    uint8_t b = data[i];
    s_pixels[(uint32_t)y * s_width + x] = s_palette[b >> 4].argb;
    if (x + 1 < s_width) {
      s_pixels[(uint32_t)y * s_width + x + 1] = s_palette[b & 0x0F].argb;
    }
  }

  s_received += len;
  if (s_received >= s_expected) {
    s_loading = false;
    s_complete = true;
    s_failures = 0;
    stop_timeout();
  } else {
    arm_timeout();
  }
  return true;
}

bool preview_handle_message(DictionaryIterator *iter) {
  if (dict_find(iter, MESSAGE_KEY_IMG_W)) {
    return start_image(iter);
  }
  if (dict_find(iter, MESSAGE_KEY_IMG_DATA)) {
    return receive_chunk(iter);
  }
  return false;
}

// --- Zeichnen --------------------------------------------------------------

GRect preview_draw(GContext *ctx, GRect frame) {
  if (!s_pixels || s_width == 0 || s_height == 0) {
    return GRect(frame.origin.x, frame.origin.y, 0, 0);
  }

  // Groesstmoeglich einpassen, Seitenverhaeltnis erhalten.
  int32_t scale_w = (int32_t)frame.size.w * 100 / s_width;
  int32_t scale_h = (int32_t)frame.size.h * 100 / s_height;
  int32_t scale = scale_w < scale_h ? scale_w : scale_h;
  if (scale < 100) scale = 100;      // nie kleiner als die Sendeaufloesung

  int16_t dw = (int16_t)((int32_t)s_width * scale / 100);
  int16_t dh = (int16_t)((int32_t)s_height * scale / 100);
  if (dw > frame.size.w) dw = frame.size.w;
  if (dh > frame.size.h) dh = frame.size.h;

  GRect dest = GRect(frame.origin.x + (frame.size.w - dw) / 2,
                     frame.origin.y + (frame.size.h - dh) / 2, dw, dh);

  // Direkt in den Bildspeicher schreiben. Punkt fuer Punkt ueber die
  // Zeichenfunktionen zu gehen waere bei ueber zwanzigtausend Punkten
  // deutlich zu langsam.
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) {
    return dest;
  }

  for (int16_t y = 0; y < dh; y++) {
    int16_t screen_y = dest.origin.y + y;
    GBitmapDataRowInfo row = gbitmap_get_data_row_info(fb, (uint16_t)screen_y);
    if (!row.data) continue;

    uint16_t src_y = (uint16_t)((int32_t)y * s_height / dh);
    const uint8_t *src_row = &s_pixels[(uint32_t)src_y * s_width];

    for (int16_t x = 0; x < dw; x++) {
      int16_t screen_x = dest.origin.x + x;
      if (screen_x < row.min_x || screen_x > row.max_x) continue;
      uint16_t src_x = (uint16_t)((int32_t)x * s_width / dw);
#ifdef PBL_COLOR
      row.data[screen_x] = src_row[src_x];
#else
      // Auf den Schwarzweiss-Uhren liegt ein Bit je Bildpunkt. Aus der
      // Farbe wird eine Helligkeit, daraus ein Schwellwert.
      uint8_t c = src_row[src_x];
      uint8_t lum = ((c >> 4) & 3) * 3 + ((c >> 2) & 3) * 6 + (c & 3);
      uint8_t *cell = &row.data[screen_x / 8];
      uint8_t mask = (uint8_t)(1 << (screen_x % 8));
      if (lum >= 15) {
        *cell |= mask;
      } else {
        *cell &= (uint8_t)~mask;
      }
#endif
    }
  }

  graphics_release_frame_buffer(ctx, fb);
  return dest;
}
