/*
 * NDI PTZ Remote - shared definitions for the watch app.
 *
 * Architecture:
 *   Watch (this app)  --AppMessage-->  PebbleKit JS (phone)  --HTTP-->  ndi_ptz_bridge.py  --NDI SDK-->  PTZ camera
 */
#pragma once

#include <pebble.h>

/* ---- Commands sent from the watch to the phone (MESSAGE_KEY_CMD) ---- */
typedef enum {
  CMD_LIST_CAMERAS = 0,   /* request the camera list from the bridge */
  CMD_MOVE = 1,           /* continuous move: PAN/TILT/ZOOM speeds -100..100 */
  CMD_STOP = 2,           /* stop all movement on CAM_INDEX */
  CMD_PRESET_RECALL = 3,  /* PRESET = preset number */
  CMD_PRESET_STORE = 4,   /* PRESET = preset number */
  CMD_HOME = 5,           /* pan/tilt to 0/0 */
  CMD_AUTOFOCUS = 6,      /* trigger one-shot auto focus */
  CMD_FOCUS = 7,          /* ZOOM = focus speed -100..100 */
  CMD_REFRESH = 8,        /* force NDI rediscovery on the bridge */
} PtzCommand;

/* ---- Status values sent from the phone to the watch (MESSAGE_KEY_STATUS) ---- */
typedef enum {
  STATUS_OK = 0,
  STATUS_BRIDGE_UNREACHABLE = 1,
  STATUS_NO_CAMERAS = 2,
  STATUS_LOADING = 3,
  STATUS_CMD_FAILED = 4,
  STATUS_NOT_CONFIGURED = 5,
} PtzStatus;

/* ---- Limits ---- */
#define MAX_CAMERAS       12
#define CAMERA_NAME_LEN   40
#define STATUS_TEXT_LEN   48

/* ---- Movement axes selectable in button mode ---- */
typedef enum {
  AXIS_TILT = 0,
  AXIS_PAN = 1,
  AXIS_ZOOM = 2,
  AXIS_FOCUS = 3,
  AXIS_COUNT
} PtzAxis;

/* ---- Speed presets ---- */
typedef enum {
  SPEED_SLOW = 0,
  SPEED_MEDIUM = 1,
  SPEED_FAST = 2,
  SPEED_COUNT
} PtzSpeed;

/* ---- User settings (persisted on the watch, partly configurable via Clay) ---- */
typedef struct {
  bool invert_pan;
  bool invert_tilt;
  uint8_t sensitivity;   /* 1..10, motion control gain */
  uint8_t deadzone;      /* 0..30, percent of full deflection ignored */
  PtzSpeed speed;        /* button/motion speed preset */
} PtzSettings;

typedef struct {
  char name[CAMERA_NAME_LEN];
  bool ptz;
} PtzCamera;

/* ---- Global app state ---- */
typedef struct {
  PtzCamera cameras[MAX_CAMERAS];
  uint8_t camera_count;
  int8_t selected_camera;     /* -1 = none */
  PtzStatus status;
  char status_text[STATUS_TEXT_LEN];
  PtzSettings settings;
} PtzAppState;

extern PtzAppState g_app;

/* Persist keys */
#define PERSIST_KEY_SETTINGS   1
#define PERSIST_KEY_LAST_CAM   2

void settings_load(void);
void settings_save(void);
uint8_t speed_percent(PtzSpeed speed);
const char *speed_label(PtzSpeed speed);
