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

1. **Gemessene Schlagabstände** (HRV peak-to-peak, `health_service_peek_hrv_ppi_ms`). Jeder Wert
   ist der Abstand zweier real erkannter Herzschläge, der Takt folgt also der echten Messung statt
   einem Mittelwert. Die Statuszeile zeigt dann **Live**.
2. **Schläge pro Minute**, wenn die Uhr keine Einzelintervalle liefert (u. a. Pebble 2).

Genutzt wird dabei nur der Intervallwert, nie der Zeitpunkt, zu dem die Meldung eintrifft: Die Uhr
meldet die Länge eines bereits vergangenen Schlags, ihr Eintreffen sagt also nichts darüber aus,
wann der nächste Schlag fällt. Die Phase gehört der Uhrzeit, die Meldung liefert nur das Tempo.

Gemessene Intervalle sind nicht blind zu glauben. Der optische Sensor rastet zeitweise auf den
zweiten Gipfel der Pulswelle ein und meldet dann die halbe Schlagdauer, was den Puls verdoppeln
würde, während die angezeigte Zahl unverändert stehen bleibt. Zwei Stufen fangen das ab:

1. Ein Intervall wird nur übernommen, wenn es höchstens 25 % vom Takt der gemittelten BPM-Anzeige
   abweicht. Natürliche Schwankung von Schlag zu Schlag liegt deutlich darunter, ein halbiertes
   oder verdoppeltes Intervall deutlich darüber.
2. Von den letzten drei übernommenen Werten gilt der mittlere. Ein einzelner Ausreißer, der die
   erste Stufe knapp passiert, wird so überstimmt statt das Tempo zu setzen.

Kommen 5 Sekunden lang keine brauchbaren Intervalle mehr, übernimmt die BPM-Anzeige den Takt und
die Statuszeile verliert das **Live**.

Zwei Schläge kommen sich nie näher als 260 ms, also 230 pro Minute. Ein Herz schlägt nicht
schneller, und ein Piep wäre sonst noch nicht zu Ende, wenn der nächste beginnt. Ohne diese Sperre
setzte ein nach einer Lücke zurückkehrender Takt sofort einen Schlag, egal wie kurz der letzte her
war, was den vorherigen Piep abschnitt und als Klicken zu hören war.

Kurve und Schläge hängen beide an der Uhrzeit, nicht an Timer-Callbacks: Jeder Schlag klingt in
demselben Schritt, der seinen Ausschlag zeichnet. Ein verzögertes Bild kann den Puls dadurch weder
dehnen noch Schläge verschlucken, und der Abstand auf dem Display entspricht exakt dem gemessenen
Intervall. Die Kurve läuft mit 50 px/s, das Bild wird 30-mal pro Sekunde aktualisiert, und der
Messwert wird 5-mal pro Sekunde nachgeführt.

Diese Taktlogik steckt in `src/c/beat_clock.c` und hängt an keiner Pebble-Funktion, damit sie sich
direkt auf dem Rechner prüfen lässt:

```sh
cc -Wall -Wextra -o /tmp/test_beat_clock tools/test_beat_clock.c src/c/beat_clock.c && /tmp/test_beat_clock
```

Ein Hinweis zur Ehrlichkeit: Die BPM-Anzeige des Sensors ist ein geglätteter Mittelwert und hinkt
der Realität um einige Sekunden hinterher. Das ist eine Eigenschaft des optischen Sensors, keine
der App. Was die App beschleunigt, ist alles danach.

### Der Ton

Der Piep ist ein vorgerechnetes PCM-Sample statt eines roh erzeugten Tons: 880 Hz Grundton mit
zweiter und dritter Oberwelle im Verhältnis 1 : 0,55 : 0,22, 50 ms lang, mit weicher An- und
Abstiegsflanke. Die Mischung stammt aus der Frequenzanalyse einer echten Monitor-Aufnahme, und weil
das Sample bei null anfängt und aufhört, knackt es an den Rändern nicht mehr.

Die Lautstärke steht auf 65 von 100. Darüber verzerrt der kleine Lautsprecher der Core Time 2
hörbar, messbar an zusätzlichen Obertönen um 5 kHz. `BEEP_VOLUME` in `src/c/main.c` ändert das.

Neu erzeugen (etwa mit anderer Tonhöhe) lässt es sich so:

```sh
python3 tools/make_beep_sample.py --freq 880 --ms 50 > src/c/beep_sample.h
```

Der Sensor liefert je nach Uhr nur Schläge pro Minute, keine Einzelschläge. Die Kurve ist eine
Visualisierung, kein medizinisches EKG.

## Einstellungen

In der Pebble-App auf dem Handy über das Zahnrad neben dem Pulsmonitor:

| Einstellung | Auswahl |
| --- | --- |
| Piep bei jedem Schlag | an/aus |
| Lautstärke | 0 bis 100, Schritte von 5 |
| Wiedergabe | einzelne Töne oder durchgehend |
| Tonhöhe | tief 660 Hz, Monitor 880 Hz, hoch 1046 Hz |
| Vibration bei jedem Schlag | an/aus |
| Länge der Vibration | kurz 15 ms, normal 25 ms, kräftig 40 ms |
| Beleuchtung | wie sonst auch, bei jedem Schlag kurz, dauerhaft an |
| Kurvengeschwindigkeit | langsam 25 px/s, normal 50 px/s, schnell 75 px/s |
| Kurvenfarbe | grün, rot, weiß, gelb, türkis (nur Farbdisplays) |
| Puls simulieren | an/aus, derselbe Demo-Modus wie der lange Druck auf DOWN |

Alles wird auf der Uhr gespeichert und gilt sofort, ohne die App neu zu starten.

### Warum es zwei Wiedergabearten gibt

Bei **einzelnen Tönen**, der Voreinstellung, bekommt der Lautsprecher pro Schlag ein Sample und
wird danach wieder freigegeben. Auf der Core Time 2 knackt er dabei gelegentlich, hörbar etwa 100
bis 200 ms nach dem Piep, also genau dann, wenn der Verstärker abschaltet.

**Durchgehend** hält stattdessen einen PCM-Strom über die ganze Sitzung offen und schreibt zwischen
den Schlägen Stille hinein, sodass der Verstärker gar nicht erst abschaltet. Der Strom wird 70 ms
im Voraus gefüllt und alle 10 ms nachgefüllt: weit genug, dass eine verspätete Runde ihn nicht
leerlaufen lässt, kurz genug, dass der Piep bei seiner Zacke bleibt.

Diese Variante ist als Versuch gekennzeichnet und nicht voreingestellt. Auf einem Rechner lässt
sich kein Ton prüfen, deshalb bleibt die bewährte Art die Vorgabe, bis jemand die andere auf einer
echten Uhr gehört hat.

Nebenbei aus dem Emulator gelernt: Solange der Lautsprecher noch spielt, lehnt er einen zweiten
Abspielbefehl ab, statt ihn zu übernehmen. Ein Schlag, der zu kurz nach dem vorigen kommt, bleibt
also stumm. Deshalb die Sperre von 260 ms zwischen zwei Schlägen weiter oben.

Der Vibrationsmotor klickt bei jedem Schlag hörbar mit. Wer einen reinen Monitor-Ton will, schaltet
die Vibration aus, entweder in den Einstellungen oder mit der Auswahltaste.

Unten auf der Seite sitzt ein Knopf „Buy me a coffee“. Die Adresse steht als `DONATION_URL` oben in
`src/pkjs/config.js`; ist sie leer, fällt der ganze Abschnitt weg.

Damit die Pebble-App das Zahnrad überhaupt anzeigt, führt `package.json` die Fähigkeit
`configurable`. Ohne diesen Eintrag bleibt die Einstellungsseite unsichtbar, auch wenn sie fertig
im Paket liegt.

Die Seite selbst baut [Clay](https://github.com/pebble/clay). Clay liegt als reines JavaScript in
`src/pkjs/vendor/clay.js` statt als Pebble-Paket, weil das veröffentlichte Paket die neuen
Plattformen flint und gabbro nicht kennt und den Build dort abbrechen lässt.

Clay verwandelt die eigene Funktion aus `src/pkjs/custom-clay.js` in Text und legt sie in die
Seite. Alles, was dort einen Modullader bräuchte, lässt die Seite kommentarlos leer bleiben. Dieser
Test baut die Seite so auf, wie das Handy es täte, und prüft genau das:

```sh
pebble build && node tools/check_config_page.js
```

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
| `src/c/main.c` | Anzeige, Sensor, Bedienung |
| `src/c/beat_clock.c` | Takt: legt die Schläge auf die Uhrzeit und prüft die Messwerte, ohne Pebble-Abhängigkeiten |
| `src/c/settings.c` | Einstellungen: Vorgaben, Speichern, Auswerten der Handy-Nachricht |
| `src/c/beep.c` | Ton: PCM-Strom, Tonhöhe, Rückfall auf einzelne Töne |
| `src/c/beep_sample.h` | erzeugtes PCM-Sample des Pieps, nicht von Hand bearbeiten |
| `src/pkjs/config.js` | Aufbau der Einstellungsseite |
| `tools/make_beep_sample.py` | erzeugt dieses Sample (braucht nur numpy) |
| `tools/test_beat_clock.c` | Test der Taktlogik, läuft auf dem Rechner |
| `tools/check_config_page.js` | prüft, ob die Einstellungsseite aufgebaut werden kann |

## Zielplattformen

`diorite` (Pebble 2), `emery` (Core Time 2), `flint` (Core 2 Duo), `gabbro` (rund, Core Devices).
Alle vier werden mit `pebble build` in einem Rutsch gebaut.
