# C-API: Bausteine, die man wirklich braucht

Nachschlagewerk zum Gerüst in `assets/main.c`. Vollständige Referenz:
`developer.repebble.com/docs/c/`.

Inhalt: 1. Fenster · 2. Layer-Typen · 3. Selbst zeichnen · 4. Text und Fonts ·
5. Bilder und Vektoren · 6. Tasten und Touch · 7. Animationen · 8. Zeit ·
9. Speicher im Flash · 10. Sensoren und Dienste · 11. Fallen

## 1. Fenster

```c
Window *win = window_create();
window_set_window_handlers(win, (WindowHandlers) {
  .load    = window_load,      // Layer hier erzeugen
  .unload  = window_unload,    // und hier zerstoeren
  .appear  = window_appear,    // sichtbar geworden: Abos starten
  .disappear = window_disappear // verdeckt: Abos stoppen
});
window_stack_push(win, true);      // true = animiert
window_stack_pop(true);
window_destroy(win);               // erst ganz am Ende
```

`load`/`unload` können mehrfach laufen, solange die App lebt — das System
entlädt verdeckte Fenster. Laufende Timer und Abos gehören deshalb in
`appear`/`disappear`, nicht in `load`/`unload`.

Fertige Fenster für Standardfälle, die viel Code sparen:

- **`ActionBarLayer`** — Symbolleiste rechts neben den drei rechten Tasten.
  Spart dem Nutzer Scrollen und damit Akku.
- **`MenuLayer`** — Listen mit Abschnitten; auf runden Displays
  `menu_layer_set_center_focused(true)`.
- **`ActionMenu`** — modales Auswahlmenü über der App.
- **`StatusBarLayer`** — Uhrzeit oben, passt sich dem System an.
- **`ScrollLayer`** — scrollbarer Inhalt, größer als das Display.
- **`NumberWindow`** — fertiger Zahlenwähler.

## 2. Layer-Typen

| Typ | Zweck | Zerstören mit |
|---|---|---|
| `Layer` | eigene Zeichenfläche | `layer_destroy` |
| `TextLayer` | Text mit Umbruch und Ausrichtung | `text_layer_destroy` |
| `BitmapLayer` | ein Bild anzeigen | `bitmap_layer_destroy` |
| `MenuLayer` | Liste | `menu_layer_destroy` |
| `ScrollLayer` | scrollbarer Bereich | `scroll_layer_destroy` |
| `ActionBarLayer` | Tastenleiste | `action_bar_layer_destroy` |
| `StatusBarLayer` | Statuszeile | `status_bar_layer_destroy` |

Jeder Layer hat `layer_get_layer()`-Zugang zum Basis-Layer (`text_layer_get_layer`
usw.), den man `layer_add_child` übergibt.

## 3. Selbst zeichnen

```c
static void update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 4, GCornersAll);          // Radius, Ecken

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(b.size.w, b.size.h));

  graphics_fill_circle(ctx, grect_center_point(&b), 20);
  graphics_fill_radial(ctx, b, GOvalScaleModeFitCircle, 10,
                       DEG_TO_TRIGANGLE(0), DEG_TO_TRIGANGLE(270));
}
layer_set_update_proc(s_layer, update_proc);
layer_mark_dirty(s_layer);        // loest Neuzeichnen aus, zeichnet nicht selbst
```

Regeln:

- **Nur innerhalb von `update_proc` zeichnen.** Ein `GContext` außerhalb ist
  ungültig.
- `layer_mark_dirty` sammelt; mehrfach aufrufen kostet nichts.
- **Keine Mathematikbibliothek.** Es gibt kein `sin`/`cos` aus libm — stattdessen
  `sin_lookup()`/`cos_lookup()` mit `TRIGANGLE_TO_DEG`/`DEG_TO_TRIGANGLE`
  (Vollkreis = 65536) und Festkomma-Arithmetik.
- Große Flächen zeichnen ist billiger als ein Bild laden (kein Heap).

## 4. Text und Fonts

```c
text_layer_set_font(tl, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
text_layer_set_overflow_mode(tl, GTextOverflowModeWordWrap);
text_layer_set_text_alignment(tl, GTextAlignmentCenter);

// Eigener Font aus den Ressourcen: unbedingt wieder entladen!
s_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MONO_20));
...
fonts_unload_custom_font(s_font);
```

**`text_layer_set_text` kopiert nicht.** Der Zeiger muss gültig bleiben, solange
der Layer ihn anzeigt — deshalb statische Puffer, keine lokalen.

Höhe vorher ausrechnen, wenn der Text variabel ist:

```c
GSize size = graphics_text_layout_get_content_size(str, font, box,
               GTextOverflowModeWordWrap, GTextAlignmentLeft);
```

Systemschriften heißen `FONT_KEY_GOTHIC_{14,18,24,28}[_BOLD]`,
`FONT_KEY_BITHAM_{30,42}_*`, `FONT_KEY_ROBOTO_*`, `FONT_KEY_LECO_*`.

## 5. Bilder und Vektoren

```c
GBitmap *bmp = gbitmap_create_with_resource(RESOURCE_ID_LOGO);
bitmap_layer_set_bitmap(bl, bmp);
bitmap_layer_set_compositing_mode(bl, GCompOpSet);   // Alpha respektieren
...
gbitmap_destroy(bmp);                                 // nach dem Layer
```

- **Speicher:** ein Byte pro Pixel in Farbe, ein Bit in S/W. Vor dem Einbinden
  ausrechnen (`Breite × Höhe`), ob das Budget reicht.
- **Animierte Bilder:** `GBitmapSequence` (APNG), Bilder einzeln in ein
  vorhandenes `GBitmap` rendern statt pro Frame neu anlegen.
- **PDC (Pebble Draw Commands):** Vektorformat, winzig, skaliert sauber über
  Plattformen. `gdraw_command_image_create_with_resource(...)`,
  `gdraw_command_image_draw(ctx, img, GPoint(0,0))`,
  `gdraw_command_image_destroy(...)`. Für Symbole fast immer die bessere Wahl
  als PNG. SVG lässt sich mit dem SDK-Werkzeug `svg2pdc` konvertieren.

## 6. Tasten und Touch

```c
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, down_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, down_cb, up_cb);  // 0 = ~500 ms
  window_multi_click_subscribe(BUTTON_ID_UP, 2, 2, 0, true, double_cb);
}
window_set_click_config_provider(win, click_config_provider);
```

`BUTTON_ID_BACK` lässt sich abfangen, sollte aber die App weiterhin beenden
können — sonst fühlt sie sich kaputt an. In einem **Watchface** stehen die
Tasten gar nicht zur Verfügung.

Touch (`PBL_TOUCH`, also `emery` und `gabbro`) ist eine Ergänzung, kein Ersatz:
Die Tasten müssen weiter alles bedienen können.

## 7. Animationen

```c
Animation *a = (Animation *)property_animation_create_layer_frame(layer, &from, &to);
animation_set_duration(a, 300);
animation_set_curve(a, AnimationCurveEaseOut);
animation_schedule(a);     // raeumt sich selbst auf
```

Für eigene Werte `animation_create()` mit `AnimationImplementation` und einem
`update`-Callback, der `AnimationProgress` (0 … `ANIMATION_NORMALIZED_MAX`)
bekommt. Niemals Bewegung mit `psleep()` bauen — das blockiert die ganze App.

## 8. Zeit

```c
time_t now = time(NULL);
struct tm *t = localtime(&now);           // Zeiger auf statischen Puffer!
strftime(buf, sizeof(buf), clock_is_24h_style() ? "%H:%M" : "%I:%M %p", t);

tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
AppTimer *timer = app_timer_register(250, timer_cb, NULL);
app_timer_cancel(timer);
```

`localtime()` liefert einen Zeiger auf einen gemeinsamen statischen Puffer —
Inhalt kopieren, wenn er länger gebraucht wird.

## 9. Speicher im Flash

```c
persist_write_int(KEY_COUNT, 42);
persist_write_data(KEY_SETTINGS, &settings, sizeof(settings));
if (persist_exists(KEY_SETTINGS)) {
  persist_read_data(KEY_SETTINGS, &settings, sizeof(settings));
}
```

- **4 kB pro App insgesamt**, ein einzelner Wert höchstens
  `PERSIST_DATA_MAX_LENGTH` (derzeit 256 Byte). Schlüssel sind `uint32_t`.
- **Daten überleben ein Update der App**, werden aber beim Deinstallieren
  gelöscht.
- **Schreiben blockiert** für Millisekunden, und die Hintergrundpflege des
  Speichers kann zusätzlich kurz bremsen. Deshalb beim Starten oder Beenden
  lesen und schreiben — nicht in Timer-Callbacks, nicht während Animationen,
  und nie während Tonausgabe (siehe `pebble-audio`).
- Versionierung einplanen: einen Schlüssel für die Struktur-Version mitführen
  oder die Größe prüfen, sonst liest ein Update Müll.

## 10. Sensoren und Dienste

| Dienst | Einstieg | Hinweis |
|---|---|---|
| Beschleunigung | `accel_data_service_subscribe(10, handler)` | gebündelt abholen, spart Akku |
| Tippen | `accel_tap_service_subscribe(handler)` | billigste Geste |
| Kompass | `compass_service_subscribe`, `compass_service_set_heading_filter` | Filter setzen |
| Herzfrequenz | `health_service_peek_current_value(HealthMetricHeartRateBPM)` | nur mit `PBL_HEALTH` + HRM |
| Schritte, Schlaf | `health_service_sum_today(HealthMetricStepCount)` | `PBL_HEALTH` prüfen |
| Batterie | `battery_state_service_subscribe` | |
| Bluetooth | `connection_service_subscribe` | Anzeige ohne Handy testen |
| Diktat | `dictation_session_create/start` | nur `PBL_MICROPHONE`; **kein** Zugriff auf Rohaudio |
| Vibration | `vibes_short_pulse`, `vibes_enqueue_custom_pattern` | abschaltbar anbieten |
| Licht | `light_enable_interaction()` | danach automatische Steuerung zurückgeben |
| Wecken | `wakeup_schedule(time, cookie, true)` | startet die App auch beendet |
| Hintergrund | Worker in `worker_src/c/` | eigener Prozess, sehr wenig Speicher |
| Ton | `speaker_*` | eigener Skill: `pebble-audio` |

## 11. Fallen, die immer wieder zuschlagen

1. **`*_create` ohne `*_destroy`** im passenden `window_unload`. Die App
   überlebt den Emulator und stirbt auf der Uhr nach dem zwölften Öffnen.
2. **Rückgabewert nicht geprüft.** Bei knappem Speicher liefert `*_create()`
   `NULL`; der Absturz kommt später und woanders.
3. **`text_layer_set_text` mit lokalem Puffer.** Der Text ist weg, sobald die
   Funktion zurückkehrt — die Anzeige zeigt Müll oder stürzt ab.
4. **Farbwerte in Arrays.** `GColorX` sind Compound-Literals und taugen nicht
   als Array-Initialisierer. In Tabellen die Ganzzahl-Makros benutzen
   (`GColorYellowARGB8`, mit `8` am Ende) und zur Laufzeit
   `(GColor){ .argb = wert }` bauen.
5. **Feste Pixelwerte.** Sieben Plattformen von 144 × 168 bis 260 × 260, zwei
   davon rund. Immer aus `layer_get_unobstructed_bounds()` rechnen.
6. **Lange Rechnung im Callback.** Es gibt keinen zweiten Thread; alles darüber
   blockiert Anzeige, Tasten und Bluetooth gleichzeitig.
7. **Großer Puffer auf dem Stack.** Der Stack ist klein — statisch anlegen oder
   `malloc` mit `NULL`-Prüfung.
