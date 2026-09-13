# Pulsmonitor für Pebble 2

Eine kleine Watchapp, die den Herzschlag der Pebble 2 (und der neuen Core-Devices-Uhren)
**grafisch** und **akustisch** darstellt.

| Pebble 2 (schwarz/weiß) | Suche nach Puls | Core Time 2 (Farbe, Lautsprecher) |
| --- | --- | --- |
| ![Pebble 2](screenshots/pebble2_diorite.png) | ![Suche](screenshots/pebble2_searching.png) | ![Core Time 2](screenshots/core_time2_emery.png) |

## Was die App macht

- Liest den optischen Pulssensor über die Health-API und fordert eine Abtastrate von 1 s an.
- Zeigt den Puls groß in BPM an, daneben ein Herz, das bei jedem Schlag pumpt.
- Zeichnet darunter eine laufende EKG-artige Kurve (P-QRS-T), die pro Schlag einen Ausschlag bekommt.
- Gibt jeden Schlag akustisch wieder:
  - **Pebble 2:** kurzer Vibrations-Klick (25 ms), die Uhr hat keinen Lautsprecher.
  - **Core Time 2 / Core 2 Duo:** zusätzlich ein Monitor-Piep über den Lautsprecher.
    Ist die Uhr stummgeschaltet (Quiet Time / Sounds), bleibt der Piep aus.
- Kommt 15 s lang kein gültiger Wert, zeigt die App wieder `--` und „Suche Puls...“.
- Beim Beenden werden die angeforderten Abtastraten zurückgesetzt, damit der Akku geschont wird.

### Woher das Timing kommt

Die App nimmt die beste Quelle, die die Uhr anbietet:

1. **Gemessene Schlagabstände** (HRV peak-to-peak, `health_service_peek_hrv_ppi_ms`). Jedes
   Ereignis ist ein real erkannter Herzschlag, Kurve und Ton folgen also dem echten Schlag statt
   einem Mittelwert. Die Statuszeile zeigt dann **Live**.
2. **Schläge pro Minute**, wenn die Uhr keine Einzelintervalle liefert (u. a. Pebble 2). Die
   Schläge werden im gemessenen Takt erzeugt.

Kurve und Schläge hängen beide an der Uhrzeit, nicht an Timer-Callbacks: Jeder Schlag klingt in
demselben Schritt, der seinen Ausschlag zeichnet. Ein verzögertes Bild kann den Puls dadurch weder
dehnen noch Schläge verschlucken, und der Abstand auf dem Display entspricht exakt dem gemessenen
Intervall. Die Kurve läuft mit 50 px/s, das Bild wird 30-mal pro Sekunde aktualisiert, und der
Messwert wird 5-mal pro Sekunde nachgeführt.

Ein Hinweis zur Ehrlichkeit: Die BPM-Anzeige des Sensors ist ein geglätteter Mittelwert und hinkt
der Realität um einige Sekunden hinterher. Das ist eine Eigenschaft des optischen Sensors, keine
der App. Was die App beschleunigt, ist alles danach.

### Der Ton

Der Piep ist ein vorgerechnetes PCM-Sample statt eines roh erzeugten Tons: 880 Hz Grundton mit
zweiter und dritter Oberwelle im Verhältnis 1 : 0,55 : 0,22, 50 ms lang, mit weicher An- und
Abstiegsflanke. Die Mischung stammt aus der Frequenzanalyse einer echten Monitor-Aufnahme, und weil
das Sample bei null anfängt und aufhört, knackt es an den Rändern nicht mehr.

Neu erzeugen (etwa mit anderer Tonhöhe) lässt es sich so:

```sh
python3 tools/make_beep_sample.py --freq 880 --ms 50 > src/c/beep_sample.h
```

Der Sensor liefert je nach Uhr nur Schläge pro Minute, keine Einzelschläge. Die Kurve ist eine
Visualisierung, kein medizinisches EKG.

## Bedienung

| Taste | Funktion |
| --- | --- |
| SELECT | Vibrations-Klick an/aus |
| UP | Piep an/aus (nur Uhren mit Lautsprecher) |
| DOWN lang | Demo-Modus an/aus (simulierter Puls 58–112 BPM, z. B. für den Emulator) |
| BACK | Beenden |

Die Einstellungen für Vibration und Ton werden gespeichert.

Hinweis: Der Vibrationsmotor kann den optischen Sensor kurz stören. Wenn die Messung unruhig wird,
die Vibration mit SELECT ausschalten.

## Bauen und installieren

Voraussetzung ist das aktuelle Pebble-SDK von Core Devices:

```sh
uv tool install pebble-tool      # oder: pip install pebble-tool
pebble sdk install latest
```

Dann im Projektordner:

```sh
cd pebble/heart-rate
pebble build
pebble install --phone <IP der Pebble-App>    # auf die Uhr
pebble install --emulator diorite             # oder im Emulator (Pebble 2)
```

Die fertige Datei liegt danach unter `build/heart-rate.pbw` und kann auch direkt über die
Pebble-App aufs Handy geschickt und installiert werden.

Im Emulator lässt sich ein Puls simulieren (funktioniert mit der neueren Firmware, z. B. `emery`):

```sh
pebble emu-heart-rate --emulator emery 72
```

Auf der alten Pebble-2-Emulator-Firmware hilft stattdessen der Demo-Modus (DOWN lang drücken,
im Emulator: `pebble emu-button --emulator diorite push down`, kurz warten, `... release down`).

## Aufbau

| Datei | Inhalt |
| --- | --- |
| `src/c/main.c` | die ganze App |
| `src/c/beep_sample.h` | erzeugtes PCM-Sample des Pieps, nicht von Hand bearbeiten |
| `tools/make_beep_sample.py` | erzeugt dieses Sample (braucht nur numpy) |

## Zielplattformen

`diorite` (Pebble 2), `emery` (Core Time 2), `flint` (Core 2 Duo), `gabbro` (rund, Core Devices).
Alle vier werden mit `pebble build` in einem Rutsch gebaut.
