/*
 * HELO Remote - Pebble Time 2 app to control and monitor an AJA HELO
 * (H.264 streaming/recording encoder) via its REST API.
 *
 * The watch never talks to the HELO directly: PebbleKit JS on the phone
 * (src/pkjs/index.js) polls the HELO and forwards status via AppMessage.
 * The watch sends single "CMD" values back to trigger actions.
 *
 * Buttons:
 *   UP      -> Record start / stop (stop asks for confirmation)
 *   SELECT  -> Refresh status now
 *   DOWN    -> Stream start / stop (stop asks for confirmation)
 *   BACK    -> Quit
 */
#include <pebble.h>

/* ---- Protocol (must match src/pkjs/index.js) ---------------------------- */
enum HeloCmd {
  CMD_REFRESH      = 0,
  CMD_REC_START    = 1,   /* eParamID_ReplicatorCommand = 1 */
  CMD_REC_STOP     = 2,   /* eParamID_ReplicatorCommand = 2 */
  CMD_STREAM_START = 3,   /* eParamID_ReplicatorCommand = 3 */
  CMD_STREAM_STOP  = 4,   /* eParamID_ReplicatorCommand = 4 */
};

/* eParamID_ReplicatorRecordState / eParamID_ReplicatorStreamState */
enum HeloState {
  HELO_STATE_UNKNOWN       = -1,
  HELO_STATE_UNINIT        = 0,
  HELO_STATE_IDLE          = 1,
  HELO_STATE_ACTIVE        = 2,   /* recording / streaming */
  HELO_STATE_FAILED_IDLE   = 3,
  HELO_STATE_FAILED_ACTIVE = 4,
  HELO_STATE_SHUTDOWN      = 5,
};

enum ConnState {
  CONN_UNKNOWN  = 0,   /* nothing heard from the phone yet */
  CONN_OK       = 1,
  CONN_OFFLINE  = 2,   /* HELO unreachable */
  CONN_AUTH     = 3,   /* HELO asks for a password we don't have / wrong */
  CONN_NOCONFIG = 4,   /* no IP configured in the settings page */
};

/* ---- State -------------------------------------------------------------- */
typedef struct {
  int  conn;
  int  rec_state;
  int  stream_state;
  int  media_pct;       /* -1 = unknown */
  int  temp_c;          /* -1000 = unknown */
  bool vibrate;
  char rec_name[20];
  char stream_name[20];
  char rec_dur[16];
  char stream_dur[16];
  char sys_name[32];
} HeloStatus;

#define CONFIRM_TIMEOUT_MS   4000
#define STALE_TIMEOUT_MS    20000
#define HINT_TIMEOUT_MS      3500

static Window     *s_window;
static Layer      *s_canvas;
static HeloStatus  s_status;
static char        s_hint[48];
static int         s_confirm_cmd;     /* 0 or CMD_*_STOP awaiting 2nd press */
static bool        s_busy;            /* command sent, waiting for status   */
static AppTimer   *s_confirm_timer;
static AppTimer   *s_stale_timer;
static AppTimer   *s_hint_timer;

static GFont s_font_small;
static GFont s_font_small_bold;
static GFont s_font_label;
static GFont s_font_digits;

static GPath *s_path_play;
static GPath *s_path_arrow;

static const GPathInfo PLAY_PATH_INFO = {
  .num_points = 3,
  .points = (GPoint[]) {{0, 0}, {10, 6}, {0, 12}}
};
static const GPathInfo ARROW_PATH_INFO = {
  .num_points = 3,
  .points = (GPoint[]) {{0, 0}, {6, 3}, {0, 6}}
};

/* ---- Small helpers ------------------------------------------------------ */
static bool state_is_active(int st) {
  return st == HELO_STATE_ACTIVE || st == HELO_STATE_FAILED_ACTIVE;
}

static bool state_is_failed(int st) {
  return st == HELO_STATE_FAILED_IDLE || st == HELO_STATE_FAILED_ACTIVE;
}

static void copy_str(char *dst, size_t dst_len, const char *src) {
  if (!src) { dst[0] = '\0'; return; }
  strncpy(dst, src, dst_len - 1);
  dst[dst_len - 1] = '\0';
}

static void hint_timeout_cb(void *data) {
  s_hint_timer = NULL;
  s_hint[0] = '\0';
  layer_mark_dirty(s_canvas);
}

static void set_hint(const char *text, bool sticky) {
  copy_str(s_hint, sizeof(s_hint), text);
  if (s_hint_timer) { app_timer_cancel(s_hint_timer); s_hint_timer = NULL; }
  if (!sticky && s_hint[0]) {
    s_hint_timer = app_timer_register(HINT_TIMEOUT_MS, hint_timeout_cb, NULL);
  }
  layer_mark_dirty(s_canvas);
}

static void confirm_timeout_cb(void *data) {
  s_confirm_timer = NULL;
  s_confirm_cmd = 0;
  set_hint("", false);
}

static void clear_confirm(void) {
  if (s_confirm_timer) { app_timer_cancel(s_confirm_timer); s_confirm_timer = NULL; }
  s_confirm_cmd = 0;
}

static void stale_timeout_cb(void *data) {
  s_stale_timer = NULL;
  if (s_status.conn == CONN_OK) {
    s_status.conn = CONN_UNKNOWN;
    set_hint("Keine Daten vom Telefon", true);
  }
}

static void arm_stale_timer(void) {
  if (s_stale_timer) app_timer_cancel(s_stale_timer);
  s_stale_timer = app_timer_register(STALE_TIMEOUT_MS, stale_timeout_cb, NULL);
}

/* ---- AppMessage --------------------------------------------------------- */
static void send_cmd(int cmd) {
  DictionaryIterator *iter;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if (res != APP_MSG_OK || !iter) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "outbox_begin failed: %d", (int) res);
    set_hint("Telefon beschaeftigt...", false);
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_CMD, cmd);
  dict_write_end(iter);
  res = app_message_outbox_send();
  if (res != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "outbox_send failed: %d", (int) res);
    set_hint("Senden fehlgeschlagen", false);
    return;
  }
  s_busy = (cmd != CMD_REFRESH);
  layer_mark_dirty(s_canvas);
}

static void notify_transition(int old_state, int new_state, bool is_rec) {
  if (!s_status.vibrate || old_state == new_state) return;
  if (old_state == HELO_STATE_UNKNOWN) return;      /* first status, stay quiet */
  if (!state_is_active(old_state) && state_is_active(new_state)) {
    vibes_short_pulse();
    set_hint(is_rec ? "Aufnahme laeuft" : "Stream laeuft", false);
  } else if (state_is_active(old_state) && !state_is_active(new_state)) {
    vibes_double_pulse();
    set_hint(is_rec ? "Aufnahme gestoppt" : "Stream gestoppt", false);
  } else if (state_is_failed(new_state)) {
    vibes_long_pulse();
  }
}

static void inbox_received_cb(DictionaryIterator *iter, void *context) {
  Tuple *t;
  int old_rec = s_status.rec_state;
  int old_stream = s_status.stream_state;
  bool got_status = false;

  if ((t = dict_find(iter, MESSAGE_KEY_CONN))) {
    s_status.conn = (int) t->value->int32;
    got_status = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_REC_STATE))) {
    s_status.rec_state = (int) t->value->int32;
    got_status = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_STREAM_STATE))) {
    s_status.stream_state = (int) t->value->int32;
    got_status = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_REC_NAME)))
    copy_str(s_status.rec_name, sizeof(s_status.rec_name), t->value->cstring);
  if ((t = dict_find(iter, MESSAGE_KEY_STREAM_NAME)))
    copy_str(s_status.stream_name, sizeof(s_status.stream_name), t->value->cstring);
  if ((t = dict_find(iter, MESSAGE_KEY_REC_DUR)))
    copy_str(s_status.rec_dur, sizeof(s_status.rec_dur), t->value->cstring);
  if ((t = dict_find(iter, MESSAGE_KEY_STREAM_DUR)))
    copy_str(s_status.stream_dur, sizeof(s_status.stream_dur), t->value->cstring);
  if ((t = dict_find(iter, MESSAGE_KEY_SYS_NAME)))
    copy_str(s_status.sys_name, sizeof(s_status.sys_name), t->value->cstring);
  if ((t = dict_find(iter, MESSAGE_KEY_MEDIA_PCT)))
    s_status.media_pct = (int) t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_TEMP_C)))
    s_status.temp_c = (int) t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_VIBRATE)))
    s_status.vibrate = t->value->int32 != 0;
  if ((t = dict_find(iter, MESSAGE_KEY_MESSAGE)) && t->value->cstring[0]) {
    set_hint(t->value->cstring, s_status.conn != CONN_OK);
  }

  if (got_status) {
    arm_stale_timer();
    if (s_status.conn == CONN_OK) {
      s_busy = false;
      /* a status update supersedes a sticky connection hint, but not a
       * pending stop confirmation ("Nochmal: ... STOPP") */
      if (s_hint[0] && !s_hint_timer && !s_confirm_cmd) s_hint[0] = '\0';
    } else {
      s_busy = false;
      clear_confirm();
    }
    notify_transition(old_rec, s_status.rec_state, true);
    notify_transition(old_stream, s_status.stream_state, false);
  }
  layer_mark_dirty(s_canvas);
}

static void inbox_dropped_cb(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "inbox dropped: %d", (int) reason);
}

static void outbox_failed_cb(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "outbox failed: %d", (int) reason);
  s_busy = false;
  set_hint("Telefon nicht erreichbar", false);
}

/* ---- Button handling ---------------------------------------------------- */
static void handle_toggle(bool is_rec) {
  const int state    = is_rec ? s_status.rec_state : s_status.stream_state;
  const int stop_cmd = is_rec ? CMD_REC_STOP : CMD_STREAM_STOP;
  const int start_cmd = is_rec ? CMD_REC_START : CMD_STREAM_START;

  if (s_status.conn == CONN_NOCONFIG) {
    set_hint("IP in App-Einstellungen setzen", false);
    vibes_short_pulse();
    return;
  }
  if (s_status.conn != CONN_OK) {
    set_hint("HELO nicht verbunden", false);
    vibes_short_pulse();
    return;
  }

  if (state_is_active(state)) {
    /* Stopping is destructive-ish: ask for a second press within 4 s */
    if (s_confirm_cmd == stop_cmd) {
      clear_confirm();
      set_hint(is_rec ? "Stoppe Aufnahme..." : "Stoppe Stream...", true);
      send_cmd(stop_cmd);
    } else {
      clear_confirm();
      s_confirm_cmd = stop_cmd;
      s_confirm_timer = app_timer_register(CONFIRM_TIMEOUT_MS, confirm_timeout_cb, NULL);
      set_hint(is_rec ? "Nochmal: Aufnahme STOPP" : "Nochmal: Stream STOPP", true);
    }
  } else {
    clear_confirm();
    set_hint(is_rec ? "Starte Aufnahme..." : "Starte Stream...", true);
    send_cmd(start_cmd);
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  handle_toggle(true);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  handle_toggle(false);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_confirm_cmd) {
    clear_confirm();
    set_hint("Abgebrochen", false);
    return;
  }
  set_hint("Aktualisiere...", false);
  send_cmd(CMD_REFRESH);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

/* ---- Drawing ------------------------------------------------------------ */
static GColor color_for_state(int st, GColor active) {
  if (state_is_failed(st))   return PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack);
  if (state_is_active(st))   return active;
  return PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite);
}

static const char *fallback_state_name(int st) {
  switch (st) {
    case HELO_STATE_UNINIT:        return "Init";
    case HELO_STATE_IDLE:          return "Bereit";
    case HELO_STATE_ACTIVE:        return "Aktiv";
    case HELO_STATE_FAILED_IDLE:
    case HELO_STATE_FAILED_ACTIVE: return "Fehler";
    case HELO_STATE_SHUTDOWN:      return "Aus";
    default:                       return "--";
  }
}

static void draw_card(GContext *ctx, GRect r, const char *label, int state,
                      const char *name, const char *dur, GColor active_color, bool big) {
  const bool have_data = s_status.conn == CONN_OK;
  GColor bg = have_data ? color_for_state(state, active_color)
                        : PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite);
  GColor fg = gcolor_legible_over(bg);
  const int pad = big ? 6 : 4;

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, r, big ? 6 : 4, GCornersAll);
#ifndef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_round_rect(ctx, r, big ? 6 : 4);
#endif

  graphics_context_set_text_color(ctx, fg);

  /* label top-left */
  GRect label_box = GRect(r.origin.x + pad, r.origin.y + (big ? 2 : 0), r.size.w - 2 * pad, 20);
  graphics_draw_text(ctx, label, s_font_small_bold, label_box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  /* state name top-right */
  const char *shown = (have_data && name && name[0]) ? name
                    : (have_data ? fallback_state_name(state) : "--");
  graphics_draw_text(ctx, shown, s_font_label, label_box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  /* duration, large, bottom */
  const int digits_h = big ? 30 : 24;
  GRect dur_box = GRect(r.origin.x + pad, r.origin.y + r.size.h - digits_h - (big ? 6 : 3),
                        r.size.w - 2 * pad, digits_h);
  const char *dur_shown = (have_data && dur && dur[0]) ? dur : "--:--:--";
  graphics_draw_text(ctx, dur_shown, s_font_digits, dur_box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  /* blinking-style marker when active */
  if (have_data && state_is_active(state)) {
    graphics_context_set_fill_color(ctx, fg);
    graphics_fill_circle(ctx, GPoint(r.origin.x + r.size.w - pad - 5,
                                     r.origin.y + r.size.h - pad - 6), big ? 5 : 4);
  }
}

static void draw_action_bar(GContext *ctx, GRect bar, bool big) {
  const bool have_data = s_status.conn == CONN_OK;
  const GColor bar_bg  = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack);
  const GColor glyph   = GColorWhite;
  const int cx = bar.origin.x + bar.size.w / 2;
  const int r  = big ? 8 : 6;

  graphics_context_set_fill_color(ctx, bar_bg);
  graphics_fill_rect(ctx, bar, 0, GCornerNone);

  /* --- UP: record start (dot) / stop (square) --- */
  int cy = bar.origin.y + bar.size.h / 6;
  if (have_data && state_is_active(s_status.rec_state)) {
    graphics_context_set_fill_color(ctx, glyph);
    graphics_fill_rect(ctx, GRect(cx - r + 1, cy - r + 1, 2 * r - 2, 2 * r - 2), 2, GCornersAll);
  } else {
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    graphics_fill_circle(ctx, GPoint(cx, cy), r);
    graphics_context_set_stroke_color(ctx, glyph);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_circle(ctx, GPoint(cx, cy), r);
  }

  /* --- SELECT: refresh (open arc + arrow head) --- */
  cy = bar.origin.y + bar.size.h / 2;
  graphics_context_set_stroke_color(ctx, glyph);
  graphics_context_set_stroke_width(ctx, 2);
  GRect arc = GRect(cx - r, cy - r, 2 * r, 2 * r);
  graphics_draw_arc(ctx, arc, GOvalScaleModeFitCircle,
                    DEG_TO_TRIGANGLE(30), DEG_TO_TRIGANGLE(330));
  graphics_context_set_fill_color(ctx, glyph);
  gpath_move_to(s_path_arrow, GPoint(cx + r - 4, cy - r - 3));
  gpath_draw_filled(ctx, s_path_arrow);

  /* --- DOWN: stream start (play) / stop (square) --- */
  cy = bar.origin.y + (bar.size.h * 5) / 6;
  if (have_data && state_is_active(s_status.stream_state)) {
    graphics_context_set_fill_color(ctx, glyph);
    graphics_fill_rect(ctx, GRect(cx - r + 1, cy - r + 1, 2 * r - 2, 2 * r - 2), 2, GCornersAll);
  } else {
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorWhite));
    gpath_move_to(s_path_play, GPoint(cx - 4, cy - 6));
    gpath_draw_filled(ctx, s_path_play);
  }
}

static void draw_footer(GContext *ctx, GRect f, bool big) {
  const bool have_data = s_status.conn == CONN_OK;
  const int pad = big ? 6 : 4;
  char buf[32];

  graphics_context_set_text_color(ctx, GColorBlack);

  /* media bar */
  const int bar_h = big ? 10 : 8;
  GRect bar = GRect(f.origin.x + pad, f.origin.y + 4, f.size.w - 2 * pad, bar_h);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, bar, 2);
  if (have_data && s_status.media_pct >= 0) {
    int pct = s_status.media_pct > 100 ? 100 : s_status.media_pct;
    int w = ((bar.size.w - 2) * pct) / 100;
    GColor fill = PBL_IF_COLOR_ELSE(pct < 10 ? GColorRed : (pct < 25 ? GColorOrange : GColorIslamicGreen),
                                    GColorBlack);
    graphics_context_set_fill_color(ctx, fill);
    if (w > 0) graphics_fill_rect(ctx, GRect(bar.origin.x + 1, bar.origin.y + 1, w, bar_h - 2), 1, GCornersAll);
    snprintf(buf, sizeof(buf), "Medien %d%% frei", pct);
  } else {
    snprintf(buf, sizeof(buf), "Medien --");
  }

  GRect line1 = GRect(f.origin.x + pad, bar.origin.y + bar_h, f.size.w - 2 * pad, 18);
  graphics_draw_text(ctx, buf, s_font_small, line1,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (have_data && s_status.temp_c > -1000) {
    snprintf(buf, sizeof(buf), "%d°C", s_status.temp_c);
    graphics_draw_text(ctx, buf, s_font_small, line1,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }

  /* hint / message line */
  const char *msg = s_hint;
  if (!msg[0]) {
    switch (s_status.conn) {
      case CONN_UNKNOWN:  msg = "Warte auf Telefon..."; break;
      case CONN_OFFLINE:  msg = "HELO nicht erreichbar"; break;
      case CONN_AUTH:     msg = "HELO: Passwort pruefen"; break;
      case CONN_NOCONFIG: msg = "IP in Einstellungen setzen"; break;
      default:            msg = s_busy ? "Sende Befehl..." : ""; break;
    }
  }
  GRect line2 = GRect(f.origin.x + pad, line1.origin.y + (big ? 18 : 15), f.size.w - 2 * pad, 20);
  if (s_confirm_cmd || s_busy) {
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkCandyAppleRed, GColorBlack));
    graphics_draw_text(ctx, msg, s_font_small_bold, line2,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  } else {
    graphics_draw_text(ctx, msg, s_font_small, line2,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  const bool big = b.size.w >= 200;          /* emery: 200x228, basalt/diorite: 144x168 */
  const int bar_w    = big ? 40 : 30;
  const int header_h = big ? 28 : 22;
  const int footer_h = big ? 64 : 50;
  const int gap      = big ? 4 : 3;
  const int content_w = b.size.w - bar_w;
  const int card_h = (b.size.h - header_h - footer_h - 3 * gap) / 2;

  /* background */
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  /* header */
  GRect header = GRect(0, 0, content_w, header_h);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, header, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  const char *title = s_status.sys_name[0] ? s_status.sys_name : "AJA HELO";
  graphics_draw_text(ctx, title, big ? s_font_label : s_font_small_bold,
                     GRect(6, big ? 2 : 0, content_w - 26, header_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  /* connection dot */
  GColor dot;
  switch (s_status.conn) {
    case CONN_OK:      dot = PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite); break;
    case CONN_AUTH:    dot = PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite); break;
    case CONN_OFFLINE: dot = PBL_IF_COLOR_ELSE(GColorRed, GColorWhite); break;
    default:           dot = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite); break;
  }
  graphics_context_set_fill_color(ctx, dot);
  graphics_fill_circle(ctx, GPoint(content_w - 12, header_h / 2), big ? 6 : 5);
#ifndef PBL_COLOR
  if (s_status.conn != CONN_OK) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, GPoint(content_w - 12, header_h / 2), big ? 3 : 2);
  }
#endif

  /* cards */
  int y = header_h + gap;
  GRect rec_card = GRect(gap, y, content_w - 2 * gap, card_h);
  draw_card(ctx, rec_card, "REC", s_status.rec_state, s_status.rec_name, s_status.rec_dur,
            PBL_IF_COLOR_ELSE(GColorRed, GColorBlack), big);
  y += card_h + gap;
  GRect stream_card = GRect(gap, y, content_w - 2 * gap, card_h);
  draw_card(ctx, stream_card, "STREAM", s_status.stream_state, s_status.stream_name,
            s_status.stream_dur, PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorBlack), big);
  y += card_h + gap;

  /* footer */
  draw_footer(ctx, GRect(0, y, content_w, b.size.h - y), big);

  /* action bar */
  draw_action_bar(ctx, GRect(content_w, 0, bar_w, b.size.h), big);
}

/* ---- Window lifecycle --------------------------------------------------- */
static void initial_refresh_cb(void *data) {
  send_cmd(CMD_REFRESH);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

static void init(void) {
  s_status.conn         = CONN_UNKNOWN;
  s_status.rec_state    = HELO_STATE_UNKNOWN;
  s_status.stream_state = HELO_STATE_UNKNOWN;
  s_status.media_pct    = -1;
  s_status.temp_c       = -1000;
  s_status.vibrate      = true;

  s_font_small      = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_font_small_bold = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  s_font_label      = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_font_digits     = fonts_get_system_font(FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM);

  s_path_play  = gpath_create(&PLAY_PATH_INFO);
  s_path_arrow = gpath_create(&ARROW_PATH_INFO);

  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  app_message_register_inbox_received(inbox_received_cb);
  app_message_register_inbox_dropped(inbox_dropped_cb);
  app_message_register_outbox_failed(outbox_failed_cb);
  app_message_open(512, 64);

  window_stack_push(s_window, true);

  /* Ask the phone for a status once PebbleKit JS had a moment to start. */
  app_timer_register(1500, initial_refresh_cb, NULL);
  arm_stale_timer();
}

static void deinit(void) {
  if (s_confirm_timer) app_timer_cancel(s_confirm_timer);
  if (s_stale_timer)   app_timer_cancel(s_stale_timer);
  if (s_hint_timer)    app_timer_cancel(s_hint_timer);
  gpath_destroy(s_path_play);
  gpath_destroy(s_path_arrow);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
