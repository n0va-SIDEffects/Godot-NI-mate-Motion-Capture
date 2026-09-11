#include "reminders.h"
#include "i18n.h"
#include "model.h"

// Apps are suspended when closed, so reminders go through the firmware's
// wakeup service: on exit we schedule at most one wakeup that matches the
// last known state; on a wakeup launch we vibrate and leave a sticky hint.

enum { WAKEUP_RUNNING = 1, WAKEUP_NO_TIMER = 2 };

// Next occurrence of `hour`:00 local time strictly after `now`.
static time_t prv_next_hour(time_t now, int hour) {
  struct tm lt = *localtime(&now);
  lt.tm_hour = hour;
  lt.tm_min = 0;
  lt.tm_sec = 0;
  time_t t = mktime(&lt);
  while (t <= now) {
    t += 24 * 3600;
  }
  return t;
}

static time_t prv_next_weekday_hour(time_t now, int hour) {
  time_t t = prv_next_hour(now, hour);
  for (int i = 0; i < 3; i++) {
    const struct tm *w = localtime(&t);
    if (w->tm_wday != 0 && w->tm_wday != 6) {
      break;
    }
    t += 24 * 3600;
  }
  return t;
}

static void prv_schedule(time_t when, int32_t cookie) {
  // Another app may own that minute; nudge forward a few times.
  for (int attempt = 0; attempt < 5; attempt++) {
    WakeupId id = wakeup_schedule(when, cookie, false);
    if (id >= 0) {
      APP_LOG(APP_LOG_LEVEL_INFO, "Wakeup %d scheduled in %d min", (int)cookie, (int)((when - time(NULL)) / 60));
      return;
    }
    if (id != E_RANGE) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "wakeup_schedule failed: %d", (int)id);
      return;
    }
    when += 60;
  }
}

void reminders_handle_launch(void) {
  if (launch_reason() != APP_LAUNCH_WAKEUP) {
    return;
  }
  WakeupId id;
  int32_t cookie = 0;
  if (!wakeup_get_launch_event(&id, &cookie)) {
    return;
  }
  vibes_double_pulse();
  light_enable_interaction();
  if (cookie == WAKEUP_RUNNING) {
    model_set_hint(STR(S_HINT_STILL_RUNNING), HINT_STILL_RUNNING);
  } else {
    model_set_hint(STR(S_HINT_NO_TIMER), HINT_NO_TIMER);
  }
}

void reminders_schedule(void) {
  const AppModel *m = model_get();
  const TimerStatus *s = &m->status;
  const ReminderConfig *c = &m->config;
  const time_t now = time(NULL);

  wakeup_cancel_all();

  if (s->valid && s->running) {
    if (c->flags & REMIND_RUNNING) {
      time_t by_duration = s->start_time + (time_t)c->max_hours * 3600;
      time_t by_clock = prv_next_hour(now, c->late_hour);
      time_t when = by_duration < by_clock ? by_duration : by_clock;
      if (when < now + 60) {
        when = now + 60;
      }
      prv_schedule(when, WAKEUP_RUNNING);
    }
  } else if (c->flags & REMIND_NO_TIMER) {
    prv_schedule(prv_next_weekday_hour(now, c->start_hour), WAKEUP_NO_TIMER);
  }
}
