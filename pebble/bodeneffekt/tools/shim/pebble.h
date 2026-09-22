// Minimaler Ersatz fuer pebble.h, damit world.c unveraendert auf dem Rechner
// uebersetzt und die Welt ohne Uhr begutachtet werden kann.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define TRIG_MAX_ANGLE 0x10000
#define TRIG_MAX_RATIO 0xffff

typedef struct { uint8_t argb; } GColor8;
typedef GColor8 GColor;

static inline int32_t sin_lookup(int32_t a) {
  return (int32_t)(sin((double)a * 2.0 * M_PI / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}
static inline int32_t cos_lookup(int32_t a) {
  return (int32_t)(cos((double)a * 2.0 * M_PI / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}
static inline GColor8 GColorFromRGB(uint8_t r, uint8_t g, uint8_t b) {
  GColor8 c;
  c.argb = (uint8_t)(0xC0 | ((r >> 6) << 4) | ((g >> 6) << 2) | (b >> 6));
  return c;
}
#define APP_LOG(...) do {} while (0)

// --- Ersatz fuer die Zeichen- und Systemaufrufe, die voxel.c benutzt --------
// Der Renderer selbst schreibt nur in einen uint8-Puffer; alles andere (Layer,
// Timer, Text) ist fuer die Vorschau ohne Wirkung.
typedef struct { int16_t x, y; } GPoint;
typedef struct { int16_t w, h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
#define GRect(x, y, w, h) ((GRect){ { (int16_t)(x), (int16_t)(y) }, { (int16_t)(w), (int16_t)(h) } })
typedef struct Layer Layer;
typedef struct GContext GContext;
typedef struct GBitmap GBitmap;
typedef void *GFont;
typedef void *AppTimer;
typedef enum { GBitmapFormat8Bit = 1 } GBitmapFormat;
typedef enum { GTextOverflowModeTrailingEllipsis = 0 } GTextOverflowMode;
typedef enum { GTextAlignmentLeft = 0 } GTextAlignment;
#define GColorWhite ((GColor8){ 0xFF })
#define FONT_KEY_GOTHIC_14 "g14"
#define FONT_KEY_GOTHIC_18_BOLD "g18b"
#define APP_LOG_LEVEL_INFO 0
static inline GBitmap *graphics_capture_frame_buffer_format(GContext *c, GBitmapFormat f) { (void)c; (void)f; return 0; }
static inline void graphics_release_frame_buffer(GContext *c, GBitmap *b) { (void)c; (void)b; }
static inline uint8_t *gbitmap_get_data(GBitmap *b) { (void)b; return 0; }
static inline uint16_t gbitmap_get_bytes_per_row(GBitmap *b) { (void)b; return 0; }
static inline GRect gbitmap_get_bounds(GBitmap *b) { (void)b; return GRect(0, 0, 0, 0); }
static inline GBitmapFormat gbitmap_get_format(GBitmap *b) { (void)b; return GBitmapFormat8Bit; }
static inline void graphics_context_set_text_color(GContext *c, GColor8 col) { (void)c; (void)col; }
static inline void graphics_draw_text(GContext *c, const char *t, GFont f, GRect r,
                                      GTextOverflowMode o, GTextAlignment a, void *p) {
  (void)c; (void)t; (void)f; (void)r; (void)o; (void)a; (void)p;
}
static inline GFont fonts_get_system_font(const char *k) { (void)k; return 0; }
static inline void layer_set_update_proc(Layer *l, void (*p)(Layer *, GContext *)) { (void)l; (void)p; }
static inline void layer_mark_dirty(Layer *l) { (void)l; }
static inline AppTimer *app_timer_register(uint32_t ms, void (*cb)(void *), void *d) {
  (void)ms; (void)cb; (void)d; return 0;
}
static inline void app_timer_cancel(AppTimer *t) { (void)t; }

// Persistenz gibt es auf dem Rechner nicht; die Vorschau nutzt die Standardwerte.
static inline bool persist_exists(uint32_t key) { (void)key; return false; }
static inline int persist_read_data(uint32_t k, void *b, size_t n) { (void)k; (void)b; (void)n; return 0; }
static inline int persist_write_data(uint32_t k, const void *b, size_t n) { (void)k; (void)b; (void)n; return (int)n; }
