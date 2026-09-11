#include <pebble.h>

#include "comm.h"
#include "dictation.h"
#include "model.h"
#include "reminders.h"
#include "status_window.h"

// Toggl Track for Pebble: start, stop and switch Toggl timers from the wrist.
// The watch only talks to the phone (src/pkjs), which talks to the Toggl API.

#ifndef PBL_PLATFORM_APLITE   // app glances need firmware 4.0+
static char s_glance_text[128];

static void prv_glance_reload(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1) {
    return;
  }
  const AppGlanceSlice slice = {
    .layout = {
      .icon = APP_GLANCE_SLICE_DEFAULT_ICON,
      .subtitle_template_string = (const char *)context,
    },
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
  };
  app_glance_add_slice(session, slice);
}

// Show the running entry in the launcher (app glance) after leaving the app.
static void prv_update_glance(void) {
  const TimerStatus *s = &model_get()->status;
  if (s->valid && s->running) {
    char what[DESC_LEN];
    strncpy(what, s->description[0] ? s->description
                : (s->project_name[0] ? s->project_name : "Timer"), sizeof(what) - 1);
    what[sizeof(what) - 1] = '\0';
    // Braces have a special meaning in glance templates.
    for (char *c = what; *c; c++) {
      if (*c == '{' || *c == '}') {
        *c = ' ';
      }
    }
    // The launcher keeps the elapsed time current on its own.
    snprintf(s_glance_text, sizeof(s_glance_text), "{time_since(%d)|format('%%aR')} · %s",
             (int)s->start_time, what);
  } else {
    snprintf(s_glance_text, sizeof(s_glance_text), "Kein Timer läuft");
  }
  app_glance_reload(prv_glance_reload, s_glance_text);
}
#else
static void prv_update_glance(void) {
  // App glances are not available on this platform.
}
#endif

static void prv_init(void) {
  model_load();
  comm_init();
#ifdef PBL_TOUCH
  // Let the project list scroll and select by touch (system gesture bridge).
  app_touch_navigation_enable(true);
#endif
  reminders_handle_launch();
  status_window_push();
}

static void prv_deinit(void) {
  window_stack_pop_all(false);
  status_window_destroy();
  dictation_deinit();
  comm_deinit();
  reminders_schedule();
  prv_update_glance();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
  return 0;
}
