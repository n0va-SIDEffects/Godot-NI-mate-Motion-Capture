// Theremin fuer die Pebble Time 2 (Plattform "emery").
//
// Steuerung ueber den Beschleunigungssensor (Neigung des Handgelenks):
//   - Handgelenk nach links/rechts rollen  -> Tonhoehe (4 Oktaven, A2 .. A6)
//   - Uhr zu sich kippen / von sich weg    -> Lautstaerke
// Der Ton wird als roher PCM-Strom in Echtzeit erzeugt und ueber die
// Lautsprecher-API der Uhr ausgegeben.
//
// Tasten:
//   SELECT        Ton an / aus
//   SELECT lang   Nullpunkt neu kalibrieren (Uhr ~1 s ruhig halten)
//
// Beim Start wird die Haltung der ersten Sekunde automatisch zum Nullpunkt:
// Nullpunkt = mittlere Tonhoehe und mittlere Lautstaerke.
//   UP / DOWN     Wellenform wechseln (Sinus, Dreieck, Rechteck, Saegezahn)
//   BACK          App beenden
//
// Hinweis: Das Pebble-SDK liefert keine libm mit, deshalb ist alles in
// Festkomma-Arithmetik mit Tabellen geloest (siehe sine_table.h /
// semitone_table.h).

#include <pebble.h>
#include "sine_table.h"
#include "semitone_table.h"

// ---------------------------------------------------------------------------
// Einstellungen
// ---------------------------------------------------------------------------
#define SAMPLE_RATE       16000
#define PCM_FORMAT        SpeakerPcmFormat_16kHz_16bit
#define STREAM_VOLUME     100            // Lautstaerke des Streams (0-100)
#define TICK_MS           8              // Intervall der Audio-Pumpe
#define LEAD_MS           120            // Vorlauf, den wir dem Lautsprecher voraus sind (Latenz)
#define CHUNK_SAMPLES     256            // 16 ms Audio pro Block (512 Bytes, Vielfaches der 256-Byte-Bloecke der Firmware)
#define WRITES_PER_TICK   8              // max. Bloecke pro Tick (fuer das schnelle Auffuellen am Anfang)
#define CAL_SAMPLES       20             // Sensor-Callbacks fuer die Startkalibrierung (~0.8 s)

#define F_MIN_HZ          110            // A2 = unterer Rand des Tonbereichs
#define OCTAVES           4              // Bereich: A2 .. A6
#define SEMITONE_RANGE    (OCTAVES * 12)

#define TILT_PITCH_RANGE  600            // +/- milli-g Rollwinkel (rel. zum Nullpunkt) fuer den vollen Tonbereich
#define TILT_VOL_MIN      (-200)         // milli-g rel. zum Nullpunkt: von sich weg kippen -> stumm
#define TILT_VOL_MAX      250            // milli-g rel. zum Nullpunkt: zu sich kippen -> maximal (~15 Grad)

#define PERSIST_KEY_WAVE  1

typedef enum {
  WaveSine = 0,
  WaveTriangle,
  WaveSquare,
  WaveSaw,
  WaveCount
} Wave;

static const char *WAVE_NAMES[WaveCount] = { "Sinus", "Dreieck", "Rechteck", "Sägezahn" };
static const char *NOTE_NAMES[12] = { "A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#" };

// ---------------------------------------------------------------------------
// Zustand
// ---------------------------------------------------------------------------
static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_timer;

static bool s_playing = false;
static Wave s_wave = WaveSine;
static const char *s_status = "Ruhig halten...";

// Sensor (gefiltert, milli-g)
static int32_t s_fx = 0, s_fy = 0;
static int32_t s_cal_x = 0, s_cal_y = 0;   // Nullpunkt-Kalibrierung
static bool s_have_sample = false;
static uint8_t s_ui_div = 0;
static uint8_t s_cal_count = 0;           // laufende Startkalibrierung: gesammelte Callbacks
static int32_t s_cal_sum_x = 0, s_cal_sum_y = 0;
static bool s_calibrated = false;

// Synthese
static uint32_t s_phase = 0;        // 32-Bit Phasenakkumulator
static uint32_t s_phase_inc = 0;    // aktueller Phasenschritt pro Sample
static uint32_t s_target_inc = 0;   // Ziel-Phasenschritt (vom Sensor)
static int32_t  s_amp = 0;          // aktuelle Amplitude, Q16 (0 .. 65536)
static int32_t  s_target_amp = 0;   // Ziel-Amplitude, Q16
static uint32_t s_semis_q8 = 0;     // Halbtoene ueber F_MIN, Q8 (fuer die Anzeige)
static uint16_t s_freq_hz = F_MIN_HZ;

static int16_t  s_chunk[CHUNK_SAMPLES];
static uint16_t s_chunk_len = 0, s_chunk_pos = 0;   // in Bytes
static uint32_t s_samples_written = 0;
static uint32_t s_clock_ms = 0;          // bereinigte, monotone Stream-Uhr
static uint32_t s_last_raw_ms = 0;

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------
static uint32_t now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (uint32_t)s * 1000u + ms;
}

// time_ms() der Firmware liefert um Sekundengrenzen herum gelegentlich Werte, die
// um genau +/-1000 ms daneben liegen. Diese Uhr korrigiert solche Spruenge und
// laeuft nie rueckwaerts, sonst geraet die Audio-Taktung aus dem Tritt.
static uint32_t stream_clock_ms(void) {
  uint32_t raw = now_ms();
  int32_t dt = (int32_t)(raw - s_last_raw_ms);
  s_last_raw_ms = raw;
  if (dt > 900)  dt -= 1000;
  if (dt < -900) dt += 1000;
  if (dt < 0)    dt = 0;
  s_clock_ms += (uint32_t)dt;
  return s_clock_ms;
}

static int32_t clamp32(int32_t v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Frequenz in Q16 (Hz * 65536) aus Halbtoenen ueber F_MIN (Q8).
static uint32_t freq_q16_from_semis_q8(uint32_t semis_q8) {
  uint32_t semis = semis_q8 >> 8;
  uint32_t frac = semis_q8 & 0xFF;
  uint32_t oct = semis / 12;
  uint32_t k = semis % 12;
  uint32_t r0 = SEMITONE_RATIO_Q16[k];
  uint32_t r1 = SEMITONE_RATIO_Q16[k + 1];
  uint32_t ratio = r0 + (((r1 - r0) * frac) >> 8);      // Q16, linear interpoliert
  return (uint32_t)(F_MIN_HZ << oct) * ratio;           // passt in 32 Bit (max ~2.3e8)
}

// Phasenschritt = f / SAMPLE_RATE * 2^32. Mit f in Q16: f_q16 * 2^16 / 8000 = f_q16 * 8.192
static uint32_t phase_inc_from_freq_q16(uint32_t freq_q16) {
  return (uint32_t)(((uint64_t)freq_q16 * 536871ull) >> 16);   // 536871 / 65536 = 8.19201
}

static inline int8_t wave_sample(uint32_t phase, Wave w) {
  uint8_t p = phase >> 24;
  switch (w) {
    case WaveSquare:   return (p < 128) ? 90 : -90;   // Rechteck ist subjektiv lauter, etwas leiser
    case WaveTriangle: return (int8_t)((p < 128) ? (p * 2 - 128) : (383 - p * 2));
    case WaveSaw:      return (int8_t)(p - 128);
    case WaveSine:
    default:           return SINE_TABLE[p];
  }
}

// ---------------------------------------------------------------------------
// Sensor -> Zielwerte
// ---------------------------------------------------------------------------
static void update_targets(void) {
  int32_t roll = s_fx - s_cal_x;     // links/rechts rollen
  int32_t tilt = -(s_fy - s_cal_y);  // zu sich kippen = positiv

  // Tonhoehe: linear in Halbtoenen (= exponentiell in Hz), wie beim echten Theremin
  int32_t r = clamp32(roll, -TILT_PITCH_RANGE, TILT_PITCH_RANGE) + TILT_PITCH_RANGE;
  s_semis_q8 = (uint32_t)r * (SEMITONE_RANGE * 256) / (2 * TILT_PITCH_RANGE);
  if (s_semis_q8 > SEMITONE_RANGE * 256) s_semis_q8 = SEMITONE_RANGE * 256;
  uint32_t fq16 = freq_q16_from_semis_q8(s_semis_q8);
  s_freq_hz = (uint16_t)((fq16 + 32768) >> 16);
  s_target_inc = phase_inc_from_freq_q16(fq16);

  // Lautstaerke: 0..256 linear aus dem Kippwinkel, dann leicht gekruemmt
  // (Mittel aus linear und quadratisch): Nullpunkt-Haltung ~ -10 dB, zu sich kippen = voll
  int32_t t = clamp32(tilt, TILT_VOL_MIN, TILT_VOL_MAX) - TILT_VOL_MIN;
  t = t * 256 / (TILT_VOL_MAX - TILT_VOL_MIN);
  s_target_amp = (t * t + t * 256) / 2;   // 0 .. 65536
}

static void accel_handler(AccelData *data, uint32_t num_samples) {
  if (num_samples == 0) return;
  int32_t x = 0, y = 0;
  for (uint32_t i = 0; i < num_samples; i++) {
    x += data[i].x;
    y += data[i].y;
  }
  x /= (int32_t)num_samples;
  y /= (int32_t)num_samples;

  if (!s_have_sample) {
    s_fx = x;
    s_fy = y;
    s_have_sample = true;
  } else {
    s_fx += (x - s_fx) / 3;   // leichter Tiefpass gegen Zittern
    s_fy += (y - s_fy) / 3;
  }
  if (s_cal_count < CAL_SAMPLES) {
    // Startkalibrierung (oder nach langem SELECT): Haltung ueber ~0.8 s mitteln
    s_cal_sum_x += x;
    s_cal_sum_y += y;
    if (++s_cal_count == CAL_SAMPLES) {
      s_cal_x = s_cal_sum_x / CAL_SAMPLES;
      s_cal_y = s_cal_sum_y / CAL_SAMPLES;
      s_calibrated = true;
      s_status = s_playing ? "SELECT: Stop" : "SELECT: Start";
      vibes_short_pulse();
    }
  }
  update_targets();

  if (++s_ui_div >= 5) {     // Anzeige mit ~5 Hz aktualisieren
    s_ui_div = 0;
    layer_mark_dirty(s_canvas);
  }
}

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------
static void generate_chunk(void) {
  for (int i = 0; i < CHUNK_SAMPLES; i++) {
    // Sanftes Gleiten zu den Zielwerten (kein "Zipper"-Rauschen, kein Knacken)
    s_phase_inc += (uint32_t)(((int32_t)(s_target_inc - s_phase_inc)) >> 8);
    s_amp += (s_target_amp - s_amp) >> 7;
    s_phase += s_phase_inc;
    // Wellenform (8 Bit) * Amplitude (Q16) -> 16-Bit-Sample
    int32_t v = (int32_t)wave_sample(s_phase, s_wave) * s_amp;   // -128*65536 .. 127*65536
    s_chunk[i] = (int16_t)(v >> 8);
  }
  s_chunk_len = CHUNK_SAMPLES * sizeof(int16_t);
  s_chunk_pos = 0;
}

static void audio_tick(void *ctx) {
  s_timer = NULL;
  if (!s_playing) return;

  // Wir halten den Strom hoechstens LEAD_MS vor der Wiedergabe. Dadurch bleibt
  // die Latenz klein, egal wie gross der interne Puffer des Lautsprechers ist.
  uint32_t allowed = (stream_clock_ms() + LEAD_MS) * (SAMPLE_RATE / 1000);

  int budget = WRITES_PER_TICK;
  while (s_samples_written < allowed && budget-- > 0) {
    if (s_chunk_pos >= s_chunk_len) generate_chunk();
    uint32_t n = speaker_stream_write((const uint8_t *)s_chunk + s_chunk_pos, s_chunk_len - s_chunk_pos);
    if (n == 0) break;   // Geraetepuffer voll, naechster Tick
    s_chunk_pos += n;
    s_samples_written += n / sizeof(int16_t);
  }

  s_timer = app_timer_register(TICK_MS, audio_tick, NULL);
}

static void start_audio(void) {
  if (s_playing) return;
  if (!speaker_stream_open(PCM_FORMAT, STREAM_VOLUME)) {
    s_status = "Lautsprecher-Fehler";
    layer_mark_dirty(s_canvas);
    return;
  }
  s_playing = true;
  if (s_calibrated) s_status = "SELECT: Stop";
  s_last_raw_ms = now_ms();
  s_clock_ms = 0;
  s_samples_written = 0;
  s_chunk_len = s_chunk_pos = 0;
  s_amp = 0;                 // weich einblenden
  s_phase_inc = s_target_inc;
  audio_tick(NULL);
  layer_mark_dirty(s_canvas);
}

static void stop_audio(void) {
  if (!s_playing) return;
  s_playing = false;
  if (s_calibrated) s_status = "SELECT: Start";
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  speaker_stream_close();
  layer_mark_dirty(s_canvas);
}

// ---------------------------------------------------------------------------
// Anzeige
// ---------------------------------------------------------------------------
static void canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  const int16_t w = b.size.w;

  GColor accent = PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorBlack);
  GColor accent2 = PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Titel
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "THEREMIN", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, 2, w, 22), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Frequenz gross
  static char freq_buf[16];
  snprintf(freq_buf, sizeof(freq_buf), "%u Hz", (unsigned)s_freq_hz);
  graphics_context_set_text_color(ctx, s_playing ? accent : GColorBlack);
  graphics_draw_text(ctx, freq_buf, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                     GRect(0, 26, w, 48), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Notenname
  static char note_buf[8];
  uint32_t midi = 45 + ((s_semis_q8 + 128) >> 8);   // 45 = A2
  snprintf(note_buf, sizeof(note_buf), "%s%u", NOTE_NAMES[(midi - 45) % 12], (unsigned)(midi / 12) - 1);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, note_buf, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(0, 70, w, 32), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Tonhoehen-Balken (Marker wandert links/rechts)
  const int16_t px = 14, pw = w - 2 * px;
  const int16_t py = 126;
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "Tonhöhe: Handgelenk rollen", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, py - 18, w, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, GRect(px, py, pw, 14));
  int16_t mx = px + (int16_t)((s_semis_q8 * (uint32_t)(pw - 6)) / (SEMITONE_RANGE * 256));
  graphics_context_set_fill_color(ctx, accent);
  graphics_fill_rect(ctx, GRect(mx, py + 1, 6, 12), 0, GCornerNone);
  // Oktavmarken
  for (int o = 1; o < OCTAVES; o++) {
    int16_t ox = px + (int16_t)(pw * o / OCTAVES);
    graphics_draw_line(ctx, GPoint(ox, py + 14), GPoint(ox, py + 18));
  }

  // Lautstaerke-Balken (fuellt sich)
  const int16_t vy = 170;
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "Lautstärke: zu dir kippen", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, vy - 18, w, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_rect(ctx, GRect(px, vy, pw, 14));
  int16_t vw = (int16_t)(((int64_t)s_amp * (pw - 2)) >> 16);
  graphics_context_set_fill_color(ctx, accent2);
  graphics_fill_rect(ctx, GRect(px + 1, vy + 1, vw, 12), 0, GCornerNone);

  // Fusszeile: Wellenform + Status
  static char foot_buf[40];
  snprintf(foot_buf, sizeof(foot_buf), "%s   |   %s", WAVE_NAMES[s_wave], s_status);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, foot_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(0, b.size.h - 26, w, 22), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// ---------------------------------------------------------------------------
// Tasten
// ---------------------------------------------------------------------------
static void select_click(ClickRecognizerRef rec, void *ctx) {
  if (s_playing) stop_audio(); else start_audio();
}

static void select_long(ClickRecognizerRef rec, void *ctx) {
  // Kalibrierung neu starten: die Haltung der naechsten ~0.8 s wird zum Nullpunkt
  s_cal_count = 0;
  s_cal_sum_x = s_cal_sum_y = 0;
  s_status = "Ruhig halten...";
  layer_mark_dirty(s_canvas);
}

static void wave_step(int dir) {
  s_wave = (Wave)(((int)s_wave + dir + WaveCount) % WaveCount);
  persist_write_int(PERSIST_KEY_WAVE, s_wave);
  layer_mark_dirty(s_canvas);
}
static void up_click(ClickRecognizerRef rec, void *ctx)   { wave_step(-1); }
static void down_click(ClickRecognizerRef rec, void *ctx) { wave_step(+1); }

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, select_long, NULL);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 250, up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 250, down_click);
}

// ---------------------------------------------------------------------------
// Fenster / App
// ---------------------------------------------------------------------------
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  accel_data_service_subscribe(2, accel_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
}

static void window_unload(Window *window) {
  stop_audio();
  accel_data_service_unsubscribe();
  layer_destroy(s_canvas);
}

static void init(void) {
  if (persist_exists(PERSIST_KEY_WAVE)) {
    int32_t w = persist_read_int(PERSIST_KEY_WAVE);
    if (w >= 0 && w < WaveCount) s_wave = (Wave)w;
  }
  update_targets();

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
