#pragma once
#include "ptz.h"

// Vorschaubild von der Kamera.
//
// Das Bild kommt in Sendeaufloesung an - also klein, denn jedes Byte muss
// durch die Bluetooth-Strecke. Hochgerechnet wird erst beim Zeichnen. Das
// spart Speicher und laesst die Bildgroesse frei waehlbar.

void preview_init(void);
void preview_deinit(void);

// Nimmt Bildnachrichten entgegen. Gibt true zurueck, wenn die Nachricht
// zum Bild gehoerte.
bool preview_handle_message(DictionaryIterator *iter);

// Ein neues Bild anfordern. Tut nichts, wenn die Vorschau aus ist.
void preview_request(void);

// Laufende Uebertragung verwerfen - etwa, wenn eine Fahrt beginnt.
void preview_cancel(void);

bool preview_enabled(void);
void preview_set_enabled(bool on);

bool preview_has_image(void);
bool preview_is_loading(void);
uint8_t preview_progress(void);   // 0..100

// Zeichnet das Bild formatfuellend in das Rechteck und liefert den Bereich
// zurueck, den es tatsaechlich einnimmt. Ohne Bild bleibt das Rechteck leer.
GRect preview_draw(GContext *ctx, GRect frame);

// Bild mit Verzoegerung anfordern. Nach einer Fahrt braucht die Kamera einen
// Moment, bis das Bild steht - und ein Bild, das waehrend der Bewegung
// entsteht, ist verwischt und wertlos.
void preview_request_delayed(uint32_t delay_ms);
