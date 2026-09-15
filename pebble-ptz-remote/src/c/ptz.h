#pragma once
#include <pebble.h>

// ---------------------------------------------------------------------------
// PTZ Remote - gemeinsame Typen
//
// Geschwindigkeiten werden in der App als vorzeichenbehafteter Wert von
// -49 bis +49 gefuehrt, 0 bedeutet Stillstand. Erst beim Versand an das
// Telefon wird daraus der Panasonic-Wert 01..99 (50 = Stopp).
// ---------------------------------------------------------------------------

#define PTZ_SPEED_MAX      49
#define PTZ_NEUTRAL        50
#define PTZ_MAX_CAMERAS     8
#define PTZ_MAX_PRESETS    24
#define PTZ_NAME_LEN       24

// Befehle Richtung Telefon
typedef enum {
  PTZ_CMD_MOVE          = 1,  // Pan/Tilt/Zoom Geschwindigkeit
  PTZ_CMD_PRESET_RECALL = 2,
  PTZ_CMD_PRESET_STORE  = 3,
  PTZ_CMD_FOCUS         = 4,  // manuelle Fokusgeschwindigkeit
  PTZ_CMD_AUTOFOCUS     = 5,  // VALUE: 0 = manuell, 1 = automatisch
  PTZ_CMD_POWER         = 6,  // VALUE: 0 = Standby, 1 = an
  PTZ_CMD_SELECT_CAM    = 7,  // VALUE: Index der Kamera
  PTZ_CMD_HELLO         = 8,  // Zustand nach dem Start abfragen
  PTZ_CMD_STOP_ALL      = 9,  // Not-Stopp, alle Achsen
} PtzCommand;

// Verbindungszustand, kommt vom Telefon zurueck
typedef enum {
  PTZ_STATUS_UNKNOWN  = 0,  // noch keine Antwort
  PTZ_STATUS_OK       = 1,  // Kamera antwortet
  PTZ_STATUS_BUSY     = 2,  // Befehl unterwegs
  PTZ_STATUS_ERROR    = 3,  // Kamera nicht erreichbar
  PTZ_STATUS_NOCONFIG = 4,  // keine Kamera eingerichtet
} PtzStatus;

// Welche Achse die Auf/Ab-Tasten in der Tastenansicht bedienen
typedef enum {
  PTZ_AXIS_TILT = 0,
  PTZ_AXIS_PAN  = 1,
  PTZ_AXIS_ZOOM = 2,
  PTZ_AXIS_FOCUS = 3,
  PTZ_AXIS_COUNT = 4,
} PtzAxis;

// Welche Achsen die Neigung der Uhr bedient
typedef enum {
  GYRO_AXES_PAN_TILT = 0,  // seitlich = Pan, vor/zurueck = Tilt
  GYRO_AXES_PAN_ZOOM = 1,  // seitlich = Pan, vor/zurueck = Zoom
  GYRO_AXES_TILT_ONLY = 2, // nur vor/zurueck = Tilt, Pan gesperrt
  GYRO_AXES_COUNT = 3,
} GyroAxes;

const char *ptz_axis_name(PtzAxis axis);
const char *gyro_axes_name(GyroAxes mode);
