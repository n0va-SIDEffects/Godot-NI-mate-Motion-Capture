# BODENEFFEKT (Arbeitstitel, engl. Ground Effect)

Konzeptpapier aus der Ideenrunde fuer die Pebble Time 2. Jurybewertung 6,9 von 10.
Der Text ist der Stand der Ideenrunde. Der Abschnitt ganz am Ende beantwortet
mehrere seiner offenen Fragen mit Messungen und Quellenanalysen, die spaeter
dazugekommen sind, und hat Vorrang.

## Pitch

Ein Voxel-Tiefflug-Racer im Stil von Comanche (1992), aber auf 1,5 Zoll am Handgelenk: Du jagst einen Gleiter durch prozedural erzeugte Canyons und hältst ihn so knapp über dem Boden, dass sein Schatten fast den Rumpf berührt, denn nur dort lädt sich der Boost. Die Landschaft wird 30-mal pro Sekunde perspektivisch aus einer Heightmap direkt in den 8-Bit-Framebuffer gerastert, im Dunst gedithert, von der echten Sonnenposition beleuchtet und per RGB-Backlight physisch gegradet, sodass die Uhr abends orange und nachts blau glüht. Eine Runde dauert 60 bis 120 Sekunden, die Tagesstrecke wechselt mit Datum und Tageszeit.

## Genre

Pseudo-3D-Arcade-Racer (Voxel-Heightmap-Tiefflug, Time-Attack durch Tore), Single-Player mit asynchroner Bestenliste, Emery-exklusiv

## Kernschleife

(1) Start: Strecke wählen (6 handgebaute "Rezepte" aus Seed + Torliste + Sonnenrichtung + Nebeldichte, plus die Tagesstrecke aus dem Datum). Die Welt (128 x 128 Heightmap + vorbeleuchtete Colormap) wird in unter 20 ms aus dem Seed generiert. Countdown 3-2-1 als LRA-Metronom, Backlight fährt auf die Tageszeit-Farbe. (2) Flug: Der Gleiter fliegt mit konstantem Grundtempo (ca. 25 Zellen/s) automatisch vorwaerts; der Spieler steuert nur Roll (Kurs) und Hoehe. Auf der Strecke stehen 6 bis 10 Tore (Pylonenpaare), jedes durchflogene Tor gibt +4 s Zeitgutschrift, ein verpasstes Tor nichts. Berge muessen umflogen oder ueberflogen werden; Bodenkontakt (Hoehe ueber Grund < 2 Einheiten) kostet 40 Prozent Tempo, einen von drei Rumpfpunkten und loest Rot-Blitz plus Rumpel-Pattern aus. (3) Bodeneffekt: Zwischen 2 und 12 Einheiten ueber Grund fuellt sich der Boost-Balken (voll nach ca. 3 s). Sichtbar am Schatten, der an den Rumpf rueckt, hoerbar am lauter werdenden Windrauschen, fuehlbar am dichter werdenden LRA-Prasseln. Tap oder Select zuendet den Boost: 2,5 s doppeltes Tempo, Backlight-Weissblitz, Cinemascope-Vignette, Streaks. Im Boost ist das Terrain doppelt so gefaehrlich, also Risk/Reward in jeder Schlucht. (4) Ende nach dem Zieltor oder wenn der Zeitbalken (Startguthaben 45 s plus Torgutschriften) leer ist oder alle Rumpfpunkte weg sind. Ergebnis: Zeit, Tore, "Sohlenzeit" (Sekunden im Bodeneffekt), Puls-Delta als Kosmetik. Bestzeit lokal in persist, optional Upload ueber PebbleKit JS. (5) Meta: Dieselbe Strecke sieht morgens, mittags, abends und nachts anders aus und spielt sich anders (Sichtweite, Schattenhaenge, Leuchttore). Die Tagesstrecke hat drei gewertete Versuche; AppGlance zeigt den Stand, ein optionaler Wakeup erinnert an die "goldene Stunde" vor Sonnenuntergang, in der die Abendvariante freigeschaltet ist.

## Sitzungslaenge und Wiederkehr

Ein Lauf dauert 60 bis 120 s (Strecke 6 bis 10 Tore plus Zielsprint), Ergebnisbildschirm 10 s, Neustart per Select. Typische Session: 2 bis 3 Laeufe, also 3 bis 5 Minuten, ohne Menue-Umwege (Quick-Launch startet direkt die Tagesstrecke). Wiederkommen: (a) Tagesstrecke mit Datums-Seed und nur drei gewerteten Versuchen, (b) echte Tageszeit veraendert Licht, Sichtweite und Schwierigkeit derselben Strecke, also lohnt ein Abend- und ein Nachtlauf, (c) AppGlance im Launcher zeigt "Daily 1:32,4 / 2 Versuche" und triggert den Ehrgeiz beim Blick auf die Uhr, (d) Bestenliste pro Strecke und Tag ueber das Telefon, (e) Golden-Hour-Wakeup einmal taeglich, abschaltbar.

## Bedienung

Profil A "Fingerstick" (Standard, wenn touch_service_is_enabled() wahr): Finger irgendwo im unteren Bilddrittel (Cockpit-/HUD-Band, Zeilen 140 bis 227) aufsetzen; der Versatz zum Touchdown-Punkt aus den Rohevents (TouchEvent_PositionUpdate x/y) ist der Steuerinput: x = Roll, y = Hoehe, Deadzone 6 px, Saettigung bei 40 px, quadratische Kurve fuer feine Korrekturen. Loslassen = Neutrallage, der Gleiter richtet sich auf. Tap (Liftoff innerhalb 250 ms per time_ms() und unter 6 px Bewegung) = Boost. Es muss nichts getroffen werden, und der Finger liegt unter der Landschaft, nicht darauf. Tasten als Modifier im Zangengriff (Daumen der Fingerhand an der rechten Tastenreihe): Select gehalten waehrend des Ziehens = Praezisionsmodus (halbe Empfindlichkeit fuer enge Tore), Up/Down = Hoehentrimmung, Back = Pause, Long-Back = Abbruch. Profil B "Tilt" (waehlbar): accel_data_service mit 50 Hz, Batch 5, Tiefpass; x-Achse = Roll, y-Achse = Hoehe, Nullpunkt per Long-Select im Menue kalibriert, Klopfen aufs Gehaeuse (accel_tap_service) oder Select = Boost. Profil C "Tasten-only" (automatisch, wenn Touch systemweit aus ist und Tilt nicht gewaehlt; auch mitten im Lauf umschaltbar, weil touch_service_is_enabled() bei jedem Fokuswechsel geprueft wird): Up/Down mit Repeat-Click = Roll links/rechts, Select gehalten = steigen, losgelassen = sinken bis zur Trimmhoehe (Flappy-artiges Hoehenmodell, gut spielbar), Doppelklick Select = Boost, Back = Pause. Lautsprecher stumm (speaker_is_muted() oder Volume 0): jeder Audio-Cue hat eine haptische und visuelle Doublette: Tor = 30-ms-LRA-Tick plus Cyan-Backlight-Tick, Boost bereit = vibes_double_pulse plus gruenlich pulsierendes Backlight, Bodenkontakt = Rumpel-Pattern plus Rot-Blitz, Hoehe ueber Grund = Schatten am Rumpf plus Prasselrate. Die Latenzkalibrierung entfaellt dann.

## Einsatz der Hardware

**touch**: Tragend. Fingerstick aus Rohevents (touch_service_subscribe): relativer Zwei-Achsen-Stick, Touchdown irgendwo im unteren Drittel, Versatz = Roll und Hoehe, Tap = Boost. Der Pan-Recognizer wird nicht genutzt, weil er nur eine Achse liefert. Im Ergebnis- und Streckenmenue: Swipe-Recognizer links/rechts zum Blaettern, Tap zum Bestaetigen. Bei Touch aus (Settings) faellt das Spiel ohne Neustart auf Tilt oder Tasten zurueck.

**speaker**: Tragend als Instrument, nicht als Jingle-Automat. Waehrend des Laufs laeuft ein eigener Software-Synth ueber speaker_stream_open (mono, 8 kHz, 8 Bit): pro 30-fps-Frame werden 267 Samples in einen Doppelpuffer (2 x 512 B) gemischt: Motor = Saegezahn 40 bis 120 Hz proportional zum Tempo, Wind = LFSR-Rauschen, dessen Lautstaerke mit der Bodennaehe steigt (Audio-Hoehenmesser), Tor-Ping = abklingender Sinus 880 Hz aus einer 256-B-Sinustabelle, Boost = Aufwaerts-Sweep, Crash = 2,4-KB-PCM-Sample aus den Ressourcen. Mischen kostet ca. 30 Zyklen pro Sample, also 0,01 M Zyklen pro Frame. Im Menue: speaker_play_tracks mit 4 Stimmen (Square-Bass, Triangle-Pad, Sawtooth-Lead, kurze Square-Noten als Perkussion). speaker_set_finish_callback faengt Preempted (Benachrichtigung) ab und oeffnet den Stream neu. Rueckfallebene, falls Stream-Latenz oder Unterlaeufe stoeren: nur speaker_play_notes fuer Ping/Boost/Crash, kein Motorsound.

**rgb_backlight**: Tragend als physischer Color-Grade. light_enable(true) nur waehrend des Laufs; light_set_color_rgb888 setzt dieselbe Grade-Farbe, mit der die Palette-LUT den Framebuffer toent (Morgen warmes Orange, Mittag neutral, Abend Magenta-Rot, Nacht tiefes Blau, jeweils gedimmt). Das transflektive Panel liegt vor der LED, im Halbdunkel wirkt das wie ein Farbfilter ueber gegradetem Footage. Ereignisse: Boost = 100 ms Weiss, Bodenkontakt = 150 ms Rot, Tor = 60 ms Cyan, Boost bereit = leichtes Pulsieren Richtung Gruen alle 2 s (Information im Augenwinkel ohne HUD-Blick). Option 'Backlight-Grade aus' fuer Akkusparer; im Menue Backlight aus bzw. light_enable_interaction().

**lra_haptics**: Tragend als Hoehenmesser und Metronom. Prasseln: alle 250/500/1000 ms ein vibes_enqueue_custom_pattern mit einem 20-ms-Segment, Rate nach Bodennaehe in drei Stufen mit Hysterese von 2 Einheiten, damit es nicht flattert. Tor = 30-ms-Tick, Boost bereit = vibes_double_pulse, Bodenkontakt = Pattern {80, 40, 120}, Countdown = drei Ticks im 1-s-Raster, Zieltor = {40, 60, 40, 60, 200}. Der LRA macht 20-ms-Impulse sauber trennbar; ein ERM wuerde das Prasseln zu einem Brummen verschmieren. Im Tilt-Profil werden Accel-Samples 60 ms nach jedem Impuls verworfen, weil der Aktor sonst als Steuerausschlag gemessen wird.

**heart_rate**: Nicht fuer Gameplay genutzt, weil HealthMetricHeartRateBPM bis 15 Minuten alt sein kann, der Sample-Period-Aufruf nur ein Vorschlag ist und jede Echtzeit-Mechanik damit unfair oder unlesbar wuerde. Nur Kosmetik im Ergebnisbildschirm: 'Puls-Delta' aus HealthMetricHeartRateRawBPM vor dem Start und nach dem Ziel, angezeigt nur, wenn waehrend des Laufs ein HealthEventHeartRateUpdate kam; abschaltbar.

**compass**: Begrenzt, ehrlich abgesteckt. Im Hangar-/Panorama-Modus vor dem Start folgt der Kamera-Yaw dem Kompass-Heading (compass_service_subscribe, Tiefpass, nur bei Status kalibriert): Man dreht sich real um die eigene Achse und schaut sich in der Voxel-Landschaft um, und weil die In-Game-Sonne auf dem echten Azimut steht (aus Uhrzeit und den vom Telefon gelieferten Sonnenzeiten interpoliert), liegen die Schatten dort, wo sie auch draussen liegen. Das ist der Screenshot- und Store-Video-Modus. Im Lauf nicht genutzt: zu rauschig, zu traege, Kalibrierung am Handgelenk unzuverlaessig. Fallback bei unkalibriertem Kompass: Up/Down drehen die Kamera.

**accelerometer**: Tragend im Tilt-Profil (50 Hz, Batch 5, Tiefpass, Nullpunktkalibrierung, x = Roll, y = Hoehe) und als Boost-Ausloeser per accel_tap_service (Klopfen aufs Gehaeuse, Achse z), sodass im Tilt-Profil keine Hand die Tasten braucht. Zusaetzlich im Fingerstick-Profil als 'Ruhe-Erkennung': wenn der Arm stark schwingt (Gehen), wird die Steuerempfindlichkeit um 30 Prozent gesenkt.

**cpu_240mhz**: Der eigentliche Star. Der komplette Voxel-Rasterizer (200 Rays x ca. 100 Schritte pro Frame, Y-Buffer-Fuellung), der Nebel-Dither, der Palette-Grade-Pass ueber 45.600 Byte, der Software-Audio-Mixer und die Weltgenerierung aus dem Seed (Value-Noise mit 4 Oktaven fuer 16.384 Zellen plus Slope-Beleuchtung) laufen in Software. Gesamtbudget pro Frame ca. 1,2 M Zyklen, also rund 5 ms von 33 ms; die Pebble Time haette mit 100 MHz und 64 KB fuer dieselbe Szene nur die halbe Aufloesung und keine Colormap gehabt. Fixed-Point 24.8 durchgehend, keine Division im inneren Loop (1/z-Tabelle), Sinus aus Tabelle.

**display_64c_mip**: Die 64 Farben werden als fuenf handgewaehlte Terrain-Rampen (Wasser, Sand, Gras, Fels, Schnee) zu je fuenf Helligkeitsstufen plus zwei 'emissive' Leuchtfarben fuer Nachttore genutzt; Zwischenwerte entstehen ausschliesslich per Bayer-Ordered-Dithering, das auf dem MiP ohne Ghosting und ohne LCD-Verschmieren gestochen scharf bleibt und bei 202 ppi aus normaler Distanz als Verlauf gelesen wird. Sonnenlichtlesbarkeit heisst, das Spiel funktioniert draussen ohne Backlight, daher muss die Palette ohne Grade stehen. Zeilenweise Panelaktualisierung: Himmel (ca. 90 Zeilen) und HUD-Band (30 Zeilen) werden nur bei Aenderung neu geschrieben.

**phone_js**: PebbleKit JS berechnet lokal (NOAA-Formel, Geolocation der Telefon-App, kein Internet noetig) Sonnenauf- und -untergang und schickt sie per AppMessage; daraus interpoliert die Uhr den Sonnenazimut fuer Beleuchtung, Grade und Golden-Hour-Wakeup. Clay-Konfigurationsseite: Steuerprofil, Pitch invertieren, Backlight-Grade, Lautstaerke, Audio-Latenzoffset. Bestenliste: Bestzeit pro Strecke und Tagesseed per XHR an einen kleinen Endpoint (Cloudflare Worker o. ae., ausserhalb des SDK, optional); Antwort = Top 5 als ein AppMessage-Paket unter 512 B. Keine Heightmap-Downloads, weil Persistent Storage mit ca. 4 KB dafuer zu klein ist; stattdessen koennen neue Strecken-'Rezepte' (Seed, Torliste, Sonnenrichtung, ca. 120 B) nachgeladen und in persist abgelegt werden.

**worker_wakeup_glance**: AppWorker nicht genutzt, weil kein Hintergrundprozess etwas zum Spiel beitraegt (ein Schritte-zu-Flugmeilen-Mechanismus waere moeglich, verwaessert aber den Kern). Wakeup-API: einmal taeglich, abschaltbar, ca. 40 Minuten vor Sonnenuntergang 'Goldene Stunde: Abendstrecke im Streiflicht', geplant beim App-Exit mit den vom Telefon gelieferten Sonnenzeiten (Fallback 19:30). AppGlance: nach jedem Lauf app_glance_reload mit Status 'Daily 1:32,4 / 2 Versuche uebrig' bzw. 'Abendstrecke offen' plus Icon, Ablauf um Mitternacht.

## Grafik-Kniffe

### Voxel-Space-Terrain direkt im 8-Bit-Framebuffer (Comanche-Algorithmus mit Y-Buffer und Roll per Horizontverschiebung)

**how**: Pro Frame graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit), fb = gbitmap_get_data(), stride = gbitmap_get_bytes_per_row(). Fuer jede der 200 Bildspalten laeuft ein Ray vom Kamerapunkt in Blickrichtung (Yaw als 24.8-Fixed-Point, Sinus/Cosinus aus einer 1024-Eintrag-int16-Tabelle, 2 KB). Schrittweite waechst quadratisch (dz startet bei 1,0 und steigt um 0,02 pro Schritt), Sichtweite 200 Einheiten ergibt ca. 100 Schritte. Pro Schritt: Heightmap-Index = ((y >> 8) & 127) << 7 | ((x >> 8) & 127), das Wrap-Around der 128er-Kachel ist damit gratis; Bildschirmzeile = ((camH - h) * invz[z]) >> 8 + horizon[col], wobei invz eine 512-Eintrag-uint16-Tabelle (1 KB) ist, also keine Division. Liegt die Zeile ueber ybuffer[col] (uint8[200]), wird die Spalte von der neuen Zeile bis ybuffer[col] - 1 mit der Terrainfarbe gefuellt (fb[y * stride + col]) und ybuffer[col] aktualisiert. Roll wird nicht durch Bildrotation erzeugt, sondern durch horizon[col] = horizon0 + ((col - 100) * tanRoll >> 8), ein Add pro Spalte; der Gleiter-Sprite rollt mit doppeltem Winkel, die Kamera mit halbem. Pitch verschiebt horizon0 in Subpixel-Fixed-Point.

**why_impressive**: Eine durchgehend perspektivische, rollende Landschaft mit Bergketten hinter Bergketten und korrekter Verdeckung, fliegend mit 30 fps, auf einer Uhr. Das ist genau der Effekt, den 1992 ein 486er in 320 x 200 zeigte, und niemand erwartet ihn auf 1,5 Zoll. Die Ray-pro-Spalte-Struktur nutzt das Hochformat perfekt: 200 Spalten sind wenig, 228 Zeilen geben Tiefe.

**cost**: Daten: Heightmap 16 KB, Colormap 16 KB, ybuffer 200 B, Sinus 2 KB, invz 1 KB, horizon-Tabelle 400 B. Zyklen: 200 x 100 Schritte x ca. 25 Zyklen (2 Adds, Maskierung, Load, Sub, Mul, Shift, Compare) = 0,5 M; Fuellung ca. 16.000 Terrainpixel x 5 Zyklen = 0,08 M. Zusammen ca. 0,6 M Zyklen = 2,5 ms bei 240 MHz, mit Faktor 2 Sicherheitsaufschlag 5 ms. Notbremse bei Speicherbandbreiten-Ueberraschung: 100 Rays mit 2 px breiten Spalten (halbiert die Ray-Kosten, Comanche tat dasselbe).

### Prozedurale Welt aus Seed mit eingebackener Beleuchtung (Baked Lighting) in der Colormap

**how**: Beim Streckenstart erzeugt Value-Noise (4 Oktaven, integer-Hash, bilineare Interpolation in Fixed-Point) die 128 x 128 Heightmap; ein Canyon-Rezept subtrahiert entlang einer Bezier-Kurve ein Talprofil, sodass die Strecke immer eine fliegbare Schlucht hat. Danach wird pro Zelle einmal die Beleuchtung berechnet: Hangneigung = h[x + sunDx][y + sunDy] - h[x][y], quantisiert auf -2 bis +2 Helligkeitsstufen; Terraintyp aus Hoehe und Neigung (steil = Fels); Farbe = rampe[typ][2 + stufe] aus 5 x 5 handgewaehlten GColor8-Werten. Ergebnis ist die 16-KB-Colormap, deren Bytes direkt Framebuffer-Werte sind. Zur Laufzeit kostet Licht pro Sample nichts mehr. Sonnenrichtung ist Streckenparameter, bei der Tagesstrecke der echte Azimut aus Uhrzeit und Sonnenzeiten vom Telefon.

**why_impressive**: Haenge zur Sonne hin leuchten, abgewandte liegen im Schatten, Grate zeichnen sich als helle Kanten ab: Die Landschaft wirkt modelliert statt flach eingefaerbt, und morgens sieht dieselbe Strecke anders aus als abends. Es gibt keine Terrain-Ressourcen, also unbegrenzt viele Strecken bei null Flash-Verbrauch.

**cost**: Generierung einmalig ca. 16.384 Zellen x (4 Oktaven x ca. 20 Zyklen + Beleuchtung 15 Zyklen) = ca. 1,6 M Zyklen, also unter 10 ms, ausgefuehrt hinter dem Countdown. Speicher: 16 KB Colormap zusaetzlich zur Heightmap; Rampen 25 B; Rezepte je 120 B. Fallback bei Heap-Not: Colormap weglassen und Farbe zur Laufzeit ableiten (spart 16 KB, kostet ca. 10 Zyklen pro Sample, also 0,2 M pro Frame).

### Distanz-Nebel als 2D-LUT (Farbe x Nebelstufe) mit Bayer-4x4-Ordered-Dithering

**how**: fogLUT[32][64] (2 KB, uint8) enthaelt fuer jede der 64 Farben 32 Stufen der Mischung Richtung Himmelsfarbe, berechnet beim Start in RGB888 und dann auf 2 Bit pro Kanal quantisiert. Weil 2 Bit grob springen, wird pro Terrainpixel statt der reinen Stufe fog16 = z >> 4 (0..15, 4 Bit Nachkomma) der Index (fog16 * 2 + bayer[(col & 3) | ((y & 3) << 2)]) >> 3 genommen; die Bayer-Tabelle (16 B) verteilt die Rundung raeumlich, sodass zwischen zwei Palettenfarben vier Mischmuster liegen. Farbe = fogLUT[idx][colormap[i]]. Zwei Palettenwerte (Nachttor-Gelb, Sonnen-Weiss) sind in allen Nebelstufen Identitaet, also 'emissive'. Dieselbe LUT toent im Hintergrund zusaetzlich zum Grade: Nacht = Nebel Richtung Dunkelblau statt Hellgrau.

**why_impressive**: Atmosphaerische Tiefe in 64 Farben: fuenf Bergketten verblassen weich in Dunst, statt in drei harten Stufen zu springen. Auf dem MiP flimmert das Dither nicht und verschmiert nicht, aus 30 cm Distanz liest man einen Verlauf. Gleichzeitig ist der Nebel Spielinformation (Sichtweite = Schwierigkeit).

**cost**: 2 KB LUT + 16 B Bayer. Pro geschriebenem Terrainpixel ein zusaetzliches Load und zwei Shifts, ca. 3 Zyklen x 16.000 Pixel = 0,05 M Zyklen pro Frame. Nebeldichte veraendert zudem die maximale Ray-Laenge: Nacht mit Sichtweite 120 spart ca. 30 Prozent der Ray-Schritte.

### Tageszeit-Color-Grade: Palette-Remap des ganzen Framebuffers plus physisches Backlight-Grading und Ereignis-Grades

**how**: Nach dem Terrain- und Sprite-Pass laeuft ein Pass ueber alle 45.600 Framebuffer-Bytes: fb[i] = gradeLUT[fb[i]] mit einer 256-B-LUT (nur die 64 gueltigen GColor8-Werte werden veraendert). Verarbeitung als 32-Bit-Woerter, vier Lookups pro Wort. Die LUT entsteht aus einer kleinen Grade-Definition wie in der Postproduktion: 3 x 3 Farbmatrix plus Lift/Gain je Tageszeit (8 Stuetzstellen, 8 x 256 B = 2 KB vorberechnet, zur Laufzeit zwischen zwei Nachbarn per Zeit gemischt und neu quantisiert, alle 2 s). Parallel light_set_color_rgb888 mit der Grade-Farbe, sodass das transflektive Panel physisch denselben Farbstich bekommt. Ereignisse tauschen die LUT fuer wenige Frames: Bodenkontakt = 3 Frames Rot-Grade, Tor = 1 Frame Lift +1 Stufe, Boost = obere und untere 40 Zeilen mit einer dunkleren LUT (Cinemascope-Vignette per Zeilenband) plus 12 helle 1-px-Streaks vom Fluchtpunkt in zufaellige Randspalten; Gegenlichtstrecken bekommen eine kontrastaermere LUT mit Flare-Lift, wenn der Yaw in Richtung Sonne zeigt.

**why_impressive**: Sonnenuntergang auf der Uhr: Das ganze Bild kippt ueber zwei Spielminuten von Mittagsneutral nach Abendrot, und die Uhr selbst glueht in derselben Farbe. Fuer jemanden mit Postproduktionsblick ist das ein echter Grade, kein Farbfilter-Sprite. Der Effekt kostet nichts an Szenenkomplexitaet, weil er nach dem Rendern auf Bytes arbeitet.

**cost**: 2 KB Grade-LUTs + 256 B aktive LUT. 11.400 Woerter x ca. 12 Zyklen = 0,14 M Zyklen = 0,6 ms pro Frame. Backlight kostet Akku, deshalb nur waehrend des Laufs und abschaltbar; in der Sonne ist die LED wirkungslos, die LUT wirkt trotzdem.

### Halbtransparenter Bodenschatten und Torverdeckung per Spalten-Clip aus dem Ray-Marsch

**how**: Schatten: Die Terrainhoehe unter dem Gleiter (Heightmap-Lookup an Kameraposition + 16 Einheiten voraus) wird mit derselben Projektionsformel wie das Terrain auf den Bildschirm gebracht; dort wird eine Ellipse (Breite 40 px, Hoehe 12 px, beides schrumpft mit wachsender Hoehe ueber Grund) gestempelt, aber nur auf Pixeln, bei denen der Bayer-Schachbrettwert passt (50 Prozent), und mit darkLUT[64] (jede Farbe eine Stufe dunkler, 64 B) statt einer festen Farbe. Ergebnis wirkt wie ein weicher 50-Prozent-Multiply-Schatten trotz fehlendem Blending. Tore: Waehrend des Ray-Marschs wird in dem Moment, in dem z die Distanz des naechsten Tores ueberschreitet, der aktuelle ybuffer[col] in torClip[col] (uint8[200]) kopiert. Nach dem Terrain werden die Pylonen als eigene 2-px-Bresenham-Linien mit Nebelfarbe der Tordistanz in den Framebuffer gezeichnet, aber nur Pixel mit y < torClip[col]. Damit tauchen Tore korrekt hinter Hügelkuppen auf und verschwinden hinter Graten. Der Gleiter selbst ist ein 48 x 24 px 4-Bit-palettiertes GBitmap (3 Rollposen, links/rechts gespiegelt) mit GCompOpSet fuer Transparenz, gezeichnet nach graphics_release_frame_buffer.

**why_impressive**: Der Schatten ist die wichtigste Spielinformation (Hoehe ueber Grund) und sieht dabei nach echter Beleuchtung aus; Tore, die hinter einem Kamm hervorkommen, geben der Szene Tiefe, die ein Sprite-ueber-Hintergrund-Ansatz nie haette. Beides ohne Z-Buffer.

**cost**: Schatten ca. 300 Pixel x 6 Zyklen, Tore ca. 400 Pixel, torClip 200 B, darkLUT 64 B, Gleiter-Sprites ca. 2 KB im Heap. Zusammen unter 0,02 M Zyklen pro Frame. Verdeckung gilt nur fuer das jeweils naechste Tor (ein Clip-Buffer); das zweite Tor wird ohne Clip in Nebelfarbe gezeichnet, was im Dunst nicht auffaellt.

### Himmel-Caching, Sonnenscheibe als Distanzfeld nur in Himmelpixeln und Dirty-Row-Minimierung

**how**: Der Himmel ist ein vertikaler Verlauf aus skyLUT[228] (228 B, pro Zeile zwei Farben plus Bayer-8x8-Schwelle aus 64 B, ergibt 16 Zwischentoene statt 4) und wird nur neu geschrieben, wenn sich horizon0 oder Roll um mehr als 1 px geaendert haben; sonst bleibt er im Framebuffer stehen und die Terrainfuellung ueberschreibt nur darunter. Die Sonne sitzt bei Spalte (yaw - sunAzimut) * 200 / fov und Zeile horizon0 - sunElevation; ein 32 x 32-Distanzfeld (glowLUT[16] Stufen ueber Radius-Quadrat, Bayer-gedithert) wird nur in Pixel geschrieben, deren Zeile ueber ybuffer[col] liegt, also geht die Sonne hinter Bergen unter. Das HUD-Band (Zeilen 198 bis 227: Zeit, Boost-Balken, Torzaehler) wird per eigenem Layer mit layer_mark_dirty nur bei Wertaenderung neu gezeichnet. Im Boost wird der Himmel bewusst jedes Frame um 1 px vertikal versetzt (Kamerashake), was billig ist, weil nur die Himmelzeilen neu geschrieben werden.

**why_impressive**: Eine tiefstehende Sonne, halb hinter dem Grat, mit weichem Halo, ist der Screenshot-Moment; dass sie sich wie ein Himmelskoerper verhaelt (verschwindet hinter Terrain, wandert mit der echten Uhrzeit), verkauft die 3D-Illusion mehr als jedes Polygon. Nebenbei sinkt die Zahl geaenderter Zeilen, was der zeilenweisen Panelaktualisierung entgegenkommt.

**cost**: skyLUT 456 B, glowLUT 16 B, Bayer-8x8 64 B. Himmel bei Aenderung 200 x 90 Pixel x 3 Zyklen = 0,05 M, im Geradeausflug null. Sonne 1024 Pixel x 6 Zyklen = 0,006 M. Unsicherheit: Ob das System bei Framebuffer-Capture Zeilen-Diffs macht oder den ganzen Layer ueberträgt, ist nicht dokumentiert; im schlimmsten Fall bleibt es eine reine CPU-Ersparnis.

## Mechanik-Kniffe

- Bodeneffekt als Risk/Reward direkt aus dem Renderer: Die Kollisions- und Boost-Logik ist ein einziger Heightmap-Lookup unter dem Gleiter, dieselbe Tabelle, die das Bild erzeugt. Zwischen 2 und 12 Einheiten ueber Grund laedt der Boost, unter 2 gibt es Bodenkontakt. Drei Sinne melden dieselbe Zahl, ohne dass das Auge das HUD braucht: der Schatten rueckt an den Rumpf (Grafik), das Windrauschen im Stream wird lauter (Speaker), das LRA-Prasseln wird dichter (Haptik). Wer schneller sein will, muss tiefer fliegen, und tiefer heisst, dass jede Kuppe zur Falle wird.

- Fingerstick mit Tasten als Modifier statt virtuellem Joystick: Relativer Touch (Versatz zum Touchdown) bedeutet, dass auf 1,5 Zoll nichts getroffen werden muss und der Finger im Cockpit-Band unten liegt, nicht in der Landschaft. Zangengriff: Der Zeigefinger zieht, der Daumen derselben Hand liegt an der rechten Tastenreihe. Select gehalten halbiert die Empfindlichkeit fuer enge Tore, Up/Down trimmen die Hoehe, ohne den Stick loszulassen. Tap ohne Bewegung zuendet den Boost, das Bild ist nach 250 ms wieder frei.

- Sonne als Leveldesign und Kartenlesen ueber Licht: Die Sonnenrichtung (Streckenparameter, bei der Tagesstrecke der echte Azimut) bestimmt ueber das Baked Lighting, welche Haenge lesbar hell und welche im Schatten liegen; Tore im Gegenlicht liegen unter einer kontrastaermeren Flare-LUT, Schluchten im Schatten haben weniger Helligkeitsstufen. Der Spieler lernt, morgens die Osthaenge und abends die Westhaenge zu nehmen, und das Backlight-Grading signalisiert physisch, welche Tageszeitvariante gerade laeuft. Dieselbe Strecke ist um 12 Uhr eine andere als um 19 Uhr.

- Sichtweite als gemeinsame Stellschraube fuer Schwierigkeit und Frame-Budget: Nebeldichte verkuerzt die Ray-Laenge (Nacht 120 statt 200 Einheiten), rendert also schneller und sieht dramatischer aus, waehrend der Spieler Berge spaeter sieht. Nachts sind die Tore mit zwei 'emissiven' Palettenfarben markiert, die die Nebel-LUT nicht anfasst, sodass sie wie Leuchtbojen aus dem Dunst stechen. Performance-Reserve und Schwierigkeitskurve sind derselbe Parameter.

- Haptischer Hoehenmesser mit Hysterese und Accel-Blanking: Die Prasselrate wechselt nur, wenn die Hoehe die Stufengrenze um mehr als 2 Einheiten ueberschreitet, damit der Aktor nicht flattert; jeder Impuls ist ein 20-ms-Segment, das der LRA sauber absetzt. Im Tilt-Profil werden Accel-Batches 60 ms nach jedem Impuls verworfen, weil der Aktor sonst als Rollbefehl gelesen wuerde. Umgekehrt dient der LRA im Countdown als Metronom, gegen das der Spieler in den Einstellungen einmal 'zum Tick tippt', um den Audio-Latenzoffset fuer Speaker-Pings zu kalibrieren.

- Tageszeit und Kalender als Content-Generator ohne Ressourcen: Datum ist der Seed der Tagesstrecke, Uhrzeit setzt Sonnenstand, Grade, Nebel und Backlight, Sonnenzeiten vom Telefon setzen den Golden-Hour-Wakeup und die Abendvariante. Eine Handvoll Rezepte zu je 120 B ergibt Hunderte unterschiedlich aussehende Laeufe, und der Grund, heute Abend nochmal zu spielen, steht am Himmel.

## Der Moment, der verkauft

Das Store-Video zeigt eine Handaufnahme im Halbdunkel: Der Gleiter zieht im Tiefflug durch eine Schlucht, sein halbtransparenter Schatten flitzt ueber den Boden, links und rechts Felswaende in Ocker und Rostrot mit eingebackenem Streiflicht, dahinter verblassen fuenf Bergketten gedithert in magentafarbenem Dunst, die Sonnenscheibe steht halb hinter dem Grat. Ein Tor taucht hinter der Kuppe auf, der Spieler tippt, der Boost zuendet: Weissblitz im Backlight, der Horizont kippt in die Kurve, Cinemascope-Balken und Streaks fuer zwei Sekunden, dann glueht die Uhr wieder in demselben Abendorange wie der In-Game-Himmel, weil das Backlight das Bild physisch gradet. Der Screenshot dazu ist der Hangar-Panorama-Modus: 200 x 228 Pixel Abendlandschaft mit Sonne am echten Azimut, aufgenommen, waehrend man sich real nach Westen dreht.

## Speicherbudget

Code + Heap (Limit 128 KB): Code ca. 32 bis 38 KB (Voxel-Rasterizer und Weltgenerator ca. 6 KB, Spiel- und Streckenlogik 8 KB, Software-Synth 3 KB, Menues, Panorama, Ergebnis, Clay/AppMessage-Handling, Glance/Wakeup 12 bis 16 KB, libc/SDK-Glue 4 KB). Heap-Daten: Heightmap 16.384 B, Colormap 16.384 B, ybuffer 200 B, torClip 200 B, horizon-Tabelle 400 B, Sinustabelle 2.048 B, invz 1.024 B, fogLUT 2.048 B, Grade-LUTs 2.048 B + aktive LUT 256 B, skyLUT 456 B, Bayer 4x4 und 8x8 80 B, darkLUT und glowLUT 80 B, Gleiter-Bitmaps 3 Posen 48 x 24 4-Bit ca. 2.000 B, Audio-Doppelpuffer 1.024 B + Sinus-8-Bit und LFSR-Zustand 300 B, Crash-Sample 2.400 B (nur bei Bedarf geladen), Torliste 64 x 6 B = 384 B, Streckenrezepte 8 x 120 B = 960 B, Text- und Score-Puffer 1 KB. Summe Daten ca. 50 KB. System-Overhead: Window, Layer, AppMessage-Inbox 1 KB und Outbox 256 B, Timer, Services ca. 6 KB; Fragmentierungsreserve 8 KB. Gesamt ca. 96 bis 102 KB, also 26 bis 32 KB Luft. Notnagel: Colormap zur Laufzeit ableiten (minus 16 KB). Der 45.600-B-Framebuffer gehoert dem System und zaehlt nicht mit. Ressourcen (Limit 256 KB): Gleiter-PNGs 4-Bit 3 KB, HUD- und Menue-Icons 4 KB, Custom-Ziffernfont 6 KB (optional, System-Fonts sind gratis), Menue-Musik als 4-Stimmen-Notendaten 4 KB, PCM-Samples (Crash 0,3 s, Zieltor-Fanfare 0,4 s bei 8 kHz/8 Bit) 6 KB, Streckenrezepte und Rampen 2 KB, App-Icon, Glance-Icons als PDC 2 KB, Clay-Konfig-HTML liegt im JS, nicht in den Uhr-Ressourcen. Summe ca. 27 KB. Optional drei handgemalte 128 x 128 Signature-Heightmaps als 8-Bit-PNG (je 16 KB) fuer +48 KB, dann immer noch unter 80 KB von 256 KB. Persistent Storage: Bestzeiten, Daily-Stand, Kalibrierung, Einstellungen, nachgeladene Rezepte zusammen unter 1,5 KB der ca. 4 KB.

## Bildratenschaetzung

Ziel: feste 30 fps per AppTimer (33 ms Raster). CPU-Rechnung pro Frame bei 240 MHz: Voxel-Terrain 0,6 M Zyklen, Nebel-Dither 0,05 M, Palette-Grade-Pass 0,14 M, Himmel bei Aenderung 0,05 M, Schatten/Tore/Sonne/Sprite 0,03 M, Audio-Mix 0,01 M, Spiel- und Eingabelogik 0,05 M, zusammen ca. 0,95 M Zyklen = 4 ms. Mit Faktor 2,5 fuer Compiler-Overhead, Speicherlatenz des Framebuffers und Interrupts liegt das Rendering bei 10 ms, also ca. 30 Prozent Auslastung bei 30 fps. Rein CPU-seitig waeren 50 bis 60 fps drin; die Grenze setzt die nicht dokumentierte Panel-Uebertragung (fast alle Terrainzeilen aendern sich jedes Frame) und der System-Overhead des Layer-Renderns. Deshalb 30 fps als Designannahme und Messung auf echter Hardware als erste Aufgabe; Fallback bei Bandbreitenproblemen sind 100 Rays mit 2 px breiten Spalten oder 25 fps im Nachtmodus. Der Emulator misst hierfuer nichts Verlaessliches.

## Risiken

- PCM-Stream: Latenz, Puffergroesse und Unterlaufverhalten von speaker_stream_write sind nicht dokumentiert; ebenso, ob speaker_play_notes einen laufenden Stream preemptet. Plan B: Events nur ueber speaker_play_notes, Motor- und Windsound entfallen, Bodennaehe bleibt haptisch und visuell lesbar.

- Panel-Transferzeit: Da sich pro Frame nahezu alle Terrainzeilen aendern, greift Dirty-Row-Sparen nur fuer Himmel und HUD. Falls die zeilenweise Uebertragung des JDI-Panels mehr als ca. 25 ms pro Vollbild braucht, limitiert sie die Framerate unabhaengig vom Rasterizer. Nur auf echter Hardware messbar.

- Framebuffer-Capture-Semantik: Nicht dokumentiert, ob graphics_capture_frame_buffer den gesamten Layer als geaendert markiert oder das System Zeilen-Diffs macht; Himmel-Caching koennte nur CPU sparen und nicht Transfer.

- Touch-Abtastrate und -Latenz sind nicht dokumentiert (kein Timestamp im Event). Liegt die Rate unter ca. 30 Hz oder die Latenz ueber 60 ms, fuehlt sich der Fingerstick schwammig an; dann wird Tilt zum Standardprofil und Touch bleibt fuer Tap/Menue.

- Speicherbandbreite: Falls der System-Framebuffer nicht im schnellsten SRAM liegt, kostet die Spaltenfuellung mit Stride 200 mehr als geschaetzt. Fallback: 100 Rays, 2 px pro Spalte, oder Sichtweite reduzieren.

- Heap: 128 KB inklusive Code sind komfortabel, aber Fragmentierung durch Systemobjekte (Fonts, Layer, AppMessage) ist auf Pebble erfahrungsgemaess tueckisch. Colormap-Verzicht ist der eingeplante Notnagel; alle grossen Puffer werden einmal beim Start alloziert und nie freigegeben.

- LRA-Mindestimpuls: Ob ein 20-ms-Segment auf dem AW86225 als sauberer Tick ankommt oder verschluckt wird, ist nicht dokumentiert; ggf. 30 bis 40 ms. Dauerprasseln plus Backlight plus Speaker ueber zwei Minuten kostet spuerbar Akku, daher alles pro Feature abschaltbar.

- Tilt-Steuerung waehrend des Gehens ist unbrauchbar (Armschwung); die Vibrations-Blanking-Logik und der Tiefpass muessen sorgfaeltig abgestimmt werden, sonst wirkt das Spiel im Tilt-Profil unpraezise.

- Kompass am Handgelenk ist haeufig unkalibriert und neigt zu Sprüngen; deshalb nur im Panorama-Modus mit Fallback. Eine Kalibrierbewegung mitten im Store-Video sieht schlecht aus.

- Lesbarkeit und Motion Sickness: Rollender Horizont auf 1,5 Zoll kann irritieren; Kamera-Roll auf +/-15 Grad begrenzen, Pitch-Bereich klein halten, Option 'Kamera-Roll aus'.

- Sonnenzeiten und Bestenliste haengen am Telefon; ohne Verbindung gelten Standardzeiten (6:30/19:30), lokale Bestzeiten und die Tagesstrecke aus dem Datum funktionieren offline. Der Bestenlisten-Endpoint ist ein externer Betriebsaufwand fuer einen Solo-Entwickler.

- Emery-only verkleinert die Zielgruppe; eine Basalt-Portierung (144 x 168, 100 MHz, 64 KB) waere nur mit halber Aufloesung und ohne Colormap denkbar und ist bewusst nicht Ziel.

- Streckenrepetition: Eine 128er-Kachel wiederholt sich bei Sichtweite 200 in der Ferne; Nebel und diagonale Streckenfuehrung kaschieren das, ein Blick aus grosser Hoehe wuerde es zeigen, daher Maximalhoehe begrenzen.

## Aufwand

Solo, Hobby, Abende und Wochenenden, C mit SDK 4.9: (1) Voxel-Renderer mit Nebel, Roll, Himmel und Palette-Pass im Emulator: 1 bis 2 Wochenenden (ca. 15 h). (2) Weltgenerator, Kollision, Tore mit Verdeckung, Schatten, Fingerstick und Tasten-Fallback, Boost-Loop: 2 Wochen Abende (ca. 20 h). (3) Software-Synth im Stream, LRA-Muster, Backlight-Grades, Latenzkalibrierung, Hardwaretests: 1 bis 2 Wochen (ca. 15 h). (4) Sechs Strecken, Tagesstrecke, Tageszeitvarianten, Ergebnis- und Menue-UI, Panorama-Modus mit Kompass, persist, Glance, Wakeup: 2 Wochen (ca. 20 h). (5) PebbleKit JS mit Sonnenzeiten, Clay, optionale Bestenliste mit Mini-Endpoint: 1 Woche (ca. 10 h). (6) Polish, Balancing, Akku- und Performance-Messungen auf dem Geraet, Store-Assets, Video: 2 Wochen (ca. 15 h). Gesamt ca. 90 bis 120 h ueber 8 bis 12 Wochen. Ein spielbares MVP ohne JS, Kompass und Bestenliste (Punkte 1 bis 3 plus zwei Strecken) steht nach ca. 40 h und eignet sich fuer einen ersten Store-Release als 'Early Version'.

## Store-Tauglichkeit

Kategorie Games, targetPlatforms nur emery, Titelzeile im Store sinngemaess 'Comanche am Handgelenk: Voxel-Tiefflug in 64 Farben'. Drei Screenshots aus dem Emulator (200 x 228): Canyon-Tiefflug mit Schatten und Tor, Nachtflug mit Leuchttoren im Blaudunst, Hangar-Panorama mit tiefstehender Sonne. Store-Video: Handaufnahme im Halbdunkel, damit das Backlight-Grading sichtbar wird, 20 Sekunden, Sonnenuntergangsstrecke mit Boost. Verkaufsargumente: erstes Pseudo-3D-Spiel, das die neuen Time-2-Funktionen (Touch, Speaker, RGB-Backlight, LRA) zusammen einsetzt statt einzeln; laeuft ohne Telefon; 60- bis 120-Sekunden-Runden; taegliche Strecke; funktioniert mit Touch aus und stumm; keine Werbung, kein Account. Fuer die Emery-Community ist ein Emery-exklusiver Showcase ein Anreiz, die App zu zeigen; die Bestenliste und die Tagesstrecke geben Anlass zu Screenshots in Foren. Konfiguration ueber Clay, Support-Hinweis auf Steuerprofile und Akkuoptionen in der Beschreibung, Spendenlink im Ergebnisbildschirm wie bei der Theremin-App.

## Jury, Hauptkritik (6,9 von 10)

Die Schleife ist sauber und passt ans Handgelenk, und die Bodennaehe wird
dreifach gemeldet, ueber Schatten, Windrauschen und LRA-Prasseln. Der
Fingerstick ist aber schwaecher als behauptet: die Hochachse zieht die Kuppe
beim Steigen aus dem Cockpitband bis an die Horizontlinie und verdeckt genau
den Bodenschatten, also die wichtigste Information, und das HUD-Band liegt
ohnehin unter dem Finger. Der Tastenmodus mit Flappy-Hoehenmodell ist
moeglicherweise die bessere Steuerung, was fuer ein Touch-Konzept ein
schlechtes Zeichen ist. Rollender Horizont auf 1,5 Zoll und zwei Pixel breite
Tore im Dunst sind Lesbarkeitsrisiken, und nach zehn Minuten bleibt ein
solider, aber generischer Racer.

Mitnehmenswerte Einzelideen aus der Bewertung:

- Drei-Sinne-Hoehenmesser: eine Groesse (Hoehe ueber Grund) als Schattenabstand,
  Rauschpegel und LRA-Rate, mit zwei Einheiten Hysterese gegen Flattern.
- Relativer Touch statt virtuellem Joystick: der Versatz zum Aufsetzpunkt ist
  die Eingabe, es muss nichts getroffen werden.
- Das Steuerprofil wird bei jedem Fokuswechsel geprueft und mitten im Lauf
  umgeschaltet, wenn Touch systemweit ausgeht.
- Sichtweite (Nebel) als gemeinsame Stellschraube fuer Schwierigkeit und
  Rechenbudget.
- Tageszeit, Datum und echter Sonnenazimut als Inhaltsgenerator ohne Ressourcen.

## Offene Fragen des Konzepts, inzwischen beantwortet

Aus der Analyse der PebbleOS-Quellen (Commit 5503dd4, Board obelix) und aus dem
Messgeruest in `pebble/schwebung`. Diese Punkte schlagen das Konzeptpapier.

- **Framebuffer-Semantik: beantwortet, und zwar ungünstig. Am Geraet
  bestaetigt.** `graphics_release_frame_buffer` meldet immer den ganzen Puffer
  als schmutzig, und der Compositor ruft ohnehin `framebuffer_dirty_all`.
  Himmel-Caching und Dirty-Row-Minimierung sparen also Rechenzeit, aber keine
  Uebertragung. Der Kniff "Dirty-Row-Minimierung" faellt als Bandbreitentrick
  weg. Die Messung auf der Uhr bestaetigt das auf die Zehntelmillisekunde:
  ein Vollbild und zehn geaenderte Zeilen kosten beide 37,1 ms.
- **Panel-Uebertragung: die eigentliche Grenze, und inzwischen gemessen.** Die
  Bildausgabe laeuft auf KernelMain, und das ist eine hoehere Prioritaet als die
  App und als der Tonnachschub. Gemessen auf der Uhr (je 300 Bilder, siehe
  `pebble/bodeneffekt/README.md`): **ein leeres Vollbild kostet 37,1 ms**, zehn
  geaenderte Zeilen kosten exakt dasselbe, und die volle Voxel-Szene kostet
  37,4 ms bei 10,8 ms eigener Rasterzeit. Folgen: **30 fps sind nicht
  erreichbar** (die Obergrenze sind 27 fps, bevor Spielcode laeuft), das Ziel
  sind 25 fps im 40-ms-Raster — und 97 Prozent der Rasterzeit verschwinden
  hinter der Uebertragung, sodass pro Bild rund 29 ms CPU frei bleiben. Die
  Sichtweite ist damit **keine Notbremse fuer die Bildrate mehr**, sondern nur
  noch Schwierigkeitsgrad und Reserve fuer den Ton.
- **PCM-Stream: vollstaendig vermessen.** Ring 8192 Byte (256 ms bei
  16 kHz/16 Bit). Die Firmware holt alle 32 ms genau 1024 Byte, auf dem
  Systemtask mit der niedrigsten Prioritaet, unterhalb der App und unterhalb
  der Bildausgabe, und baut im Treiber keinen Vorrat auf. Eine verpasste Frist
  sind 32 ms Stille, gezaehlt wird das nirgends. Details mit Zeilenangaben in
  `pebble/schwebung/docs/firmware-befund.md`. Folgen: Vorlauf mindestens
  160 ms, Fuellstand an der Backpressure eichen, waehrend des Spiels kein
  `APP_LOG`.
- **Format: 16 kHz und 16 Bit, nicht 8 kHz und 8 Bit.** Die 8-kHz-Formate
  laufen durch einen kubischen Interpolator, der an jeder Blockgrenze einen
  Knick erzeugt. Das Speicherbudget oben rechnet noch mit 8 kHz/8 Bit und muss
  entsprechend verdoppelt werden.
- **`speaker_play_notes` und ein laufender Stream: beantwortet.** Bei gleicher
  Prioritaet wird der neue Ton abgelehnt, solange der Stream offen ist, und
  `speaker_stream_close` laesst ihn noch 80 ms ausklingen. Wer einen
  Systemklang spielen will, muss den Stream vorher wirklich beenden
  (`speaker_stop`), siehe `audio_stop_now` in `pebble/schwebung/src/c/audio.c`.
- **LRA: Mindestimpuls noch offen, Rahmenbedingungen geklaert.**
  `vibes_cancel` blockiert den App-Task 10 bis 80 ms, waehrend eines laufenden
  Musters nimmt das System kein neues an, und es gibt keinen Rueckruf am Ende.
  Das Dauerprasseln des Hoehenmessers muss deshalb aus Einzelimpulsen auf einem
  festen Zeitraster bestehen, siehe `pebble/schwebung/src/c/haptics.c`. Ob
  20-ms-Impulse getrennt ankommen, misst der LRA-Bildschirm des Messgeruests.
- **Touch-Abtastrate und Ruhe-Jitter:** misst der STIMMEN-Bildschirm des
  Messgeruests und zeigt sie live an.
- **Fingerstick gegen Tasten: gemessen, und der Fingerstick faellt durch.**
  Der DUELL-Bildschirm in `pebble/bodeneffekt` misst beide Profile ueber je
  60 Sekunden auf derselben Strecke. Auf der Uhr: Tasten halten den
  Bodeneffekt 39 Prozent der Zeit bei 26 Zellen mittlerer Hoehe, der
  Fingerstick nur 17 Prozent bei 59 Zellen — und der Schatten liegt bei ihm zu
  **77 Prozent der Zeit unter der Hand**, trotz umgekehrter Nicklage. Die
  Ursache ist nicht das Steigen, wie die Jury vermutet hat, sondern die
  Bildgeometrie: der Schatten wandert ueber das Bodeneffekt-Fenster durch die
  Zeilen 160 bis 210, und ab rund sechs Zellen Hoehe gibt es auf 228 Zeilen
  keine Fingerposition mehr, die ihn frei laesst. Der Fingerstick in der Form
  des Konzepts ist damit erledigt; Einzelheiten und die drei Auswege im README
  des Projekts.
- **`time_ms()` springt gelegentlich um plus/minus 1000 ms**, und eine
  Aufrufpause ueber einer Sekunde verschluckt in der naiven Rechnung eine ganze
  Sekunde. Fertige Loesung: `pebble/schwebung/src/c/e1clock.c`.
- **Kein Gyroskop, kein Mikrofon, kein Umgebungslicht.**
  Beschleunigungssensor und Tap-Dienst gibt es.
