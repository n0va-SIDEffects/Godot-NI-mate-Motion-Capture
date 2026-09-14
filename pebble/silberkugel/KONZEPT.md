# SILBERKUGEL, Magna-Finger-Flipper (Arbeitstitel)

Konzeptpapier aus der Ideenrunde fuer die Pebble Time 2. Jurybewertung 7,3 von 10,
bester Wert der sieben Entwuerfe nach SCHWEBUNG. Der Text ist der Stand der
Ideenrunde; die Hardware-Korrekturen am Ende sind spaeter aus den PebbleOS-Quellen
und aus dem Messgeruest in `pebble/schwebung` dazugekommen und haben Vorrang.

## Pitch

Ein Flipperautomat, dessen Gehäuse deine Uhr ist. Die seitlichen Tasten sind die Flipperknöpfe wie am echten Automaten, ein Klaps aufs Gehäuse ist der Nudge, das RGB-Backlight ist die General Illumination, der LRA das mechanische Klacken der Spulen. Neu und nur auf der Pebble Time 2 möglich: Dein Zeigefinger auf dem Glas ist ein frei platzierbarer Elektromagnet, der die Stahlkugel anzieht, mit gedrückter Select-Taste festhält, mitzieht und beim Loslassen schleudert, während der Daumen flippert und das Handgelenk den Tisch stößt. Dazu ein dunkler Tisch, auf dem die Kugel ihr Licht selbst trägt. Kein Fake, keine Gimmicks: Jede Eingabe entspricht einer physischen Handlung am echten Automaten, nur der Magnetfinger ist die Übertreibung, die die Uhr erlaubt.

## Genre

Physik-Arcade / Flipper (Pinball), Einzelspieler, Highscore-getrieben, mit täglichem Seed-Tisch und deterministischen Replays.

## Kernschleife

1. Abschuss: Kugel liegt in der Plunger-Bahn rechts. Finger auf die Bahn, nach unten ziehen (Slingshot-Geste, max. 60 px), pro 10 px ein LRA-Tick als Ratsche, loslassen = Abschuss. Skill-Shot: exakt 4 Ticks trifft die obere Lane, gibt Bonus und lädt den Magneten voll.
2. Halten: Kugel mit den Flippern (Back = links, Down = rechts, Raw-Click, Halten = Flipper oben) im Spiel halten. Nudge mit dem Handgelenk, Klaps auf die Gehäuseseite für einen gerichteten Stoß. Tilt-Bob füllt sich, Backlight warnt bernsteinfarben, zweite Warnung = TILT.
3. Laden: Drei "Spulen"-Ziele (Drop-Targets) laden den Magneten (+20 Einheiten pro Treffer, max. 100). Backlight wird grün, sobald der Magnet einsatzbereit ist.
4. Lenken: Finger aufs Glas = Magnet aktiv (Radius 64 px, Kraft ~1/(r²+r0²), Verbrauch 25/s solange die Kugel im Radius ist). Damit zieht man die Kugel auf Rampen, in Modus-Löcher, weg von den Outlanes. Unterhalb der Flipperlinie (unterste 56 px) wirkt kein Magnet: kein Endlos-Save.
5. Greifen: Select gedrückt halten, während der Finger über der Kugel ist (Abstand < 12 px), fängt die Kugel ein. Jetzt mitziehen, Select loslassen = Kugel fällt mit der Fingergeschwindigkeit (Notwurf). Kostet die volle Ladung.
6. Modus: Jeder Tisch hat 3 Missionen (Rampen-Kombo, alle Lampen, Loch-Sequenz). Mission komplett = Multiball (2–3 Kugeln), Jackpot-Ziel blinkt, Treffer = Jackpot mit Lichtshow.
7. Abfluss: Kugel weg, langer LRA-Puls, Backlight rot, DMD zeigt Bonus-Zählung. Drei Kugeln pro Spiel, dann Score, Bestenliste (persist), Tagestisch-Versuch abgebucht, AppGlance aktualisiert.
Die Physik läuft mit festem Zeitschritt in Fixed-Point, jede Partie ist aus dem Input-Log exakt reproduzierbar: Der Attract-Mode spielt den besten Ball des Spielers als Demo ab.

## Sitzungslaenge und Wiederkehr

Ein Ball dauert 20 bis 60 Sekunden, ein Spiel mit drei Kugeln 2 bis 4 Minuten, ein "Tagestisch-Versuch" ist genau ein Spiel. Wiederkommen: (a) Der Tagestisch (Seed aus dem Datum, funktioniert offline) hat jeden Tag eine andere Regelvariante (1,2-fache Schwerkraft, doppelter Magnetradius, vertauschte Flipper, Nacht-Tisch, Erdbeben-Nudges) und nur drei Versuche, die Glance zeigt "Tagestisch: 2 Versuche, Best 1.240.500". (b) Missionen pro Tisch sind in 2-Minuten-Häppchen erreichbar, aber der Jackpot-Multiball braucht ein sauberes Spiel. (c) Das eigene Best-Ball-Replay als Attract-Demo lädt ein, es zu schlagen. (d) Drei Tische mit eigener Lichtstimmung, Freischaltung über Missionsfortschritt.

## Bedienung

GRIFF ("Zangengriff"): Uhr am linken Handgelenk, rechte Hand greift die Uhr wie eine Münze: Daumen an der rechten Gehäuseseite (Up/Select/Down), Mittelfinger an der linken Seite (Back), Zeigefinger frei über dem Glas. Gegendruck von Zeigefinger und Daumen macht die Tastendrücke sogar stabiler.

STANDARD (Touch an):
- Back (Mittelfinger, linke Seite): linker Flipper. Raw-Click Down/Up, Halten = Flipper bleibt oben (Fangen der Kugel).
- Down (Daumen): rechter Flipper, ebenfalls Raw-Click.
- Select (Daumen): Magnet-Griff. Nur wirksam, solange der Zeigefinger auf dem Glas ist und die Kugel im Griffradius liegt. Halten = Kugel gefangen, Loslassen = Wurf.
- Up (Daumen): Plunger halten (Alternative zur Geste), Long-Press = Pause/Menü.
- Zeigefinger (TouchService, Rohevents Touchdown/PositionUpdate/Liftoff): Magnetposition; Plunger-Zug in der Abschussbahn über den vertikalen Pan-Recognizer (Delta seit Start = Zugweg, Geschwindigkeit beim Liftoff ist irrelevant, nur der Weg zählt); Menüs per Tap-Recognizer.
- Handgelenk (AccelerometerService, 50 Hz, Batch 2, also 25 Callbacks/s): Hochpass-Anteil = Nudge-Impuls, Tiefpass-Anteil minus Neutrallage = leichte Tischneigung (±15 % der Schwerkraft), Neutrallage wird in den ersten 500 ms nach Abschuss gemittelt.
- accel_tap_service: Klaps auf die Gehäuseseite (Achse X, Richtung ±) = kräftiger gerichteter Stoß nach links/rechts, Achse Y = Stoß nach oben/unten, Achse Z (Glas) wird ignoriert, weil das meist der Zeigefinger ist.
- Bewusst nicht belegt: Drehgeste (kein natürlicher Platz im Flipper), Doppeltasten-Kombos (mit einem Daumen nicht zuverlässig).

DAUMEN-MODUS (Konfig, für alle, denen Back als Flipper zu weit ist oder falls das System langes Halten von Back abfängt): Up = linker Flipper, Down = rechter Flipper, Select = Magnet-Griff, Back = Pause.

TOUCH AUS (touch_service_is_enabled() == false, Hinweis beim Start, wie man Touch einschaltet): Der Magnet wird zum klassischen "Magna-Save" wie bei Black Knight: Select aktiviert den fest verbauten Magneten über der Outlane, der der Kugel am nächsten ist, gleiche Ladungskosten. Plunger = Up halten (LRA-Ratsche identisch). Alle Missionen sind ohne freien Magneten lösbar, nur der Notwurf entfällt, dafür gibt es 30 % mehr Magnetladung pro Spulentreffer. Kompletter, ehrlicher Flipper ohne Touch.

LAUTSPRECHER STUMM (speaker_is_muted()): Jeder Audio-Cue hat ein LRA- oder Bildäquivalent: Bumper-Tick, Flipper-Thunk, Plunger-Ratsche, Nähe-Geiger, Jackpot-Blitz, DMD-Textlauf. Musik entfällt, das Spiel liest sich vollständig über Backlight-Farbe und DMD.

BILD VERDECKT: Eine Fingerkuppe deckt bei 202 ppi rund 10 mm = 80 px ab, also gut ein Drittel der Breite. Deshalb: Magnetradius 64 px, der leuchtende Ring ist immer außerhalb der Kuppe sichtbar. HUD oben, Flipperzone unten, der Finger arbeitet im mittleren Band. Die Kugel unter dem Finger wird gefühlt (LRA-Geiger, siehe Mechanik) und über die Motion-Blur-Spur beim Ein- und Austritt gelesen.

## Einsatz der Hardware

**touch**: Tragend. Zeigefinger = frei platzierbarer Magnet (Rohevents, Position als Kraftzentrum, 64 px Radius, Ladungsressource), Ball-Drag mit Select als Modifier, Slingshot-Plunger über den vertikalen Pan-Recognizer, Tap-Recognizer in Menüs. Occlusion ist eingeplant: Magnet-Dead-Zone in der Flipperzone, Ring außerhalb der Fingerkuppe, haptische Nähe-Anzeige. Bei ausgeschaltetem Touch wird der Magnet zum tastengesteuerten Magna-Save an festen Positionen.

**speaker**: Tragend. Eigener 4+2-stimmiger Software-Synth (2 Rechteck mit Pulsbreite, 1 Dreieck, 1 LFSR-Rauschen, 1 SFX-Stimme, 1 Rollgeräusch = Rauschen mit Tiefpass abhängig von der Kugelgeschwindigkeit), gemischt in einen 8-kHz/8-Bit-Mono-Stream über speaker_stream_open/write/close, 160 Samples pro 20-ms-Frame, 2 Frames Vorlauf (~40 ms Latenz, für Flipper unkritisch). Vorteil gegenüber speaker_play_notes: Musik und SFX gleichzeitig ohne Preemption, Pitch-Bends in Echtzeit. Fallback, falls der Stream-Puffer zickt: Musik nur im Attract/zwischen Bällen per speaker_play_tracks, SFX per speaker_play_notes (Bumper = Rechteck MIDI 96, 30 ms; Flipper = Sägezahn MIDI 40, 20 ms; Abfluss = absteigende Tonleiter). Bei speaker_is_muted() komplett verzichtbar.

**rgb_backlight**: Tragend als General Illumination und als Spielinformation. light_set_color_rgb888 pro Zustand: weiß = normal, grün = Magnet geladen, blau = Multiball bereit, bernstein = Tilt-Warnung, rot = Tilt/Abfluss; Jackpot = 500 ms Sequenz weiß-gold-weiß im Frame-Takt, synchron mit dem Palette-Remap des Framebuffers, sodass Bild und physisches Licht dieselbe Farbe zeigen und der Schein auf dem Handgelenk im Dunkeln mitspielt. Drei Einstellungen wegen Akku: Lichtshow dauerhaft (Standard, 3 Minuten pro Spiel sind vertretbar), nur bei Ereignissen (light_enable_interaction plus kurze Farbfenster), aus.

**lra_haptics**: Tragend, der LRA ist die Mechanik des Automaten. vibes_enqueue_custom_pattern mit kurzen Segmenten: Flipper-Thunk 15 ms, Bumper-Tick 10 ms, Slingshot-Gummi 8 ms, Plunger-Ratsche 8 ms pro 10 px Zugweg, Abfluss 400 ms, Tilt-Warnung Doppelpuls, Nähe-Geiger: 10-ms-Ticks, deren Abstand mit der Distanz Kugel-Finger von 400 ms auf 60 ms sinkt. Rate-Limiter: min. 40 ms zwischen Aufrufen, Prioritätsstufen (Abfluss schlägt Bumper), vibes_cancel vor höherprioren Mustern. Wichtig: Jede Vibration erscheint im Accel als Stoß, deshalb Nudge-Erkennung 60 ms nach jedem vibes-Aufruf maskieren (plus das did_vibrate-Flag der AccelData als zweites Netz).

**heart_rate**: Nicht als Spielmechanik genutzt, weil der gefilterte BPM-Wert bis zu 15 Minuten alt sein kann, der Rohwert bestenfalls im Sekundenabstand kommt und health_service_set_heart_rate_sample_period nur ein Vorschlag ist: In einem 30-Sekunden-Ball ist das kein steuerbarer Input, und eine Mechanik, die manchmal reagiert, fühlt sich kaputt an. Optional, standardmäßig aus: Statistik 'Puls vor/nach dem Spiel' auf dem Score-Screen aus HealthMetricHeartRateRawBPM, nur angezeigt, wenn ein frischer Wert per HealthEventHeartRateUpdate während des Spiels eintraf.

**compass**: Nicht genutzt, weil ein Flippertisch keine Himmelsrichtung hat, das Handgelenk beim Spielen ständig dreht (Heading würde als Störgröße in den Nudge einfließen), Kalibrierungsaufforderungen die 2-Minuten-Session unterbrechen würden und jede Kompass-Idee (Tisch dreht sich mit dem Körper) das Spiel schlechter, nicht besser machen würde.

**accelerometer**: Tragend, drei Rollen aus einem Datenstrom (50 Hz, Batch 2): (1) Nudge = Hochpass (Sample minus gleitender Mittelwert über 400 ms) in X/Y, ab 150 mg als Impuls auf alle Kugeln, Betrag geht in den Tilt-Bob (Abkling 30 %/s, Warnung ab 600, Tilt ab 1000). (2) Neigung = Tiefpass minus Neutrallage, begrenzt auf ±150 mg, verschiebt den Schwerkraftvektor des Tisches, wie das Anheben eines Automatenbeins. (3) Klaps = accel_tap_service mit Achse und Richtung als starker gerichteter Stoß, kostet 250 Tilt-Bob, ist also maximal zwei- bis dreimal pro Ball drin.

**cpu_240mhz**: Erlaubt das, was auf der Pebble Time nicht ging: Fixed-Point-Physik (Q20.12 in int32) mit 200-Hz-Substeps für bis zu 3 Kugeln gegen ~180 Segmente, 12 Kreise, 2 rotierende Flipper-Kapseln mit Winkelgeschwindigkeits-Übertrag, Broadphase über ein 8x8-Zellengitter; dazu der Software-Synth, das Licht-Rendering per LUT und der Vollbild-Palette-Remap. Gesamtlast im Worst Case ~1,3 ms pro 20-ms-Frame, also unter 10 %. Der Rest der Zeit gehört der Display-Übertragung, deshalb ist Dirty-Row-Disziplin wichtiger als Rechenleistung.

**display_64c_mip**: 200x228 in 64 Farben, ohne Ghosting, sonnenlicht-lesbar: Man kann draußen im Freien flippern, wo ein Handydisplay spiegelt. Tischgrafik als 4-Bit-Bitmap mit tischeigener 16-Farben-Palette aus den 64 GColor8-Werten, beim Build mit Bayer-8x8 gedithert (bei 202 ppi unsichtbar), dynamisches Licht per Shade-LUT, GI-Flashes per Vollbild-Remap, Kamera mit Hysterese, damit die meisten Frames unter 60 geänderte Zeilen bleiben und die zeilenweise Übertragung kurz ist.

**phone_js**: Unterstützend, nicht nötig zum Spielen. (1) Clay-Konfigurationsseite: Tastenbelegung (Zangengriff/Daumen-Modus), Lichtshow-Modus, Haptik-Umfang, Tagestisch-Erinnerung an/aus. (2) Tagestisch-Seed aus dem Datum wird lokal berechnet, die JS-Seite liefert nur optional eine Bestenliste per XHR an ein kleines Backend (Phase 2, Score plus Input-Log-Hash zur Plausibilisierung). (3) Phase 2: Ghost-Duell, weil die Physik deterministisch ist: Das Input-Log eines Spiels (typisch 3 bis 6 KB, Events nur bei Änderung, Touch mit 25 Hz nur bei Kontakt) passt in wenige AppMessages; die Uhr spielt den Ghost als halbtransparente zweite Kugel ab. Kein Echtzeit-Multiplayer.

**worker_wakeup_glance**: AppGlance: Nach jedem Spiel app_glance_reload mit Bestwert, Tagestisch-Restversuchen und einem Icon des freigeschalteten Tisches, sodass der Launcher zur Einladung wird. Wakeup: nicht genutzt, ein Wecker 'Dein Tagestisch wartet' wäre Störung, keine Mechanik. AppWorker: nicht genutzt, weil es nichts gibt, was im Hintergrund sinnvoll laufen müsste (eine Wrist-Flick-Startgeste per Worker wäre ein Gimmick, das dauerhaft Accel-Strom zieht).

## Grafik-Kniffe

### Nacht-Tisch: Kugel und Finger als Lichtquellen (radiale LUT + Shade-LUT)

**how**: Im Nachtmodus wird der 4-Bit-Tisch grundsätzlich mit Helligkeitsstufe 3/16 gezeichnet. Um jede Kugel ein 40x40-Lichtfeld: pro Pixel Helligkeit = radialLUT[dy][dx] (1.600 Byte, 16 Stufen, Falloff quadratisch, einmal beim Start berechnet), dazu Bayer-4x4-Schwelle (16 Byte) auf die Stufe addiert, um 16 Stufen wie 64 wirken zu lassen. Dann Farbe = shadeLUT[stufe][paletteIndex]: Da der Tisch nur 16 Palettenindizes hat, ist die Tabelle 16x16 = 256 Byte GColor8, vorberechnet aus den RGB-Werten der Tischpalette mit Nachbarschaftssuche in den 64 Farben. Schreiben direkt in den 8-Bit-Framebuffer (graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit), gbitmap_get_data, gbitmap_get_bytes_per_row, danach graphics_release_frame_buffer). Der Magnetring des Fingers nutzt dieselbe Routine mit einer zweiten, blau getönten Shade-LUT (256 Byte), als Ring (Radius 56 bis 64 px) statt Scheibe, Helligkeit pulsiert mit der Ladung.

**why_impressive**: Dynamische Beleuchtung sieht man auf keiner Uhr. Die Kugel erhellt beim Vorbeirollen die Tischgrafik, Rampen tauchen aus dem Dunkel auf, im Multiball wandern drei Lichtkegel. Der Ring um die eigene Fingerkuppe verkauft den Finger als Objekt im Spiel, obwohl er die Mitte verdeckt.

**cost**: RAM: 1.600 + 512 + 16 Byte. CPU: 1.600 Pixel x ~7 Zyklen = ~11k Zyklen pro Licht, bei 3 Kugeln plus Finger ~45k Zyklen = 0,19 ms pro Frame. Kein Ressourcenbedarf.

### GI-Flash: Vollbild-Palette-Remap synchron zum RGB-Backlight

**how**: Acht vorberechnete 256-Byte-Remap-Tabellen (weiß-blitz, gold, rot, bernstein, blau, 25/50/75 % gedimmt), jeweils berechnet aus den 64 GColor8-Werten: Für Tönungen wird jede Farbe in RGB mit der Zielfarbe gemischt und auf die nächste der 64 Farben gerundet, für Dimmung mit Faktor skaliert. Bei aktivem Flash läuft nach dem Zeichnen ein Pass über alle 45.600 Framebuffer-Bytes: fb[i] = lut[fb[i]], mit 4 Bytes pro Schleifendurchlauf entrollt. Gleichzeitig light_set_color_rgb888 mit exakt der Tönungsfarbe des aktuellen Frames, also z. B. Jackpot = Frames 0-3 weiß, 4-12 gold, 13-25 Ausblenden in 4 Stufen zurück auf weiß.

**why_impressive**: Ein 50-fps-Color-Grade des ganzen Bildes plus physisches Licht in derselben Farbe: Der Tisch und die Umgebung des Handgelenks blitzen gemeinsam, wie die General Illumination eines echten Automaten. Weil das MiP-Panel vor dem Backlight liegt, verstärken sich Remap und Backlight, statt sich zu widersprechen. Das ist der Moment, den man im Dunkeln filmt.

**cost**: RAM: 2 KB LUTs. CPU: 45.600 x ~3 Zyklen = ~137k Zyklen = 0,57 ms pro Frame, nur während Flashes (typisch 0,3 bis 0,5 s). Akku: Backlight-Farbwechsel kosten nichts extra, solange das Licht ohnehin an ist; im Modus 'nur Ereignisse' geht es je Flash 500 ms an.

### Subpixel-Kugel mit Handgelenk-Glanzlicht und Fake-Motion-Blur

**how**: Die 6-px-Kugel wird nicht als eine Bitmap, sondern als 16 Phasen (4x4 Viertelpixel-Offsets) eines 8x8-8-Bit-Sprites gehalten (1.024 Byte), beim Start aus einer Kugelformel generiert: Normale aus Pixelposition, Diffus plus Spekular, Farbe aus einer 16-stufigen Grau/Stahl-Rampe in den 64 Farben, Randpixel über Flächendeckung in dunklere Stufen (Antialiasing in Farbstufen statt Alpha, weil GCompOpSet nur Palettentransparenz kennt). Das Glanzlicht sitzt nicht fest: Alle 4 Frames werden die 16 Phasen neu generiert, wobei die Lichtrichtung aus dem tiefpass-gefilterten Accel-Vektor kommt (1.024 Pixel x ~40 Zyklen = 41k Zyklen alle 80 ms). Fake-Motion-Blur: Vor der Kugel werden ihre letzten drei Positionen als Geister gezeichnet, indem die Tischpixel dort über shadeLUT[Stufe 10/7/5] aufgehellt werden, ohne eigene Sprites.

**why_impressive**: Bei 202 ppi wirkt eine 6-px-Kugel in Ganzpixel-Schritten wie ein zitternder Punkt. Mit Viertelpixel-Phasen gleitet sie, die Spur füllt die 8-px-Lücken zwischen zwei Frames bei 400 px/s und liest sich als Geschwindigkeit, und das Glanzlicht, das beim Kippen des Handgelenks über die Kugel wandert, macht aus dem Punkt eine Stahlkugel. Das ist derselbe Trick wie ein Blender-Environment-Reflection, nur mit einer Tabelle.

**cost**: RAM: 1 KB Sprites plus 3 Positionen pro Kugel. CPU: ~64 + 3x28 Pixel pro Kugel pro Frame = ~2k Zyklen, plus 41k Zyklen alle 4 Frames für die Regeneration. Kein Ressourcenbedarf.

### Dirty-Row-Regie: statisches HUD, Restore-Rects, Kamera mit Hysterese, 4-Bit-Unpack per Doppelbyte-LUT

**how**: Zwei Layer: Das DMD-HUD (obere 20 Zeilen) ist ein eigener Layer, layer_mark_dirty nur bei Score-Änderung. Der Tisch-Layer führt eine Liste von Dirty-Rects (alte und neue Kugelposition inkl. Spur und Licht, Flipper-Bounding-Boxen nur bei Bewegung, blinkende Lampen, Magnetring) und restauriert nur dort den Hintergrund aus der 4-Bit-Tischbitmap (200x400, 40.000 Byte im Heap). Die Kamera folgt der untersten Kugel nur, wenn sie ein 60-px-Totband verlässt, und in Ganzpixel-Schritten; auf Scroll-Frames wird das ganze Sichtfenster neu entpackt: Ein 4-Bit-Byte wird über eine 256-Einträge-LUT auf zwei GColor8 abgebildet und als uint16 geschrieben (512-Byte-LUT, 20.800 Eingabebytes pro Vollbild). Im Nachtmodus läuft der Unpack durch die gedimmte Variante derselben LUT.

**why_impressive**: Es ist der Grund, warum 50 fps auf einem zeilenweise übertragenen MiP überhaupt gehen: In einem typischen Frame ändern sich 20 bis 50 von 228 Zeilen, die Übertragung ist entsprechend kurz. Für den Spieler heißt das eine Kugel ohne Ruckeln, während das HUD wie bei einem echten Automaten still steht.

**cost**: RAM: 40 KB Tischbitmap (die größte Einzelposition), 512 Byte LUT, ~200 Byte Rect-Liste. CPU: typischer Frame ~2.000 Pixel Restore = ~10k Zyklen; Scroll-Frame ~20.800 Bytes x ~5 Zyklen = ~104k Zyklen = 0,43 ms. Zu verifizieren: ob das System bei Framebuffer-Direktzugriff die geänderten Zeilen selbst erkennt oder das Dirty-Rect des Layers überträgt; das Layout ist so gebaut, dass beides funktioniert.

### Build-Time-Bayer: in Blender gerenderte Tische, auf 16 Farben pro Tisch quantisiert

**how**: Mike rendert jeden Tisch in Blender (Plastiken, Rampen, Lackglanz, Ambient Occlusion) in 200x400. Ein Python-Skript im Build (wscript-Hook) wählt per Median-Cut eine tischspezifische 16er-Palette, snapt jede Palettenfarbe auf die nächste der 64 GColor8-Werte, dithert das Bild mit einer 8x8-Bayer-Matrix gegen diese Palette und schreibt eine 4-Bit-palettierte PBI plus die Palette als Raw-Ressource (resource_load_byte_range). Zusätzlich exportiert dasselbe Blender-File die Kollisionsgeometrie: Kanten eines Kurvenobjekts als Segmentliste (int16 x1,y1,x2,y2 = 8 Byte pro Segment), Kreise, Lampenpositionen, Flipper-Drehpunkte. Grafik und Physik kommen aus einer Quelle, die immer zusammenpassen.

**why_impressive**: Ein 3D-gerenderter Flippertisch mit Verläufen, Schatten und Glanz auf einem 64-Farben-Display. Bei 202 ppi verschmilzt das Bayer-Muster zu Verläufen; im Store-Screenshot (200x228 hochskaliert) ist es als Stilmittel sichtbar und wirkt wie bewusstes Pixel-Art-Dithering. Null Laufzeitkosten.

**cost**: Ressourcen: 40 KB pro Tisch plus ~2 KB Geometrie und Lampen. RAM: nur der aktive Tisch. CPU: keine. Produktionsaufwand: ein bis zwei Wochenenden pro Tisch, dafür in Mikes Kernwerkzeug.

### Flipper als antialiaste GPath-Kapseln, DMD-HUD als Punktmatrix

**how**: Die beiden Flipper sind die einzigen Elemente mit graphics_context_set_antialiased(true): GPath mit 6 Punkten, gpath_rotate_to mit dem Physikwinkel (0 bis 45 Grad in 60 ms), Füllung in zwei Farbtönen (Kante dunkler) und ein 1-px-Glanzstrich. Das HUD emuliert eine Dot-Matrix-Anzeige: 5x7-Font (480 Byte, 1 Bit), jeder Punkt als 1-px-Bernsteinpixel mit 1-px-Lücke auf schwarz, aktive Punkte in GColorOrange, inaktive in einem sehr dunklen Braun aus den 64 Farben, sodass das Raster schwach sichtbar bleibt wie bei echten DMDs. Textlauf und Score-Zählung als reine Punktmuster.

**why_impressive**: Die beweglichsten Teile des Bildes sind die glattesten, das Auge liest die Flipper als mechanisch präzise; das DMD ist ein sofort erkennbares Flipper-Zitat und bleibt statisch, bis sich der Score ändert.

**cost**: RAM: ~100 Byte GPath, 480 Byte Font. CPU: 2 GPath-Füllungen mit AA ~30k Zyklen pro Frame, nur wenn sich ein Flipper bewegt; DMD nur bei Änderung.

## Mechanik-Kniffe

- Magna-Finger mit LRA-Geigerzähler: Der Finger ist ein Magnet mit Radius 64 px, aber die Fingerkuppe verdeckt 80 px. Deshalb wird die Kugel unter dem Finger gefühlt statt gesehen: 10-ms-LRA-Ticks, deren Abstand von 400 ms (Kugel am Ringrand) auf 60 ms (Kugel direkt unter der Kuppe) sinkt. Der Spieler lernt in zwei Bällen, die Kugel blind unter dem Finger zu halten, weil der LRA direkt unter der Fingerkuppe sitzt: Die Uhr tippt dir buchstäblich gegen den Finger. Unterhalb der Flipperlinie ist der Magnet wirkungslos, damit die Hand nie über den Flippern hängt und es keinen Endlos-Save gibt.

- Physischer Nudge mit Tilt-Bob: Der Hochpass des Accel-Signals ist 1:1 der Stoß gegen den Automaten, ein Klaps auf die Gehäuseseite (accel_tap mit Achse X und Vorzeichen) ist der gerichtete Schlag gegen die Seitenwand. Der Tilt-Bob (Summe der Stoßbeträge mit 30 %/s Abkling) bestraft Dauerruckeln exakt wie das Pendel im echten Gerät: Bernstein-Backlight als Warnung, Rot als TILT mit toten Flippern. Dazu die Tiefpass-Neigung als leises Schummeln (Automatenbein anheben): kleine, dauerhafte Verschiebung der Schwerkraft, die zugleich den Bob langsam füllt. Technischer Kniff: Jede LRA-Vibration sieht im Accel aus wie ein Stoß; 60 ms Maske nach jedem vibes-Aufruf plus did_vibrate-Flag verhindern, dass der Flipper-Thunk zum Selbst-Nudge wird.

- Ball-Drag = Ziehen-und-Halten plus Klick: Select gedrückt halten, während der Finger über der Kugel steht, fängt sie (Abstand < 12 px, Ladung > 30). Jetzt zieht man die Kugel mit dem Finger, spürt sie als Dauerpurren (10 ms an / 40 ms aus, wieder-enqueued), und beim Loslassen von Select fällt sie mit der Geschwindigkeit des Fingers aus den letzten drei PositionUpdates (Zeitstempel per time_ms beim Event-Eingang, weil das TouchEvent keinen trägt). Der Notwurf kostet die gesamte Ladung, ist also eine strategische Karte, keine Dauerhilfe. Der Modifier funktioniert nur im Zangengriff, das ist genau die Hand-Koordination, die es auf keinem Touch-Telefon gibt.

- Slingshot-Plunger, der über Ticks zählbar ist: Der Finger zieht den Plunger in der Abschussbahn nach unten (vertikaler Pan-Recognizer, Delta seit Start), pro 10 px ein 8-ms-Ratschen-Tick, maximal 6. Weil der Finger die Bahn verdeckt, zählt man Ticks statt zu schauen; der Skill-Shot verlangt exakt 4. Die identische Ratsche läuft beim Tasten-Fallback (Up halten), sodass die Fertigkeit übertragbar bleibt.

- Backlight-Farbe als HUD, das man nicht ansehen muss: Grün = Magnet bereit, Blau = Multiball scharf, Bernstein = ein Stoß zu viel, Rot = weg. Im Flipper hat man keine Zeit für ein HUD, aber der Farbschein im Augenwinkel (und im Dunkeln auf dem Handgelenk) ist unmittelbar lesbar. Das Palette-Remap des Bildes übernimmt dieselbe Farbe, damit die Information auch bei ausgeschaltetem Licht im Bild steht.

- Deterministische Fixed-Point-Physik als Spielfeature: Fester 5-ms-Zeitschritt, int32 Q20.12, keine Floats, Inputs pro Frame quantisiert und geloggt (Tasten als Events, Touch mit 25 Hz nur bei Kontakt, typisch 3 bis 6 KB pro Spiel, 8-KB-Ring). Daraus entstehen: Attract-Mode mit dem eigenen besten Ball, Tagestisch mit gleichen Bedingungen für alle, Ghost-Duell über AppMessage in Phase 2 und ein Replay-basierter Physik-Regressionstest für den Entwickler. Ein Bugfix, der das Replay verändert, fällt sofort auf.

- Gehäuse als Cabinet: Back auf der linken und Down auf der rechten Gehäuseseite liegen genau dort, wo am Automaten die Flipperknöpfe sitzen. Raw-Click-Down/Up statt Single-Click, damit Halten (Kugel fangen) und Post-Pass (kurz loslassen, wieder drücken) funktionieren; Repeat-Click wird bewusst nicht benutzt. Der Flipper-Thunk im LRA fällt auf den Down-Event, nicht auf den Kontakt mit der Kugel, weil der echte Automat auch bei Leerschlägen knallt.

## Risiken

- Back-Taste als Flipper: Falls das System langes Halten von Back systemweit abfängt (Rückkehr zur Watchface) oder Raw-Clicks auf Back einschränkt, fällt der Zangengriff auf den Daumen-Modus (Up/Down) zurück. Früh auf echter Hardware testen; im Back-Modus ggf. Auto-Release des Flippers nach 1,2 s.

- Touch-Abtastrate und -Latenz sind nicht dokumentiert. Der Magnet verträgt 50 bis 80 ms Verzug, der Notwurf braucht drei brauchbare PositionUpdates in ~100 ms für die Geschwindigkeitsschätzung. Bei zu grober Abtastung: Wurfgeschwindigkeit aus dem Pan-Recognizer (px/s) statt aus Rohevents ableiten oder den Wurf auf feste Stärke setzen.

- LRA und Accel koppeln: Vibrationen erscheinen als Stöße. Maske 60 ms plus did_vibrate-Flag sind eingeplant; falls die LRA-Nachschwingung länger stört, Maske verlängern und Nudge-Schwelle nach Vibration anheben. Zudem unklar, ob 8- bis 15-ms-Segmente in vibes_enqueue_custom_pattern sauber ausgeführt werden; sonst auf 20 ms gehen.

- accel_tap-Empfindlichkeit ist nicht konfigurierbar. Möglich, dass Flipper-Tastendrücke oder das eigene LRA Taps auslösen; Gegenmaßnahme: Taps nur außerhalb der Vibrationsmaske und nicht innerhalb 80 ms nach einem Tastendruck werten. Wenn zu unzuverlässig: Klaps entfällt, Hochpass-Nudge reicht.

- Speaker-Stream: Puffergröße, Latenz und Underrun-Verhalten von speaker_stream_write sind nicht dokumentiert. Plan B ist gebaut: SFX über speaker_play_notes, Musik nur im Attract und zwischen Bällen über speaker_play_tracks, weil play_notes vermutlich laufende Wiedergabe preemptet.

- Heap-Enge: 116 von 128 KB kalkuliert, Compiler-Overhead und System-Heap-Fragmentierung sind unbekannt. Notfallplan (kleinerer Tisch, kleinerer Replay-Ring, Pattern nachladen) ist definiert; im Zweifel 2 statt 3 Tische im ersten Release.

- Display-Übertragung: Es ist nicht dokumentiert, ob das System bei Framebuffer-Direktzugriff geänderte Zeilen selbst erkennt oder das Layer-Dirty-Rect überträgt. Beides ist berücksichtigt, aber die tatsächliche Vollbild-Übertragungszeit entscheidet, ob Scroll-Frames 50 oder 30 fps erreichen. Erste Messung im Prototyp.

- Neutrallage des Handgelenks: Wer im Gehen spielt oder die Hand ständig bewegt, produziert Dauer-Nudges und Tilt. Das ist als Spielregel akzeptabel (Flipper spielt man im Stehen oder Sitzen), muss aber im Onboarding gesagt werden; Kalibrierung nach Abschuss und eine Empfindlichkeitsstufe in der Konfiguration.

- Touch ausgeschaltet: Der Magna-Save-Fallback ist vollständig, aber der Notwurf und das freie Lenken fehlen; ein Teil des Reizes geht verloren. Der Start-Hinweis muss erklären, was man mit eingeschaltetem Touch gewinnt, ohne zu nerven.

- Akku: Dauerhaftes Backlight in einer 3-Minuten-Session ist vertretbar, aber wer 20 Spiele am Tag macht, merkt es. Standardmodus 'Lichtshow an' mit sichtbarem Hinweis, Modus 'nur Ereignisse' als sparsame Alternative.

- Physik-Tuning ist der eigentliche Aufwandstreiber: Flipper-Übertrag, Rampenübergänge, Kugel-Tunneling bei hohem Tempo. Feste 200-Hz-Substeps und Kapsel-Kollisionen sind geplant, trotzdem sind mehrere Wochen Feintuning realistisch.

- Fingerkuppe verdeckt ein Drittel der Breite: Trotz Geiger-LRA und Ring könnten manche Spieler den Magneten als frustrierend empfinden. Der Magnet ist deshalb optional-strategisch, nicht Pflicht; alle Missionen sind ohne ihn lösbar.

## Aufwand

Solo, Hobbytempo (8 bis 10 Stunden pro Woche), Vorwissen aus der Theremin-App für Speaker und Build vorhanden.
Phase 1, Prototyp (3 bis 4 Wochen): Fixed-Point-Physik, ein Tisch als graue Testgrafik, Flipper per Raw-Click, Plunger, Hochpass-Nudge, Magnetfinger, Dirty-Rect-Renderer, Messung von Frame- und Übertragungszeiten auf Hardware. Entscheidung Back- vs. Daumen-Modus.
Phase 2, Spielbar (4 bis 6 Wochen): Erster Tisch aus Blender, Build-Skript für Quantisierung und Geometrie-Export, Ball-Drag, accel_tap, Tilt-Bob, LRA-Vokabular, Nacht-Tisch-Licht, GI-Remap plus Backlight, DMD-HUD, Missionen, Multiball.
Phase 3, Klang und Verpackung (3 bis 4 Wochen): Software-Synth über Stream (oder Fallback), drei Songs, SFX, Attract-Mode mit Replay, Tagestisch, Persist, AppGlance, Clay-Konfiguration, Onboarding für Griff und Touch-Hinweis, Store-Screenshots und Video.
Phase 4, Zweiter und dritter Tisch (je 2 Wochen): überwiegend Blender-Arbeit plus Missionsdesign.
Gesamt: rund 4 bis 5 Monate bis Store-Release mit zwei Tischen, ~7.000 bis 9.000 Zeilen C plus ~500 Zeilen Python im Build. Ghost-Duell und Online-Bestenliste sind bewusst Phase 5 (Backend nötig).

## Store-Tauglichkeit

Flipper ist eine der wenigen Arcade-Gattungen, die auf Handhelds seit dem Game Boy durchgehend funktioniert und die auf Pebble bisher nur als Tasten-Skizze existierte. Auf der Pebble Time 2 hat das Spiel ein Alleinstellungsmerkmal, das kein Telefon-Flipper bietet: echte seitliche Tasten an einem Gehäuse, das man wie einen Automaten stößt, dazu einen LRA für das mechanische Gefühl und ein farbiges Backlight als General Illumination. Der Magnetfinger ist die Neuheit, die im Store-Video sofort erklärt, warum diese Uhr Touch hat. Die Screenshots tragen sich selbst (dunkler Tisch, leuchtende Kugel, goldener Jackpot-Blitz), das Video braucht nur eine dunkle Ecke und ein Handgelenk. Als emery-exklusive App, die Touch, Speaker, RGB-Backlight, LRA und Accel gleichzeitig ausreizt, ist es ein natürlicher Showcase-Kandidat für Core Devices. Begrenzt wird die Reichweite durch die PT2-Nutzerbasis und die Kategorie Games, in der nur wenige Titel Bewertungen sammeln; dafür ist der Titel der Sorte, die in einer Community wie r/pebble organisch weitergereicht wird. Zielgruppe: Leute, die die PT2 wegen der Hardware gekauft haben und ein Spiel wollen, das man in der Kaffeepause in 3 Minuten spielt und Freunden am Handgelenk zeigt.

## Wow moment

Gefilmt in einem dunklen Raum, Uhr am Handgelenk: Nacht-Tisch, alles schwarz bis auf die Kugel, die als gleitendes Licht mit Glanzlicht und Leuchtspur über die Rampen zieht und dabei die Tischgrafik aus dem Dunkel holt. Ein Zeigefinger legt sich aufs Glas, um die Kuppe erscheint ein blau pulsierender Ring, die Kugel biegt sichtbar aus ihrer Bahn und schmiegt sich unter den Finger, man hört und sieht förmlich das Ticken des LRA. Der Daumen drückt Select, der Finger zieht die Kugel drei Zentimeter nach links, lässt los, die Kugel fliegt in das blinkende Jackpot-Loch. In demselben Frame blitzt der gesamte Bildschirm weiß und geht in Gold über, das Backlight färbt das Handgelenk und den Ärmel golden mit, das DMD scrollt JACKPOT, der LRA knallt. Fünf Sekunden Store-Video, ein Standbild für den ersten Screenshot.

## Memory budget

CODE + HEAP (Limit 128 KB):
Code (Physik, Renderer, Synth, Spiel-Logik, Menüs, DMD): ~45 KB (vergleichbare Pebble-Apps mit eigenem Rasterizer liegen bei 30 bis 50 KB).
Heap:
- Tisch-Hintergrund 200x400 @4 Bit, nur der aktive Tisch: 40.000 B
- Tisch-Palette 16 x GColor8, Kollisionsgeometrie (~180 Segmente x 8 B, 12 Kreise x 8 B, 24 Ziele/Lampen x 6 B), Flipper-Definition: ~1,8 KB
- Kugel-Sprites 16 Phasen x 8x8 @8 Bit: 1.024 B; Lampen-/Zielsprites 40 x 8x8 @2 Bit: 640 B; DMD-Font 5x7: 480 B; Flipper-GPath: ~100 B
- LUTs: Shade 16x16 x 2 Tönungen = 512 B; Radial-Licht 40x40 = 1.600 B; GI-Remap 8 x 256 B = 2.048 B; 4-Bit-Unpack 512 B; Bayer 4x4: 16 B
- Synth: 2 x 320 B Ringpuffer, Voice-States ~200 B, aktuelle Pattern-Daten ~4 KB
- Accel-Batch 25 x 8 B: 200 B; Replay-Ring: 8 KB
- Spielzustand, Dirty-Rect-Liste, Missionen, Menüs: ~4 KB
- Stack und Systemreserve: ~6 KB
Heap gesamt: ~71 KB. Code + Heap: ~116 KB von 128 KB, also ~12 KB Luft. Notfallplan, falls der Code größer wird: Tisch auf 200x360 (36 KB), Replay-Ring auf 4 KB, Pattern-Daten pro Song nachladen.

RESSOURCEN (Limit 256 KB):
- 3 Tische x (40.000 B PBI 4 Bit + ~2 KB Geometrie/Palette/Lampen als Raw-Ressource) = ~126 KB
- Sprites, Lampen, Menügrafik, Icons (Launcher, Glance): ~10 KB
- Musik: 3 Songs als Tracker-Pattern (Note, Instrument, Dauer) ~4 KB je + 1 KB Instrumentdefinitionen = ~13 KB
- PCM-Samples 8 kHz/8 Bit für Dinge, die der Synth nicht kann: Knocker 60 ms (480 B), Kugel-in-Loch, Startjingle-Stimme, Tilt-Ansage: ~6 KB
- Texte, Missionsdaten, Tagestisch-Regeltabellen: ~2 KB
Ressourcen gesamt: ~157 KB, ~99 KB Reserve für einen vierten Tisch oder feinere Grafik.

PERSIST: Bestenliste 10 x 12 B, Missionsfortschritt 3 Tische, Konfiguration, Tagestisch-Datum/Versuche, Best-Ball-Replay komprimiert in 256-Byte-Schlüsseln (max. 3 KB): ~3,5 KB von ~4 KB.

## Fps estimate

Ziel 50 fps (AppTimer alle 20 ms), Physik fest mit 200 Hz (4 Substeps pro Frame, Catch-up bei verspäteten Frames, damit die Spielgeschwindigkeit konstant bleibt).
CPU pro Frame bei 240 MHz, Worst Case (3 Kugeln, Scroll-Frame, GI-Flash aktiv, Magnet an): Physik 3 Kugeln x 4 Substeps x ~54 Broadphase-Tests x ~40 Zyklen = ~26k; Vollbild-Unpack ~104k; drei Kugellichter plus Magnetring ~45k; Kugeln, Spuren, Flipper mit AA ~35k; GI-Remap ~137k; Synth 160 Samples x 6 Stimmen x ~8 Zyklen = ~8k; Summe ~355k Zyklen = ~1,5 ms, also unter 8 % des Frames. Typischer Frame (statische Kamera, eine Kugel, kein Flash): ~60k Zyklen = 0,25 ms.
Der limitierende Faktor ist nicht die CPU, sondern die zeilenweise Übertragung zum MiP-Panel. Im Standardfall (statische Kamera, 20 bis 50 geänderte Zeilen) sind 50 fps realistisch. Auf Scroll- und Flash-Frames wird das Vollbild übertragen; da das Animation-Framework des Systems Vollbilder mit ~30 fps schafft, ist im schlechtesten Fall mit 30 fps für diese Frames zu rechnen, die feste Physikrate kaschiert den Sprung. Ergebnis: 50 fps in ~85 % der Frames, garantiert nicht unter 30 fps.

## Jury, Hauptkritik (7,3 von 10)

Der Magnetfinger widerspricht der wichtigsten Regel des Genres: die Kugel sehen.
Die Fingerkuppe deckt rund ein Drittel der Breite genau in dem mittleren Band ab,
in dem die sechs Pixel grosse Kugel am meisten unterwegs ist. Der LRA-Geigerzaehler
ist ein kluger Ersatz, aber "blind unter dem Finger halten" ist bis zu einem
Spieltest eine Behauptung. Der Zangengriff ist unerprobt, und dass der
Fallback ohne Touch ein vollstaendiger Flipper ist, rettet viel, verraet aber
auch, dass der Touch-Kern optional ist.

Mitnehmenswerte Einzelideen aus der Bewertung:

- LRA-Geigerzaehler: der Tick-Abstand kodiert die Entfernung eines Objekts unter
  der verdeckenden Fingerkuppe, von 400 ms bis 60 ms.
- Vibrationsmaske: 60 ms nach jedem Vibrationsaufruf plus ein Flag, damit die
  eigenen Impulse nicht als Beschleunigung (Nudge, Tap) gelesen werden.
- Zaehlbare LRA-Ratsche fuer Gesten, die der Finger verdeckt.
- Tagestisch mit taeglicher Regelvariante, drei Versuchen, Glance-Anzeige und
  deterministischem Replay.

## Hardware-Korrekturen nach dem Konzept

Aus der Analyse der PebbleOS-Quellen (Commit 5503dd4, Board obelix) und dem
Messgeruest in `pebble/schwebung`. Diese Punkte schlagen das Konzeptpapier.

- **Ton: 16 kHz und 16 Bit, nicht 8 kHz und 8 Bit.** Die 8-kHz-Formate laufen
  durch einen kubischen Interpolator, der an jeder Blockgrenze einen Knick
  erzeugt. `SpeakerPcmFormat_16kHz_16bit` ist der direkte Weg.
- **Der Tonnachschub ist das knappste Gut der Uhr.** Die Firmware holt alle
  32 ms genau 1024 Byte, auf dem Systemtask mit der niedrigsten Prioritaet,
  unterhalb der App und unterhalb der Bildausgabe. Eine verpasste Frist sind
  32 ms Stille, und sie wird nirgends gezaehlt. Details in
  `pebble/schwebung/docs/firmware-befund.md`. Folgen fuer den Flipper: Vorlauf
  mindestens 160 ms, den Fuellstand an der Backpressure eichen, waehrend des
  Spiels nicht per `APP_LOG` protokollieren, und Vibration und Ton nicht
  gleichzeitig ueberreizen.
- **Kein Zeilenvorteil beim Zeichnen.** `graphics_release_frame_buffer` meldet
  immer den ganzen Puffer als schmutzig, und der Compositor ruft ohnehin
  `framebuffer_dirty_all`. Der geplante Dirty-Rect-Renderer spart nichts; was
  zaehlt, ist die Bildrate. Die Vollbildzeit misst der PANEL-Bildschirm des
  Messgeruests.
- **`vibes_cancel` blockiert den App-Task 10 bis 80 ms**, waehrend eines
  laufenden Musters nimmt das System kein neues an, und es gibt keinen
  Rueckruf am Ende. Das LRA-Vokabular muss aus Einzelimpulsen auf einem festen
  Zeitraster bestehen, siehe `pebble/schwebung/src/c/haptics.c`.
- **`time_ms()` springt gelegentlich um plus/minus 1000 ms**, und eine
  Aufrufpause ueber einer Sekunde verschluckt in der naiven Rechnung eine ganze
  Sekunde. Fertige Loesung: `pebble/schwebung/src/c/e1clock.c`.
- **Touch ist ein Finger, ohne Zeitstempel, und der Nutzer kann ihn abschalten.**
  Abtastrate und Ruhe-Jitter misst der STIMMEN-Bildschirm des Messgeruests.
- **Kein Gyroskop, kein Mikrofon, kein Umgebungslicht.** Beschleunigungssensor
  und Tap-Dienst gibt es.
