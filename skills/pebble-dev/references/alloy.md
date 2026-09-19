# Alloy: Pebble-Apps in JavaScript

Alloy ist der neue JavaScript-Weg von Core Devices. Der Code läuft **auf der
Uhr** in der XS-Engine von Moddable — nicht zu verwechseln mit PebbleKit JS,
das auf dem Handy läuft. Beides kann in einem Projekt vorkommen.

**Einschränkung, die alles andere bestimmt: Alloy läuft nur auf `emery`
(Pebble Time 2) und `gabbro` (Pebble Round 2).** Eine Alloy-App lässt sich
nicht nachträglich auf ältere Uhren bringen. Wer `basalt`, `chalk` oder
`diorite` bedienen will, nimmt C.

## Projekt anlegen

```bash
pebble new-project --alloy meine-app < /dev/null
```

Ergebnis (geprüft mit SDK 4.33.1):

```
meine-app/
  package.json              # "projectType": "moddable", targetPlatforms: emery, gabbro
  src/
    embeddedjs/
      main.js               # laeuft auf der UHR
      manifest.json         # Moddable-Manifest (Module, Includes)
    pkjs/
      index.js              # laeuft auf dem HANDY (PebbleKit JS)
    c/
      mdbl.c                # Klammer zur Firmware, nicht anfassen
  resources/
  wscript
```

Die Vorlage ist ein Watchface (`"watchapp": { "watchface": true }`).

Braucht `pebble new-project --alloy` die Moddable-Werkzeuge und meldet
*"The currently active SDK does not have Moddable tools"*, ist die
SDK-Installation unvollständig — siehe `toolchain.md`, Abschnitt 3.

## Die zwei Oberflächen

- **Poco** — schlanke Zeichen-API, direkt auf den Bildschirm. Für Watchfaces
  und alles Gezeichnete.
- **Piu** — deklaratives UI-Framework mit Containern, Bindings und Übergängen.
  Für App-Oberflächen mit mehreren Ansichten.

Die Vorlage nutzt Poco:

```javascript
import Poco from "commodetto/Poco";

let render = new Poco(screen);
const font  = new render.Font("Bitham-Black", 30);
const black = render.makeColor(0, 0, 0);
const white = render.makeColor(255, 255, 255);

function draw() {
  render.begin();
  render.fillRectangle(white, 0, 0, render.width, render.height);

  const msg   = (new Date).toTimeString().slice(0, 8);
  const width = render.getTextWidth(msg, font);
  render.drawText(msg, font, black,
                  (render.width - width) / 2,
                  (render.height - font.height) / 2);

  render.end();
}

watch.addEventListener('secondchange', draw);
```

`render.width`/`render.height` statt fester Werte — `emery` ist 200 × 228,
`gabbro` 260 × 260 und rund.

`secondchange` ist bequem, aber teuer: für ein Watchface, das nur Minuten
zeigt, das Minutenereignis nehmen. Der Akkuhinweis aus dem Hauptdokument gilt
hier genauso.

## Wann Alloy, wann C

| | Alloy | C |
|---|---|---|
| Plattformen | nur `emery`, `gabbro` | alle sieben |
| Einstieg | schnell, vertraute Sprache | steiler |
| UI-Aufbau | Piu nimmt viel ab | alles selbst |
| Speicherkontrolle | begrenzt | vollständig |
| Rechenintensives, Timing, Audio | ungeeignet | dafür gemacht |
| Bibliotheken | Moddable-Ökosystem | Pebble Packages |

**Faustregel:** UI-lastige App nur für die neuen Uhren → Alloy. Alles andere,
und alles mit harten Speicher- oder Zeitanforderungen → C.

## Debuggen

Zum Debuggen von Alloy-Apps gibt es **xsbug**, den Debugger von Moddable
(eigene Anleitung unter `/guides/debugging/debugging-alloy-with-xsbug/`).
`console.log` erscheint wie gewohnt in `pebble logs`.

## Weiterführend

Die Alloy-Anleitungen auf `developer.repebble.com/guides/alloy/` decken
Sensoren, Speicher, Netzwerk, AppMessage, Animationen, eigenes Zeichnen (Port),
Diktat, Wakeups, Vibration, Health und native Funktionen (FFI) ab.
