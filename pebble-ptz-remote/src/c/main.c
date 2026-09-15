#include <pebble.h>
#include "ptz.h"
#include "settings.h"
#include "comm.h"
#include "preview.h"
#include "win_remote.h"

static void init(void) {
  settings_load();
  preview_init();
  comm_init();
  win_remote_push();
}

static void deinit(void) {
  // comm_deinit schickt noch einen Stopp hinaus. Eine Kamera, die weiterfaehrt,
  // weil jemand die App geschlossen hat, waere das schlechteste Ergebnis
  // dieser App.
  comm_deinit();
  preview_deinit();
  settings_save();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
