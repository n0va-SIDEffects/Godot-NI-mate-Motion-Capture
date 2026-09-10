#pragma once
#include "ptz_app.h"

/* Push the PTZ control window for g_app.selected_camera. */
void control_window_push(void);
/* Re-render (settings/status changed). Safe to call when not shown. */
void control_window_refresh(void);

/* Used by the options menu. */
bool control_window_motion_mode(void);
void control_window_set_motion_mode(bool on);
