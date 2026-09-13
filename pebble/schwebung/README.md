# Glasgarten, Etappe 1: Messgeruest fuer die Pebble Time 2

Baubares Pebble-Projekt (C, SDK 4.9+, Plattform `emery`), das die fuenf
Messungen des ersten Wochenendes aus dem Konzeptpapier SCHWEBUNG liefert.
Es ist kein Spiel, sondern der Beweis, ob das Spiel richtig ist:

1. Stream-Latenz und Systempuffer des PCM-Streams
2. Hoerbarkeit einer 1-Hz-Schwebung aus dem kleinen Lautsprecher
3. Trennschaerfe des LRA bei 2, 4, 6 und 8 Hz
4. Zeit pro Vollbild bei Framebuffer-Direktzugriff (Vollbild gegen 10 Zeilen)
5. Backlight: Aufrufrate, Dimmung ueber RGB-Werte, Atmen bis 3 Hz

Dazu laeuft die eigentliche Kernmechanik schon: Finger aufs Glas, Stimmgabel
singt, eine Glasblume mit verborgener Frequenz, Schwebung aus der Summe zweier
Sinus, Einrasten unter 0,5 Hz, Karenz, Halten, Bruch, neue Blume.

## Bauen und installieren

```bash
uv tool install pebble-tool        # einmalig
pebble sdk install latest          # einmalig (getestet mit 4.33.1)
pebble build
pebble install --emulator emery    # Emulator: ohne Ton, Touch, Backlight-Farbe
pebble install --cloudpebble       # echte Uhr ueber die Pebble-App (Dev Connect)
pebble logs --cloudpebble          # Log der Uhr, alle Messwerte stehen darin
```

Die Messungen brauchen die echte Uhr. Im Emulator laesst sich nur pruefen,
dass die App startet, rendert und nicht abstuerzt.

## Bedienung

Langer Druck auf Select wechselt den Bildschirm. Back beendet die App.

| Bildschirm | Select | Doppelklick Select | Up / Down |
|---|---|---|---|
| STIMMEN | neue Blume | | Feinstimmung plus/minus 0,25 Hz |
| LATENZ | Oktavsprung 500 ms mit LRA-Marker | Puffer-Probe | Feinstimmung |
| LRA | naechste Rate (2, 4, 6, 8 Hz) | | |
| LICHT | naechster Schritt (Atmen 1, 2, 3, 4 Hz, Dimmrampe, aus) | | |
| PANEL | Test starten, danach Variante wechseln | | |

STIMMEN: Finger aufs Glas legen, die Gabel singt, solange er liegt. Senkrecht
ziehen aendert die Tonhoehe relativ (langsam 0,25 Hz pro Pixel, schnell bis
2 Hz pro Pixel). Liegt der Finger 300 ms still, rueckt die Kupplung ein und
die Tonhoehe friert, bis der Finger deutlich bewegt wird. Up/Down stimmen fein,
jeder Tastendruck friert den Pan fuer 200 ms ein. Innerhalb von 40 Hz um die
Blume antwortet sie, innerhalb von 12 Hz deutlich. Liegt die mittlere
Differenz eine Sekunde lang unter 0,5 Hz, rastet sie ein (Doppelpuls,
Glasklang, Bloom). Nach einer Sekunde Karenz laeuft die Haltezeit: nach
1,8 s beginnt die LRA-Warnung, nach 3 s zerspringt das Glas.

## Die fuenf Messungen und was sie zeigen muessen

### 1. Stream-Latenz (LATENZ)

Doppelklick Select fuehrt die Puffer-Probe aus: Stille wird geschrieben, bis
`speaker_stream_write` weniger annimmt als angeboten. Im Log steht

    [E1][PROBE] Systempuffer nimmt N Byte = M ms an

Das ist die Obergrenze der Latenz, wenn man den Puffer einfach vollschreibt.
Der Stream haelt danach mit eigener Buchhaltung nur rund 56 ms Vorlauf.
Select loest einen Oktavsprung mit gleichzeitigem LRA-Marker aus. Mit dem
Handy filmen oder aufnehmen: Abstand zwischen Vibration und hoerbarem
Tonsprung ist die reale Latenz Finger zu Ohr. Ziel unter 150 ms. Die HUD-Zeile
zeigt `q` (geschaetzter Vorlauf in ms), `ur` (Unterlaeufe) und `sw` (Schreibvorgaenge
mit Backpressure). Eine Minute STIMMEN ohne Knacken und mit `ur=0` ist das Ziel.

### 2. Hoerbarkeit der Schwebung (STIMMEN)

Blume suchen, langsam annaehern. Zwischen 12 und 1 Hz Differenz muss das
Wabern klar zu hoeren sein, bei 1 Hz zaehlbar. Blumen liegen zwischen G4
(392 Hz) und G6 (1568 Hz), wo der Lautsprecher laut ist. Faellt der Test
durch, bleibt der Pendel-Modus aus dem Konzept als Hauptmodus.

### 3. LRA-Trennschaerfe (LRA)

Jede Rate laeuft, bis Select weiterschaltet. Jeder Vibrationsaufruf steht im
Log mit Zeitstempel:

    [E1][LRA] t=12345 enqueue mode=test period=250 ms segs=7

Frage am Handgelenk: Bis zu welcher Rate sind Einzelimpulse zaehlbar? Das
Konzept nimmt 5 Hz an. Der Wert wird zur Konstante `LRA_COUNTABLE_MAX_CHZ`.

### 4. Panel-Vollbildzeit (PANEL)

Select startet 300 Frames so schnell wie moeglich, erst Vollbild (alle
228 Zeilen geaendert), nach erneutem Select nur 10 Zeilen. Ergebnis im Log:

    [E1][PANEL] Vollbild: 300 Frames in 9871 ms = 32.9 ms/Frame (30.3 fps), Render 1.2 ms

Sind beide Werte gleich, gibt es keinen Zeilen-Vorteil bei Framebuffer-
Zugriff (Erwartung laut PebbleOS-Quellen). Liegt Vollbild ueber 33 ms, wird
20 fps das Ziel; Ton, LRA und Backlight haengen nicht am Frame.

### 5. Backlight (LICHT)

Schritte 0 bis 3 atmen mit 1, 2, 3 und 4 Hz zwischen 30 und 100 Prozent
Helligkeit, Schritt 4 faehrt eine Dimmrampe in acht Stufen zu je zwei
Sekunden (nur ueber den RGB-Wert, es gibt keine Helligkeits-API). Mit dem
Handy filmen: Folgt die LED sauber? Dimmen dunklere Werte sichtbar? Ab
welcher Rate wirkt das Atmen als Flackern? Im Spiel atmet das Backlight nur
bis 3 Hz.

## Log-Auswertung

`pebble logs` in eine Datei umleiten und auswerten:

```bash
pebble logs --cloudpebble | tee e1.log
python3 tools/analyze_logs.py e1.log
```

Der Auswerter fasst fps, Renderzeit, Vorlauf, Unterlaeufe, Touch-Ereignisrate
und Jitter je Modus zusammen und listet Probe, Spruenge, LRA-Intervalle,
Panel-Ergebnisse und Lichtschritte.

## Aufbau

| Datei | Inhalt |
|---|---|
| `src/c/config.h` | alle Konstanten (Centi-Hertz, Millisekunden) |
| `src/c/e1clock.c` | monotone ms-Uhr, filtert die +-1000-ms-Spruenge von `time_ms()` |
| `src/c/synth.c` | Gabel, Blume, Glasklang, Splitterrauschen; Phasenakkumulator plus Sinus-LUT |
| `src/c/audio.c` | PCM-Stream 16 kHz/16 Bit, Fuellstandsbuchhaltung, Puffer-Probe, Preemption |
| `src/c/haptics.c` | LRA-Muster pro Rate, Wechsel an Impulsgrenzen, Warnung, Doppelpuls, Bruch |
| `src/c/backlight.c` | Farborgel nach Tonklasse, Atmen bis 3 Hz, Keepalive, Testschritte |
| `src/c/input.c` | Touch als Trackpad: Dead Zone, Kupplung, Tastenmaske, Ereignisrate, Jitter |
| `src/c/render.c` | Framebuffer: Himmel, Kaustik (100 Spalten, pixelverdoppelt), Glasblume, Bloom, Panel-Test |
| `src/c/game.c` | Suchen, Einrasten, Karenz, Halten, Bruch, neue Blume |
| `src/c/main.c` | Fenster, Modi, HUD, Tasten, Timer, Sekundenlog |

Renderreihenfolge ist fest: Framebuffer einfangen, eigener Rasterizer,
freigeben, danach HUD-Text per GContext. Solange der Framebuffer eingefangen
ist, zeichnen GContext-Funktionen nichts.

## Bekannte Annahmen, die genau dieses Geruest prueft

- Der Systempuffer des Streams und seine Latenz sind nicht dokumentiert.
- Ob `graphics_release_frame_buffer` nur geaenderte Zeilen ueberträgt, ist offen.
- Wie fein der Backlight-Treiber dimmt und wie schnell er folgt, ist offen.
- Touch-Abtastrate und Ruhe-Jitter sind nicht dokumentiert (HUD-Zeile unten).
- `time_ms()` springt gelegentlich um +-1000 ms; die Uhr filtert das (`glitch=` im Log).
