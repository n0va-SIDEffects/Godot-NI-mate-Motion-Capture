/*
 * AppMessage transport between watch and phone.
 * Coalesces rapid MOVE updates so the Bluetooth link is never flooded.
 */
#pragma once

#include "ptz_app.h"

typedef void (*CommCamerasChangedCallback)(void);
typedef void (*CommStatusChangedCallback)(void);
typedef void (*CommSettingsChangedCallback)(void);

void comm_init(void);
void comm_deinit(void);

void comm_set_cameras_callback(CommCamerasChangedCallback cb);
void comm_set_status_callback(CommStatusChangedCallback cb);
void comm_set_settings_callback(CommSettingsChangedCallback cb);

/* Request the camera list from the bridge (via the phone). */
void comm_request_cameras(void);
void comm_request_refresh(void);

/*
 * Continuous movement. Speeds are -100..100.
 * Sending is rate-limited; the most recent values always win.
 * A change to all-zero is sent immediately (stop has priority).
 */
void comm_send_move(int8_t cam, int8_t pan, int8_t tilt, int8_t zoom);
void comm_send_stop(int8_t cam);
void comm_send_focus(int8_t cam, int8_t speed);
void comm_send_simple(PtzCommand cmd, int8_t cam);
void comm_send_preset(PtzCommand cmd, int8_t cam, uint8_t preset);

bool comm_is_connected(void);
