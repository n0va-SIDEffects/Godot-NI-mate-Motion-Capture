#include "control.h"
#include "bclock.h"

static CtrlOut s_out;
static CtrlStats s_st;
static CtrlProfile s_profile = CtrlFinger;
static bool s_invert = true;        // Standard: ziehen = steigen, wie am Knueppel

static int16_t s_anchor_x, s_anchor_y;
static bool s_down;
static uint32_t s_last_ev_ms;
static uint32_t s_rate_t0, s_rate_n0;

static bool s_btn_up, s_btn_down, s_btn_select;
static int32_t s_btn_roll;          // -256..256, faellt zur Mitte zurueck

// Quadratische Kurve: feine Korrekturen in der Mitte, voller Ausschlag am Rand.
static int32_t prv_curve(int32_t px) {
  int32_t a = px < 0 ? -px : px;
  if (a <= STICK_DEAD_PX) return 0;
  a -= STICK_DEAD_PX;
  const int32_t range = STICK_SAT_PX - STICK_DEAD_PX;
  if (a > range) a = range;
  int32_t lin = (a * 256) / range;              // 0..256
  int32_t out = (lin * lin) >> 8;               // quadratisch
  return px < 0 ? -out : out;
}

static void prv_touch(const TouchEvent *e, void *ctx) {
  const uint32_t now = bclock_now_ms();
  s_st.events++;
  if (s_last_ev_ms) {
    const uint32_t d = now - s_last_ev_ms;
    if (d > 0 && (s_st.min_interval_ms == 0 || d < s_st.min_interval_ms)) {
      s_st.min_interval_ms = d;
    }
  }
  s_last_ev_ms = now;

  switch (e->type) {
    case TouchEvent_Touchdown:
      s_down = true;
      s_anchor_x = e->x;
      s_anchor_y = e->y;
      s_st.touchdowns++;
      s_st.dx = 0;
      s_st.dy = 0;
      break;
    case TouchEvent_Liftoff:
      s_down = false;
      s_st.dx = 0;
      s_st.dy = 0;
      break;
    case TouchEvent_PositionUpdate:
      if (!s_down) {
        // Ohne vorheriges Touchdown wird der erste Punkt zum Anker.
        s_down = true;
        s_anchor_x = e->x;
        s_anchor_y = e->y;
        s_st.touchdowns++;
      }
      s_st.dx = (int16_t)(e->x - s_anchor_x);
      s_st.dy = (int16_t)(e->y - s_anchor_y);
      break;
  }
  s_st.down = s_down;
}

void control_init(Window *window) {
  s_out = (CtrlOut){ 0 };
  s_st = (CtrlStats){ 0 };
  s_down = false;
  s_last_ev_ms = 0;
  s_rate_t0 = bclock_now_ms();
  s_rate_n0 = 0;
  s_btn_up = s_btn_down = s_btn_select = false;
  s_btn_roll = 0;
  window_set_touch_bridge_disabled(window, true);
  touch_service_subscribe(prv_touch, NULL);
  s_st.available = touch_service_is_enabled();
  if (!s_st.available) {
    s_profile = CtrlButton;      // Touch systemweit aus: Tasten uebernehmen
  }
}

void control_deinit(void) {
  touch_service_unsubscribe();
}

void control_tick(uint32_t now) {
  // Das Steuerprofil wird bei jedem Tick geprueft, damit ein systemweites
  // Abschalten von Touch mitten im Lauf auffaengt statt den Flug einzufrieren.
  const bool avail = touch_service_is_enabled();
  if (s_st.available && !avail && s_profile == CtrlFinger) {
    s_profile = CtrlButton;
  }
  s_st.available = avail;

  const uint32_t el = now - s_rate_t0;
  if (el >= 1000) {
    s_st.ev_rate_x10 = ((s_st.events - s_rate_n0) * 10000) / el;
    s_rate_n0 = s_st.events;
    s_rate_t0 = now;
  }

  if (s_profile == CtrlFinger) {
    if (s_down) {
      s_out.roll_cmd = prv_curve(s_st.dx);
      int32_t c = prv_curve(s_st.dy);
      // Bildschirm-y waechst nach unten. Ohne Umkehr heisst ziehen nach oben
      // steigen, und genau dann wandert der Finger vor den Bodenschatten.
      s_out.climb_cmd = s_invert ? c : -c;
      if (s_btn_select) {
        s_out.roll_cmd = (s_out.roll_cmd * PRECISION_PCT) / 100;
        s_out.climb_cmd = (s_out.climb_cmd * PRECISION_PCT) / 100;
      }
    } else {
      s_out.roll_cmd = 0;          // loslassen = Neutrallage
      s_out.climb_cmd = 0;
    }
    s_out.precision = s_btn_select;
    s_out.climb_held = false;
  } else {
    // Tasten: Roll laeuft mit gedrueckter Taste auf, faellt sonst zur Mitte.
    if (s_btn_up && !s_btn_down) {
      s_btn_roll -= BTN_ROLL_STEP / 4;
    } else if (s_btn_down && !s_btn_up) {
      s_btn_roll += BTN_ROLL_STEP / 4;
    } else {
      s_btn_roll -= s_btn_roll / 6;
    }
    if (s_btn_roll > 256) s_btn_roll = 256;
    if (s_btn_roll < -256) s_btn_roll = -256;
    s_out.roll_cmd = s_btn_roll;
    s_out.climb_cmd = 0;
    s_out.climb_held = s_btn_select;
    s_out.precision = false;
  }
}

void control_set_profile(CtrlProfile p) {
  s_profile = p;
  s_btn_roll = 0;
  s_out.roll_cmd = 0;
  s_out.climb_cmd = 0;
  s_out.climb_held = false;
}

CtrlProfile control_profile(void) { return s_profile; }

void control_toggle_profile(void) {
  control_set_profile(s_profile == CtrlFinger ? CtrlButton : CtrlFinger);
}

void control_set_pitch_invert(bool on) { s_invert = on; }
bool control_pitch_invert(void) { return s_invert; }

void control_button_up(bool pressed) { s_btn_up = pressed; }
void control_button_down(bool pressed) { s_btn_down = pressed; }
void control_button_select(bool pressed) { s_btn_select = pressed; }

// Trimmung ist eine bleibende Sollsteigrate, keine Taste zum Halten: der
// Spieler stellt sie einmal ein und der Gleiter behaelt sie, ohne dass der
// Finger den Stick loslassen muss. Genau das ist ihr Zweck.
void control_trim(int32_t delta8) {
  s_out.trim8 += delta8;
  if (s_out.trim8 > TRIM_MAX) s_out.trim8 = TRIM_MAX;
  if (s_out.trim8 < -TRIM_MAX) s_out.trim8 = -TRIM_MAX;
}

int32_t control_trim8(void) { return s_out.trim8; }

const CtrlOut *control_out(void) { return &s_out; }
const CtrlStats *control_stats(void) { return &s_st; }
