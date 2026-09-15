#pragma once
#include "ptz.h"

// Gemeinsame Anzeige fuer beide Steuerarten: ein Fadenkreuz mit einem Punkt
// fuer Pan und Tilt, daneben ein Balken fuer den Zoom. Die Werte sind auf
// -100..+100 bezogen, unabhaengig von der eingestellten Geschwindigkeit.
typedef struct {
  int8_t pan, tilt, zoom;
  bool   active;        // Kamera faehrt gerade
  bool   pan_locked;    // Achse in diesem Modus gesperrt
  bool   tilt_locked;
  bool   over_image;    // liegt auf dem Vorschaubild
} GaugeState;

void gauge_draw(GContext *ctx, GRect frame, const GaugeState *state);

// Kopfzeile mit Kameraname und Verbindungszustand.
void gauge_draw_header(GContext *ctx, GRect frame, const char *cam_name,
                       PtzStatus status);
