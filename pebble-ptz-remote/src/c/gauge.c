#include "gauge.h"
#include "comm.h"

#define ZOOM_BAR_W 14
#define ZOOM_GAP    6

static GColor color_active(void) {
  // Rot ist in der Videotechnik die Farbe fuer "laeuft". Auf den
  // Schwarzweiss-Uhren bleibt nur der dicke Rahmen als Signal.
  return PBL_IF_COLOR_ELSE(GColorRed, GColorBlack);
}

static GColor color_idle(void) {
  return PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack);
}

void gauge_draw(GContext *ctx, GRect frame, const GaugeState *state) {
  const bool active = state->active;

  // Zoombalken rechts abtrennen, der Rest ist das Feld fuer Pan und Tilt.
  GRect zoom_rect = GRect(frame.origin.x + frame.size.w - ZOOM_BAR_W,
                          frame.origin.y, ZOOM_BAR_W, frame.size.h);
  GRect field = GRect(frame.origin.x, frame.origin.y,
                      frame.size.w - ZOOM_BAR_W - ZOOM_GAP, frame.size.h);

  // Feldrahmen. Ueber einem Bild bleibt er weg, solange nichts faehrt -
  // das Bild soll die Flaeche haben, nicht die Rahmenlinie.
  if (!state->over_image || active) {
    graphics_context_set_stroke_color(ctx, active ? color_active() : color_idle());
    graphics_context_set_stroke_width(ctx, active ? 3 : 1);
    graphics_draw_round_rect(ctx, field, 4);
  }

  const GPoint center = GPoint(field.origin.x + field.size.w / 2,
                               field.origin.y + field.size.h / 2);
  const int16_t half_w = field.size.w / 2 - 6;
  const int16_t half_h = field.size.h / 2 - 6;

  // Achsenkreuz, gesperrte Achsen nur angedeutet. Auf einem Bild wuerde es
  // nur stoeren, dort zeigt allein der Punkt die Auslenkung.
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack));
  if (!state->over_image && !state->pan_locked) {
    graphics_draw_line(ctx, GPoint(center.x - half_w, center.y),
                            GPoint(center.x + half_w, center.y));
  }
  if (!state->over_image && !state->tilt_locked) {
    graphics_draw_line(ctx, GPoint(center.x, center.y - half_h),
                            GPoint(center.x, center.y + half_h));
  }

  // Punkt fuer die aktuelle Neigung
  int16_t px = center.x + (int16_t)((int32_t)state->pan  * half_w / 100);
  int16_t py = center.y - (int16_t)((int32_t)state->tilt * half_h / 100);
  if (state->pan_locked)  px = center.x;
  if (state->tilt_locked) py = center.y;

  // Spur vom Mittelpunkt zum Punkt macht die Auslenkung auch dann lesbar,
  // wenn der Blick nur kurz auf die Uhr faellt.
  graphics_context_set_stroke_width(ctx, 2);
  graphics_context_set_stroke_color(ctx, active ? color_active() : color_idle());
  graphics_draw_line(ctx, center, GPoint(px, py));

  if (state->over_image) {
    // Ein heller Saum, damit der Punkt auch auf einem dunklen Buehnenbild
    // zu sehen ist.
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, GPoint(px, py), active ? 9 : 7);
  }
  graphics_context_set_fill_color(ctx, active ? color_active()
                                              : PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
  graphics_fill_circle(ctx, GPoint(px, py), active ? 7 : 5);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(px, py), active ? 3 : 2);

  // Zoombalken: Mitte ist Stillstand, oben Tele, unten Weitwinkel.
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, color_idle());
  graphics_draw_round_rect(ctx, zoom_rect, 3);
  int16_t zc = zoom_rect.origin.y + zoom_rect.size.h / 2;
  graphics_draw_line(ctx, GPoint(zoom_rect.origin.x + 2, zc),
                          GPoint(zoom_rect.origin.x + zoom_rect.size.w - 3, zc));
  if (state->zoom != 0) {
    int16_t len = (int16_t)((int32_t)(state->zoom < 0 ? -state->zoom : state->zoom)
                            * (zoom_rect.size.h / 2 - 3) / 100);
    GRect bar = state->zoom > 0
        ? GRect(zoom_rect.origin.x + 3, zc - len, zoom_rect.size.w - 6, len)
        : GRect(zoom_rect.origin.x + 3, zc, zoom_rect.size.w - 6, len);
    graphics_context_set_fill_color(ctx, active ? color_active() : color_idle());
    graphics_fill_rect(ctx, bar, 0, GCornerNone);
  }
}

void gauge_draw_header(GContext *ctx, GRect frame, const char *cam_name,
                       PtzStatus status) {
  GColor dot;
  switch (status) {
    case PTZ_STATUS_OK:       dot = PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorBlack); break;
    case PTZ_STATUS_BUSY:     dot = PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorBlack); break;
    case PTZ_STATUS_ERROR:    dot = PBL_IF_COLOR_ELSE(GColorRed, GColorBlack); break;
    case PTZ_STATUS_NOCONFIG: dot = PBL_IF_COLOR_ELSE(GColorRed, GColorBlack); break;
    default:                  dot = PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack); break;
  }

  const int16_t r = 4;
  GPoint p = GPoint(frame.origin.x + r + 3, frame.origin.y + frame.size.h / 2);
  graphics_context_set_fill_color(ctx, dot);
  graphics_fill_circle(ctx, p, r);
  // Ein offener Ring bei unbekanntem Zustand, damit "noch keine Antwort"
  // nicht wie "alles gut" aussieht.
  if (status == PTZ_STATUS_UNKNOWN) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, p, r - 2);
  }

  GRect text = GRect(frame.origin.x + 2 * r + 8, frame.origin.y - 2,
                     frame.size.w - 2 * r - 10, frame.size.h + 2);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, cam_name, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     text, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}
