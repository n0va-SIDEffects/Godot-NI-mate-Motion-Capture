#pragma once
#include <pebble.h>
#include "config.h"
#include "voxel.h"

// Flugmodell: konstantes Grundtempo vorwaerts, der Spieler steuert nur Roll
// (Kurs) und Hoehe. Der Roll dreht den Kurs, die Hoehe kommt je nach Profil
// aus dem Fingerversatz oder aus dem Flappy-Modell der Tasten.
typedef struct {
  int32_t x16, y16;          // Weltposition 16.16
  int32_t h8;                // Hoehe ueber Null, 24.8
  int32_t vz8;               // Steigrate in 24.8 Zellen pro Sekunde
  int32_t yaw;
  int32_t roll_deg8;
  int32_t agl8;              // Hoehe ueber Grund, 24.8
  int32_t ground8;
  uint32_t contacts;         // Bodenkontakte
  uint32_t effect_ms;        // Zeit im Bodeneffekt-Fenster ("Sohlenzeit")
  bool in_effect;
  bool on_ground;            // liegt gerade auf
  bool hit;                  // neuer Bodenkontakt in diesem Tick (Flanke)
} Flight;

void flight_reset(int32_t x16, int32_t y16);
void flight_step(uint32_t dt_ms);
const Flight *flight_state(void);
void flight_fill_camera(Camera *cam);
