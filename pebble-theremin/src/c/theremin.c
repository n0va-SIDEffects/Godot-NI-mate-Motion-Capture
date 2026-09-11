// Theremin fuer die Pebble Time 2 (Plattform "emery").
//
// Tonhoehe und Lautstaerke werden ueber Sensoren gesteuert, die Zuordnung ist
// im Einstellungsmenue waehlbar:
//   Heben/Senken   Beschleunigungssensor, Hand heben und senken (Kippen um die Laengsachse des Arms)
//   Drehen         Beschleunigungssensor, Handgelenk drehen (Uhr zu sich / von sich weg)
//   Kompass        Magnetkompass, Arm nach links / rechts schwenken (Blickrichtung der Uhr)
// Der Ton wird als roher PCM-Strom (16 kHz, 16 Bit) in Echtzeit erzeugt und
// ueber die Lautsprecher-API der Uhr ausgegeben.
//
// Tasten (Hauptbildschirm):
//   SELECT        Ton an / aus
//   SELECT lang   Einstellungen
//   UP / DOWN     Wellenform wechseln
//   UP lang       Nullpunkt neu kalibrieren (Uhr ~1 s ruhig halten)
//   BACK          App beenden
//
// Beim Start wird die Haltung der ersten Sekunde automatisch zum Nullpunkt.
//
// Das Pebble-SDK liefert keine libm, deshalb ist alles in Festkomma-Arithmetik
// mit Tabellen geloest (wavetables.h, semitone_table.h).

#include <pebble.h>
#include "wavetables.h"
#include "semitone_table.h"

// ---------------------------------------------------------------------------
// Feste Parameter
// ---------------------------------------------------------------------------
#define SAMPLE_RATE       16000
#define PCM_FORMAT        SpeakerPcmFormat_16kHz_16bit
#define TICK_MS           8              // Intervall der Audio-Pumpe
#define LEAD_MS           120            // Vorlauf vor der Wiedergabe (Latenz)
#define CHUNK_SAMPLES     256            // 16 ms Audio pro Block (512 Bytes)
#define WRITES_PER_TICK   8
#define CAL_SAMPLES       20             // Sensor-Callbacks fuer die Kalibrierung (~0.8 s)

#define PERSIST_KEY_SETTINGS  10
#define SETTINGS_VERSION      2

// ---------------------------------------------------------------------------
// Einstellungen
// ---------------------------------------------------------------------------
typedef enum { AxisLift = 0, AxisRoll, AxisCompass, AxisFixed, AxisCount } Axis;
typedef enum { WaveSine = 0, WaveTriangle, WaveSquare, WaveSaw, WaveCount } Wave;
typedef enum { ScaleFree = 0, ScaleChromatic, ScaleMajor, ScaleMinor, ScalePentatonic, ScaleCount } Scale;

typedef struct {
  uint8_t version;
  uint8_t wave;
  uint8_t pitch_axis;    // Axis (ohne AxisFixed)
  uint8_t vol_axis;      // Axis
  uint8_t invert_pitch;  // 0/1
  uint8_t invert_vol;    // 0/1
  uint8_t root_idx;      // Index in ROOT_MIDI
  uint8_t octaves;       // 1..4
  uint8_t scale;         // Scale
  uint8_t pitch_sens;    // 0 fein, 1 mittel, 2 grob
  uint8_t vol_sens;      // 0 fein, 1 mittel, 2 grob
  uint8_t glide;         // 0 kurz, 1 mittel, 2 lang
  uint8_t volume_idx;    // Index in VOLUME_LEVELS
} Settings;

static Settings s_set;

static const Settings DEFAULT_SETTINGS = {
  .version = SETTINGS_VERSION, .wave = WaveSine,
  .pitch_axis = AxisLift, .vol_axis = AxisCompass,
  .invert_pitch = 0, .invert_vol = 0,
  .root_idx = 1, .octaves = 4, .scale = ScaleFree,
  .pitch_sens = 1, .vol_sens = 1, .glide = 1, .volume_idx = 2,
};

static const char *WAVE_NAMES[WaveCount]   = { "Sinus", "Dreieck", "Rechteck", "Sägezahn" };
static const char *AXIS_NAMES[AxisCount]   = { "Heben/Senken", "Drehen", "Kompass", "Immer voll" };
static const char *SCALE_NAMES[ScaleCount] = { "Frei", "Chromatisch", "Dur", "Moll", "Pentatonik" };
static const char *SENS_NAMES[3]           = { "Fein", "Mittel", "Grob" };
static const char *GLIDE_NAMES[3]          = { "Kurz", "Mittel", "Lang" };
static const char *YESNO_NAMES[2]          = { "Nein", "Ja" };

static const uint8_t ROOT_MIDI[]           = { 36, 45, 48, 57 };          // C2, A2, C3, A3
static const char *ROOT_NAMES[]            = { "C2", "A2", "C3", "A3" };
#define ROOT_COUNT 4
static const uint8_t VOLUME_LEVELS[]       = { 60, 80, 100 };
static const char *VOLUME_NAMES[]          = { "60 %", "80 %", "100 %" };
#define VOLUME_COUNT 3

// Empfindlichkeit: Sensorweg fuer den vollen Bereich
static const int16_t PITCH_RANGE_MG[3]     = { 800, 600, 400 };   // milli-g, +/- um den Nullpunkt
static const int16_t PITCH_RANGE_DEG[3]    = { 60, 40, 25 };      // Grad, +/- um den Nullpunkt (Kompass)
static const int16_t VOL_RANGE_MG[3]       = { 350, 250, 170 };   // milli-g bis "voll"
static const int16_t VOL_RANGE_DEG[3]      = { 45, 30, 20 };      // Grad bis "voll" (Kompass)
static const uint8_t GLIDE_SHIFT[3]        = { 6, 8, 10 };        // Glaettung pro Sample (2^n)

// Tonleitern als 12-Bit-Masken relativ zum Grundton (Bit 0 = Grundton)
static const uint16_t SCALE_MASKS[ScaleCount] = {
  0x0FFF,                 // frei (unbenutzt)
  0x0FFF,                 // chromatisch
  0x0AB5,                 // Dur:        0 2 4 5 7 9 11
  0x05AD,                 // Moll:       0 2 3 5 7 8 10
  0x0295,                 // Pentatonik: 0 2 4 7 9
};
static const char *NOTE_NAMES[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// ---------------------------------------------------------------------------
// Zustand
// ---------------------------------------------------------------------------
static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_timer;
static Window *s_menu_window;
static MenuLayer *s_menu;

static bool s_playing = false;
static const char *s_status = "Ruhig halten...";

// Sensoren (gefiltert)
static int32_t s_fx = 0, s_fy = 0;           // Beschleunigung, milli-g
static int32_t s_heading = 0;                // Kompass, im Uhrzeigersinn, TRIG-Einheiten (0..65535)
static bool s_have_sample = false;
static bool s_compass_on = false;
static CompassStatus s_compass_status = CompassStatusDataInvalid;
static bool s_have_heading = false;

// Kalibrierung
static int32_t s_cal_x = 0, s_cal_y = 0, s_cal_heading = 0;
static uint8_t s_cal_count = CAL_SAMPLES;    // < CAL_SAMPLES: Kalibrierung laeuft
static int32_t s_cal_sum_x = 0, s_cal_sum_y = 0;
static bool s_calibrated = false;
static uint8_t s_ui_div = 0;

// Synthese
static uint32_t s_phase = 0;
static uint32_t s_phase_inc = 0, s_target_inc = 0;
static int32_t  s_amp = 0, s_target_amp = 0;     // Q16
static uint32_t s_semis_q8 = 0;                  // Halbtoene ueber dem Grundton, Q8 (Anzeige)
static uint16_t s_freq_hz = 440;
static uint8_t  s_band = 0;
static uint8_t  s_glide_shift = 8;

static int16_t  s_chunk[CHUNK_SAMPLES];
static uint16_t s_chunk_len = 0, s_chunk_pos = 0;   // Bytes
static uint32_t s_samples_written = 0;
static uint32_t s_clock_ms = 0, s_last_raw_ms = 0;

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------
static uint32_t now_ms(void) {
  time_t s; uint16_t ms;
  time_ms(&s, &ms);
  return (uint32_t)s * 1000u + ms;
}

// time_ms() liefert um Sekundengrenzen gelegentlich Werte, die um genau +/-1000 ms
// daneben liegen. Diese Uhr korrigiert das und laeuft nie rueckwaerts.
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

// Winkeldifferenz in TRIG-Einheiten auf -32768..32767 falten
static int32_t wrap_angle(int32_t a) {
  a &= 0xFFFF;
  return a >= 0x8000 ? a - 0x10000 : a;
}

static uint8_t semitone_range(void) { return s_set.octaves * 12; }

// Frequenz in Q16 aus absoluten Halbtoenen ueber A1 (MIDI 33), Q8
static uint32_t freq_q16_from_abs_semis_q8(uint32_t abs_q8) {
  uint32_t semis = abs_q8 >> 8, frac = abs_q8 & 0xFF;
  uint32_t oct = semis / 12, k = semis % 12;
  uint32_t r0 = SEMITONE_RATIO_Q16[k], r1 = SEMITONE_RATIO_Q16[k + 1];
  uint32_t ratio = r0 + (((r1 - r0) * frac) >> 8);
  return (uint32_t)(55 << oct) * ratio;      // A1 = 55 Hz; max A7 = 3520 Hz passt in 32 Bit
}

// Phasenschritt = f / 16000 * 2^32; mit f in Q16: f_q16 * 2^16 / 16000 = f_q16 * 4.096
static uint32_t phase_inc_from_freq_q16(uint32_t freq_q16) {
  return (uint32_t)(((uint64_t)freq_q16 * 268435ull) >> 16);   // 268435 / 65536 = 4.09600
}

static uint8_t band_for_freq(uint16_t hz) {
  for (uint8_t b = 0; b < WAVE_BANDS - 1; b++) {
    if (hz < WAVE_BAND_LIMIT_HZ[b]) return b;
  }
  return WAVE_BANDS - 1;
}

// ---------------------------------------------------------------------------
// Sensorachsen -> normierte Werte
// ---------------------------------------------------------------------------
// Liefert die Auslenkung einer Achse relativ zum Nullpunkt, normiert auf
// +/-1000 fuer den eingestellten Bereich.
static int32_t axis_value(Axis axis, int16_t range_mg, int16_t range_deg) {
  switch (axis) {
    case AxisLift:  return (s_fx - s_cal_x) * 1000 / range_mg;
    case AxisRoll:  return -(s_fy - s_cal_y) * 1000 / range_mg;
    case AxisCompass: {
      if (!s_have_heading || s_compass_status == CompassStatusDataInvalid) return 0;
      int32_t d = wrap_angle(s_heading - s_cal_heading);       // rechts = positiv
      return d * 1000 / DEG_TO_TRIGANGLE(range_deg);
    }
    case AxisFixed:
    default:        return 1000;
  }
}

static uint32_t quantize_semis_q8(uint32_t semis_q8) {
  if (s_set.scale == ScaleFree) return semis_q8;
  uint16_t mask = SCALE_MASKS[s_set.scale];
  int32_t n = (int32_t)((semis_q8 + 128) >> 8);
  int32_t range = semitone_range();
  for (int32_t d = 0; d <= 6; d++) {
    if (n + d <= range && (mask & (1 << ((n + d) % 12)))) return (uint32_t)(n + d) << 8;
    if (n - d >= 0     && (mask & (1 << ((n - d) % 12)))) return (uint32_t)(n - d) << 8;
  }
  return semis_q8;
}

static void update_targets(void) {
  // Tonhoehe: linear in Halbtoenen (= exponentiell in Hz), wie beim echten Theremin
  int32_t pv = axis_value((Axis)s_set.pitch_axis, PITCH_RANGE_MG[s_set.pitch_sens], PITCH_RANGE_DEG[s_set.pitch_sens]);
  if (s_set.invert_pitch) pv = -pv;
  pv = clamp32(pv, -1000, 1000) + 1000;                       // 0..2000
  uint32_t range_q8 = (uint32_t)semitone_range() * 256;
  uint32_t semis_q8 = (uint32_t)pv * range_q8 / 2000;
  semis_q8 = quantize_semis_q8(semis_q8);
  if (semis_q8 > range_q8) semis_q8 = range_q8;
  s_semis_q8 = semis_q8;

  uint32_t abs_q8 = (uint32_t)(ROOT_MIDI[s_set.root_idx] - 33) * 256 + semis_q8;
  uint32_t fq16 = freq_q16_from_abs_semis_q8(abs_q8);
  s_freq_hz = (uint16_t)((fq16 + 32768) >> 16);
  s_band = band_for_freq(s_freq_hz);
  s_target_inc = phase_inc_from_freq_q16(fq16);

  // Lautstaerke: Nullpunkt ~ ein Drittel, positiv = lauter, -800 = stumm
  int32_t vv = axis_value((Axis)s_set.vol_axis, VOL_RANGE_MG[s_set.vol_sens], VOL_RANGE_DEG[s_set.vol_sens]);
  if (s_set.invert_vol) vv = -vv;
  int32_t t = (clamp32(vv, -800, 1000) + 800) * 256 / 1800;   // 0..256
  s_target_amp = (t * t + t * 256) / 2;                       // 0..65536, leicht gekruemmt
}

static void finish_calibration(void) {
  s_cal_x = s_cal_sum_x / CAL_SAMPLES;
  s_cal_y = s_cal_sum_y / CAL_SAMPLES;
  s_cal_heading = s_heading;
  s_calibrated = true;
  s_status = s_playing ? "SELECT: Stop" : "SELECT: Start";
  vibes_short_pulse();
}

static void start_calibration(void) {
  s_cal_count = 0;
  s_cal_sum_x = s_cal_sum_y = 0;
  s_status = "Ruhig halten...";
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void accel_handler(AccelData *data, uint32_t num_samples) {
  if (num_samples == 0) return;
  int32_t x = 0, y = 0;
  for (uint32_t i = 0; i < num_samples; i++) { x += data[i].x; y += data[i].y; }
  x /= (int32_t)num_samples;
  y /= (int32_t)num_samples;

  if (!s_have_sample) { s_fx = x; s_fy = y; s_have_sample = true; }
  else { s_fx += (x - s_fx) / 3; s_fy += (y - s_fy) / 3; }

  if (s_cal_count < CAL_SAMPLES) {
    s_cal_sum_x += x;
    s_cal_sum_y += y;
    if (++s_cal_count == CAL_SAMPLES) finish_calibration();
  }
  update_targets();

  if (++s_ui_div >= 5) {     // Anzeige mit ~5 Hz aktualisieren
    s_ui_div = 0;
    layer_mark_dirty(s_canvas);
  }
}

static void compass_handler(CompassHeadingData data) {
  s_compass_status = data.compass_status;
  if (data.compass_status == CompassStatusDataInvalid) return;
  int32_t cw = (TRIG_MAX_ANGLE - data.magnetic_heading) & 0xFFFF;   // im Uhrzeigersinn
  if (!s_have_heading) { s_heading = cw; s_have_heading = true; }
  else s_heading = (s_heading + wrap_angle(cw - s_heading) / 2) & 0xFFFF;
  update_targets();
}

static bool compass_needed(void) {
  return s_set.pitch_axis == AxisCompass || s_set.vol_axis == AxisCompass;
}

static void apply_sensor_subscriptions(void) {
  bool need = compass_needed();
  if (need && !s_compass_on) {
    compass_service_set_heading_filter(DEG_TO_TRIGANGLE(1));
    compass_service_subscribe(compass_handler);
    s_compass_on = true;
  } else if (!need && s_compass_on) {
    compass_service_unsubscribe();
    s_compass_on = false;
  }
}

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------
static void generate_chunk(void) {
  const int8_t *table = WAVETABLES[s_set.wave][s_band];
  for (int i = 0; i < CHUNK_SAMPLES; i++) {
    s_phase_inc += (uint32_t)(((int32_t)(s_target_inc - s_phase_inc)) >> s_glide_shift);
    s_amp += (s_target_amp - s_amp) >> 7;
    s_phase += s_phase_inc;
    int32_t v = (int32_t)table[s_phase >> 24] * s_amp;   // -128*65536 .. 127*65536
    s_chunk[i] = (int16_t)(v >> 8);
  }
  s_chunk_len = CHUNK_SAMPLES * sizeof(int16_t);
  s_chunk_pos = 0;
}

static void audio_tick(void *ctx) {
  s_timer = NULL;
  if (!s_playing) return;

  // Hoechstens LEAD_MS vor der Wiedergabe schreiben: kleine Latenz, egal wie
  // gross der interne Puffer der Firmware ist (~1 s).
  uint32_t allowed = (stream_clock_ms() + LEAD_MS) * (SAMPLE_RATE / 1000);
  int budget = WRITES_PER_TICK;
  while (s_samples_written < allowed && budget-- > 0) {
    if (s_chunk_pos >= s_chunk_len) generate_chunk();
    uint32_t n = speaker_stream_write((const uint8_t *)s_chunk + s_chunk_pos, s_chunk_len - s_chunk_pos);
    if (n == 0) break;
    s_chunk_pos += n;
    s_samples_written += n / sizeof(int16_t);
  }
  s_timer = app_timer_register(TICK_MS, audio_tick, NULL);
}

static void start_audio(void) {
  if (s_playing) return;
  if (!speaker_stream_open(PCM_FORMAT, VOLUME_LEVELS[s_set.volume_idx])) {
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
  s_amp = 0;
  s_phase_inc = s_target_inc;
  audio_tick(NULL);
  layer_mark_dirty(s_canvas);
}

static void stop_audio(void) {
  if (!s_playing) return;
  s_playing = false;
  if (s_calibrated) s_status = "SELECT: Start";
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  speaker_stream_close();
  if (s_canvas) layer_mark_dirty(s_canvas);
}

// ---------------------------------------------------------------------------
// Einstellungen laden / speichern / anwenden
// ---------------------------------------------------------------------------
static void settings_apply(void) {
  s_glide_shift = GLIDE_SHIFT[s_set.glide];
  if (s_playing) speaker_set_volume(VOLUME_LEVELS[s_set.volume_idx]);
  apply_sensor_subscriptions();
  update_targets();
}

static void settings_load(void) {
  s_set = DEFAULT_SETTINGS;
  if (persist_exists(PERSIST_KEY_SETTINGS)) {
    Settings tmp;
    if (persist_read_data(PERSIST_KEY_SETTINGS, &tmp, sizeof(tmp)) == (int)sizeof(tmp) &&
        tmp.version == SETTINGS_VERSION) {
      s_set = tmp;
    }
  }
  // Grenzen absichern
  if (s_set.wave >= WaveCount) s_set.wave = WaveSine;
  if (s_set.pitch_axis >= AxisFixed) s_set.pitch_axis = AxisLift;
  if (s_set.vol_axis >= AxisCount) s_set.vol_axis = AxisRoll;
  if (s_set.root_idx >= ROOT_COUNT) s_set.root_idx = 1;
  if (s_set.octaves < 1 || s_set.octaves > 4) s_set.octaves = 4;
  if (s_set.scale >= ScaleCount) s_set.scale = ScaleFree;
  if (s_set.pitch_sens > 2) s_set.pitch_sens = 1;
  if (s_set.vol_sens > 2) s_set.vol_sens = 1;
  if (s_set.glide > 2) s_set.glide = 1;
  if (s_set.volume_idx >= VOLUME_COUNT) s_set.volume_idx = 2;
}

static void settings_save(void) {
  persist_write_data(PERSIST_KEY_SETTINGS, &s_set, sizeof(s_set));
}

// ---------------------------------------------------------------------------
// Einstellungsmenue
// ---------------------------------------------------------------------------
enum {
  RowCalibrate = 0, RowPitchAxis, RowVolAxis, RowInvertPitch, RowInvertVol,
  RowRoot, RowOctaves, RowScale, RowPitchSens, RowVolSens, RowGlide, RowVolume, RowWave,
  RowCount
};

static const char *ROW_TITLES[RowCount] = {
  "Kalibrieren", "Tonhöhe", "Lautstärke", "Tonhöhe umkehren", "Lautst. umkehren",
  "Tiefster Ton", "Umfang", "Tonleiter", "Empf. Tonhöhe", "Empf. Lautstärke",
  "Portamento", "Max. Lautstärke", "Wellenform",
};

static const char *row_value(int row, char *buf, size_t len) {
  switch (row) {
    case RowCalibrate:   return "Nullpunkt neu setzen";
    case RowPitchAxis:   return AXIS_NAMES[s_set.pitch_axis];
    case RowVolAxis:     return AXIS_NAMES[s_set.vol_axis];
    case RowInvertPitch: return YESNO_NAMES[s_set.invert_pitch];
    case RowInvertVol:   return YESNO_NAMES[s_set.invert_vol];
    case RowRoot:        return ROOT_NAMES[s_set.root_idx];
    case RowOctaves:     snprintf(buf, len, "%u Oktave%s", s_set.octaves, s_set.octaves == 1 ? "" : "n"); return buf;
    case RowScale:       return SCALE_NAMES[s_set.scale];
    case RowPitchSens:   return SENS_NAMES[s_set.pitch_sens];
    case RowVolSens:     return SENS_NAMES[s_set.vol_sens];
    case RowGlide:       return GLIDE_NAMES[s_set.glide];
    case RowVolume:      return VOLUME_NAMES[s_set.volume_idx];
    case RowWave:        return WAVE_NAMES[s_set.wave];
    default:             return "";
  }
}

static uint16_t menu_num_rows(MenuLayer *ml, uint16_t section, void *ctx) { return RowCount; }

static int16_t menu_header_height(MenuLayer *ml, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void menu_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  menu_cell_basic_header_draw(ctx, cell, "Einstellungen");
}

static void menu_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  char buf[24];
  menu_cell_basic_draw(ctx, cell, ROW_TITLES[idx->row], row_value(idx->row, buf, sizeof(buf)), NULL);
}

static void menu_select(MenuLayer *ml, MenuIndex *idx, void *data) {
  switch (idx->row) {
    case RowCalibrate:
      start_calibration();
      window_stack_pop(true);
      return;
    case RowPitchAxis:   s_set.pitch_axis = (s_set.pitch_axis + 1) % AxisFixed; break;   // ohne "Immer voll"
    case RowVolAxis:     s_set.vol_axis = (s_set.vol_axis + 1) % AxisCount; break;
    case RowInvertPitch: s_set.invert_pitch ^= 1; break;
    case RowInvertVol:   s_set.invert_vol ^= 1; break;
    case RowRoot:        s_set.root_idx = (s_set.root_idx + 1) % ROOT_COUNT; break;
    case RowOctaves:     s_set.octaves = (s_set.octaves % 4) + 1; break;
    case RowScale:       s_set.scale = (s_set.scale + 1) % ScaleCount; break;
    case RowPitchSens:   s_set.pitch_sens = (s_set.pitch_sens + 1) % 3; break;
    case RowVolSens:     s_set.vol_sens = (s_set.vol_sens + 1) % 3; break;
    case RowGlide:       s_set.glide = (s_set.glide + 1) % 3; break;
    case RowVolume:      s_set.volume_idx = (s_set.volume_idx + 1) % VOLUME_COUNT; break;
    case RowWave:        s_set.wave = (s_set.wave + 1) % WaveCount; break;
  }
  settings_apply();
  menu_layer_reload_data(ml);
}

static void menu_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = menu_num_rows,
    .get_header_height = menu_header_height,
    .draw_header = menu_draw_header,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
  });
  menu_layer_set_click_config_onto_window(s_menu, window);
  menu_layer_set_highlight_colors(s_menu, PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorBlack), GColorWhite);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void menu_window_unload(Window *window) {
  settings_save();
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  window_destroy(s_menu_window);
  s_menu_window = NULL;
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void open_settings(void) {
  if (s_menu_window) return;
  s_menu_window = window_create();
  window_set_window_handlers(s_menu_window, (WindowHandlers){
    .load = menu_window_load, .unload = menu_window_unload,
  });
  window_stack_push(s_menu_window, true);
}

// ---------------------------------------------------------------------------
// Hauptanzeige
// ---------------------------------------------------------------------------
static void canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  const int16_t w = b.size.w;
  GColor accent  = PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorBlack);
  GColor accent2 = PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "THEREMIN", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, 2, w, 22), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  static char freq_buf[16];
  snprintf(freq_buf, sizeof(freq_buf), "%u Hz", (unsigned)s_freq_hz);
  graphics_context_set_text_color(ctx, s_playing ? accent : GColorBlack);
  graphics_draw_text(ctx, freq_buf, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                     GRect(0, 26, w, 48), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  static char note_buf[8];
  uint32_t midi = ROOT_MIDI[s_set.root_idx] + ((s_semis_q8 + 128) >> 8);
  snprintf(note_buf, sizeof(note_buf), "%s%u", NOTE_NAMES[midi % 12], (unsigned)(midi / 12) - 1);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, note_buf, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(0, 70, w, 32), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Tonhoehen-Balken
  static char label[40];
  const int16_t px = 14, pw = w - 2 * px, py = 126;
  snprintf(label, sizeof(label), "Tonhöhe: %s", AXIS_NAMES[s_set.pitch_axis]);
  graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, py - 18, w, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, GRect(px, py, pw, 14));
  uint32_t range_q8 = (uint32_t)semitone_range() * 256;
  int16_t mx = px + (int16_t)((s_semis_q8 * (uint32_t)(pw - 6)) / range_q8);
  graphics_context_set_fill_color(ctx, accent);
  graphics_fill_rect(ctx, GRect(mx, py + 1, 6, 12), 0, GCornerNone);
  for (int o = 1; o < s_set.octaves; o++) {
    int16_t ox = px + (int16_t)(pw * o / s_set.octaves);
    graphics_draw_line(ctx, GPoint(ox, py + 14), GPoint(ox, py + 18));
  }

  // Lautstaerke-Balken
  const int16_t vy = 170;
  snprintf(label, sizeof(label), "Lautstärke: %s", AXIS_NAMES[s_set.vol_axis]);
  graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, vy - 18, w, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_rect(ctx, GRect(px, vy, pw, 14));
  int32_t shown_amp = s_playing ? s_amp : s_target_amp;   // ohne Ton: Zielwert anzeigen
  int16_t vw = (int16_t)(((int64_t)shown_amp * (pw - 2)) >> 16);
  graphics_context_set_fill_color(ctx, accent2);
  graphics_fill_rect(ctx, GRect(px + 1, vy + 1, vw, 12), 0, GCornerNone);

  // Fusszeile
  static char foot_buf[48];
  const char *status = s_status;
  if (compass_needed() && s_compass_status == CompassStatusDataInvalid) {
    status = "Kompass: Uhr in 8er-Bewegung";
    snprintf(foot_buf, sizeof(foot_buf), "%s", status);
  } else {
    snprintf(foot_buf, sizeof(foot_buf), "%s   |   %s", WAVE_NAMES[s_set.wave], status);
  }
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
static void select_long(ClickRecognizerRef rec, void *ctx) { open_settings(); }
static void up_long(ClickRecognizerRef rec, void *ctx)     { start_calibration(); }

static void wave_step(int dir) {
  s_set.wave = (uint8_t)(((int)s_set.wave + dir + WaveCount) % WaveCount);
  settings_save();
  layer_mark_dirty(s_canvas);
}
static void up_click(ClickRecognizerRef rec, void *ctx)   { wave_step(-1); }
static void down_click(ClickRecognizerRef rec, void *ctx) { wave_step(+1); }

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, select_long, NULL);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_long_click_subscribe(BUTTON_ID_UP, 700, up_long, NULL);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
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
  apply_sensor_subscriptions();
  start_calibration();
}

static void window_unload(Window *window) {
  stop_audio();
  accel_data_service_unsubscribe();
  if (s_compass_on) { compass_service_unsubscribe(); s_compass_on = false; }
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

static void init(void) {
  settings_load();
  s_glide_shift = GLIDE_SHIFT[s_set.glide];
  update_targets();

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = window_load, .unload = window_unload });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  settings_save();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
