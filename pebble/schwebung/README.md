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
dass die App startet, rendert, alle Modi durchlaeuft und nicht abstuerzt.
Tasten lassen sich per `pebble emu-button --emulator emery click select`
(`-d 900` fuer langen Druck, `-n 2 -i 150` fuer Doppelklick) simulieren.

Eigenheit des QEMU-Emulators: sein Audio-Geraet hoert gelegentlich auf, den
Stream zu leeren (nach Moduswechseln, nach dem Panel-Test). Die App erkennt
das als "Stau" (300 ms lang nichts angenommen trotz leerem Vorlauf), stoppt
den Stream und oeffnet ihn nach kurzer Pause neu; `stau=` im Log zaehlt das.
Auf der Uhr schiebt die Firmware bei Unterlauf Stille nach und der Abfluss
laeuft weiter, dort sollte `stau` bei 0 bleiben. Steigt er trotzdem, ist das
ein echter Befund.

## Bedienung

Langer Druck auf Select wechselt den Bildschirm. Back beendet die App.

| Bildschirm | Select | Doppelklick Select | Up / Down |
|---|---|---|---|
| STIMMEN | neue Blume | | Feinstimmung plus/minus 0,25 Hz |
| LATENZ | Oktavsprung 500 ms mit LRA-Marker (Gabel singt hier ohne Finger) | Puffer-Probe | Feinstimmung |
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

In diesem Bildschirm singt die Gabel dauerhaft, auch ohne Finger, damit der
Sprung immer hoerbar ist. Doppelklick Select fuehrt die Puffer-Probe aus:
Stille wird geschrieben, bis `speaker_stream_write` weniger annimmt als
angeboten. Im Log steht

    [E1][PROBE] roh 8704 B, Abfluss 96 B, Puffer 8608 B = 269 ms, 17 Writes in 2 ms

`roh` ist alles, was angenommen wurde, `Abfluss` das, was waehrend der Probe
schon abgespielt wurde (32 Byte pro ms), `Puffer` die Differenz. Das ist
die Obergrenze der Latenz, wenn man den Puffer einfach vollschreibt. Der
Stream haelt danach mit eigener Buchhaltung nur rund 56 ms Vorlauf.

Select loest einen Oktavsprung mit gleichzeitigem LRA-Marker aus:

    [E1][LATENZ] Sprung 3 t=51234ms Queue 58ms +80ms Pipeline, Marker ja

Steht dort `Marker NEIN`, war der Motor gerade belegt und der Sprung ist
fuer die Messung ungueltig (einfach noch einmal druecken). Mit dem Handy
filmen oder aufnehmen: Abstand zwischen Vibration und hoerbarem Tonsprung ist
die reale Latenz Finger zu Ohr. Ziel unter 150 ms. Die HUD-Zeile zeigt `q`
(geschaetzter Vorlauf in ms), `cap` (gemessener Systempuffer), `ur`
(Unterlaeufe) und `sw` (Schreibvorgaenge mit Backpressure). Eine Minute
STIMMEN ohne Knacken und mit `ur=0` ist das Ziel.

### 2. Hoerbarkeit der Schwebung (STIMMEN)

Blume suchen, langsam annaehern. Zwischen 12 und 1 Hz Differenz muss das
Wabern klar zu hoeren sein, bei 1 Hz zaehlbar. Blumen liegen zwischen G4
(392 Hz) und G6 (1568 Hz), wo der Lautsprecher laut ist. Faellt der Test
durch, bleibt der Pendel-Modus aus dem Konzept als Hauptmodus.

### 3. LRA-Trennschaerfe (LRA)

Jede Rate laeuft, bis Select weiterschaltet. Jeder Impuls ist ein eigener
Aufruf mit 30 ms Muster, den ein Timer zur naechsten Periode nachlegt; es
gibt keinen `vibes_cancel`, weil der im PebbleOS den App-Task blockiert.
Jeder Impuls steht im Log mit Zeitstempel:

    [E1][LRA] t=12345 pulse mode=test period=250 ms len=30 ms

Der Auswerter rechnet daraus Abstand und Streuung der Impulse. Frage am
Handgelenk: Bis zu welcher Rate sind Einzelimpulse zaehlbar? Das Konzept
nimmt 5 Hz an. Der Wert wird zur Konstante `LRA_COUNTABLE_MAX_CHZ`. Im
Spiel liegt darueber bis 6 Hz ein Rauheits-Muster (230 ms an je 250 ms),
darueber schweigt der Motor.

### 4. Panel-Vollbildzeit (PANEL)

Select startet 300 Frames so schnell wie moeglich, erst Vollbild (alle
228 Zeilen geaendert), nach erneutem Select nur 10 Zeilen. Ergebnis im Log:

    [E1][PANEL] Vollbild: 300 Frames, 9871 ms, 32.9 ms/Frame, 30.3 fps, rend 1.2 ms

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

Der Auswerter fasst je Modus fps, Renderzeit, Vorlauf, Unterlaeufe,
Touch-Ereignisrate, Jitter, Ruheabweichung, Dead-Zone-Ausreisser, kleinsten
Ereignisabstand, freien Heap und Uhr-Glitches zusammen und listet Probe,
Spruenge, LRA-Impulsabstaende, Panel-Ergebnisse und Lichtschritte.

Das Sekundenlog besteht aus drei Zeilen, weil `APP_LOG` Nachrichten bei
rund 87 Zeichen abschneidet:

    [E1] STIMMEN fps=29.8 rend=1.2ms q=58ms ur=0 sw=0 heap=105856 glitch=0
    [E1t] touch=61/s jit=0.4px still=1px dz=0 ivl=15ms lra=0 bl=12 stau=0
    [E1+] fork=440.00 fl=523.25 beat=83.25 st=SUCHEN rs=0

`touch` ist die Ereignisrate, `jit` der mittlere Versatz innerhalb der Dead
Zone, `still` die groesste Abweichung eines ruhenden Fingers vom Ankerpunkt,
`dz` die Zahl der Ausreisser ueber die Dead Zone bei ruhendem Finger und
`ivl` der kleinste Abstand zweier Touch-Ereignisse (naeherungsweise die
Abtastrate des Controllers). `lra` und `bl` zaehlen die Aufrufe von Motor und
Backlight, `stau` die Neustarts des Streams (siehe unten), `glitch` die
gefilterten Zeitspruenge, `rs` die Neusynchronisationen der Uhr. Die HUD-Zeile unten in STIMMEN zeigt die vier Touch-Werte live.

## Aufbau

| Datei | Inhalt |
|---|---|
| `src/c/config.h` | alle Konstanten (Centi-Hertz, Millisekunden) |
| `src/c/e1clock.c` | monotone ms-Uhr, filtert die +-1000-ms-Spruenge von `time_ms()` |
| `src/c/synth.c` | Gabel, Blume, Glasklang, Splitterrauschen; Phasenakkumulator plus Sinus-LUT |
| `src/c/audio.c` | PCM-Stream 16 kHz/16 Bit, Fuellstandsbuchhaltung, Puffer-Probe, Preemption |
| `src/c/haptics.c` | LRA-Einzelpulse per Timer, ohne `vibes_cancel`; Warnung, Doppelpuls, Bruch warten, bis der Motor frei ist |
| `src/c/backlight.c` | Farborgel nach Tonklasse, Atmen bis 3 Hz, Keepalive, Testschritte |
| `src/c/input.c` | Touch als Trackpad: Dead Zone, Kupplung, Tastenmaske, Ereignisrate, Jitter |
| `src/c/render.c` | Framebuffer: Himmel, Kaustik (100 Spalten, pixelverdoppelt), Glasblume, Bloom, Panel-Test |
| `src/c/game.c` | Suchen, Einrasten, Karenz, Halten, Bruch, neue Blume |
| `src/c/main.c` | Fenster, Modi, HUD, Tasten, Timer, Sekundenlog |

Renderreihenfolge ist fest: Framebuffer einfangen, eigener Rasterizer,
freigeben, danach HUD-Text per GContext. Solange der Framebuffer eingefangen
ist, zeichnen GContext-Funktionen nichts.

Ein paar Regeln, die aus den PebbleOS-Quellen folgen und im Code stecken:

- Kein `vibes_cancel` im Normalbetrieb; er blockiert den App-Task 10 bis
  80 ms, laenger als der Vorlauf des Streams. Einzig `haptics_stop` beim
  Moduswechsel ruft ihn, und fuellt vorher den Stream auf 130 ms auf.
- Waehrend eines laufenden Vibrationsmusters nimmt das System kein neues an.
  Deshalb ist jeder Impuls ein eigener kurzer Aufruf, und Ereignisse wie der
  Einrast-Doppelpuls werden nachgeholt, sobald der Motor frei ist.
- Verliert die App den Fokus (Benachrichtigung, Timeline), pausiert sie
  Stream, Motor und Backlight und nimmt beides beim Zurueckkommen wieder auf.
- Die Lautstaerke steht auf 85 Prozent, die Summen der Oszillatoren sind so
  skaliert, dass Gabel plus Blume plus Glasklang nicht clippen.

## Bekannte Annahmen, die genau dieses Geruest prueft

- Der Systempuffer des Streams und seine Latenz sind nicht dokumentiert. Aus den
  PebbleOS-Quellen: Ring 8 KB (256 ms bei 16 kHz/16 Bit), Nachfuellen in Bloecken
  von 512 Samples (32 ms) aus dem Systemtask, und die Firmware rechnet beim
  Ausklingen mit 80 ms Treiberpuffer. Bei Unterlauf schiebt sie Stille nach.
- Ob `graphics_release_frame_buffer` nur geaenderte Zeilen ueberträgt, ist offen.
- Wie fein der Backlight-Treiber dimmt und wie schnell er folgt, ist offen.
- Touch-Abtastrate und Ruhe-Jitter sind nicht dokumentiert (HUD-Zeile unten).
- `time_ms()` springt gelegentlich um +-1000 ms; die Uhr filtert das (`glitch=` im Log)
  und synchronisiert sich nur dann neu, wenn der Versatz ueber 1,5 s stabil bleibt.
- Ob der Motor innerhalb von 250 ms sauber zwei getrennte 30-ms-Pulse liefert
  (Timer-Jitter des App-Tasks plus Anlaufzeit des LRA), zeigt die Streuung im Log.
