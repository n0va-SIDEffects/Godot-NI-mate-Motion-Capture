/*
 * NDI PTZ Remote for Pebble
 *
 * Remote-controls NDI PTZ cameras from the wrist. The watch talks to
 * PebbleKit JS on the phone, which talks HTTP to the Python bridge
 * (bridge/ndi_ptz_bridge.py) that drives the cameras via the NDI SDK.
 */
#include "ptz_app.h"
#include "comm.h"
#include "motion.h"
#include "camera_menu.h"
#include "control_window.h"

PtzAppState g_app;

/* ---- settings ---- */

static const PtzSettings s_default_settings = {
  .invert_pan = false,
  .invert_tilt = false,
  .sensitivity = 5,
  .deadzone = 8,
  .speed = SPEED_MEDIUM,
};

void settings_load(void) {
  g_app.settings = s_default_settings;
  if (persist_exists(PERSIST_KEY_SETTINGS)) {
    PtzSettings s;
    if (persist_read_data(PERSIST_KEY_SETTINGS, &s, sizeof(s)) == (int)sizeof(s)) {
      g_app.settings = s;
    }
  }
  if (g_app.settings.sensitivity < 1 || g_app.settings.sensitivity > 10) g_app.settings.sensitivity = 5;
  if (g_app.settings.deadzone > 30) g_app.settings.deadzone = 8;
  if (g_app.settings.speed >= SPEED_COUNT) g_app.settings.speed = SPEED_MEDIUM;
  g_app.selected_camera = -1;
}

void settings_save(void) {
  persist_write_data(PERSIST_KEY_SETTINGS, &g_app.settings, sizeof(g_app.settings));
}

uint8_t speed_percent(PtzSpeed speed) {
  switch (speed) {
    case SPEED_SLOW: return 30;
    case SPEED_FAST: return 100;
    default: return 60;
  }
}

const char *speed_label(PtzSpeed speed) {
  switch (speed) {
    case SPEED_SLOW: return "Langsam";
    case SPEED_FAST: return "Schnell";
    default: return "Mittel";
  }
}

/* ---- comm callbacks ---- */

static void prv_cameras_changed(void) {
  camera_menu_reload();
  control_window_refresh();
}

static void prv_status_changed(void) {
  camera_menu_reload();
  control_window_refresh();
}

static void prv_settings_changed(void) {
  control_window_refresh();
}

/* ---- app lifecycle ---- */

static void prv_init(void) {
  memset(&g_app, 0, sizeof(g_app));
  settings_load();
  g_app.status = STATUS_LOADING;
  strncpy(g_app.status_text, "Suche Kameras...", STATUS_TEXT_LEN - 1);

  motion_init();
  comm_init();
  comm_set_cameras_callback(prv_cameras_changed);
  comm_set_status_callback(prv_status_changed);
  comm_set_settings_callback(prv_settings_changed);

  camera_menu_push();
}

static void prv_deinit(void) {
  motion_deinit();
  comm_deinit();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
  return 0;
}
