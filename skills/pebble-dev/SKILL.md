---
name: pebble-dev
description: Entwicklung von Pebble-Watchapps und Watchfaces in C (und Alloy/JavaScript) mit dem Core-Devices-SDK - Toolchain, Projektaufbau, Speicherbudget, Plattformunterschiede, AppMessage, kopfloser Emulatortest. Immer verwenden, wenn eine Pebble-App oder ein Watchface gebaut, erweitert, portiert oder repariert werden soll, wenn "pebble build", "pebble install", "pebble new-project" oder die package.json einer Pebble-App vorkommen, wenn Window, Layer, TextLayer, MenuLayer, GBitmap, AppMessage, persist_write, tick_timer_service, AppTimer, Wakeup, Dictation oder Background Worker auftauchen, bei Abstürzen ("App fault!", PC/LR-Adressen), Speicherproblemen ("Out of memory", zu großer Heap), Emulator-Problemen (hängender qemu-pebble, "Connection refused", "No SDK installed") und bei der Frage, auf welchen Plattformen (aplite, basalt, chalk, diorite, flint, emery, gabbro) etwas läuft. Für Tonausgabe stattdessen pebble-audio, für die Store-Einreichung pebble-publish verwenden.
---

# Pebble-Apps professionell entwickeln

Dieser Skill deckt den Weg von der leeren Konsole bis zu einer App ab, die auf
echter Hardware sauber läuft. Zwei Nachbarthemen haben eigene Skills und
gehören nicht hierher: **Tonausgabe → `pebble-audio`**, **Store-Einreichung,
Icons, Screenshots, Banner → `pebble-publish`**.

**Die eine Regel, die den Unterschied macht:** Eine Pebble ist ein
Mikrocontroller mit 24 bis 128 KB für Code *und* Heap zusammen. Jede
Entscheidung — Bildformat, Puffergröße, Zielplattform, Bibliothek — ist zuerst
eine Speicherentscheidung. Wer wie am Desktop programmiert, merkt es nicht beim
Kompilieren, sondern wenn die App auf der Uhr wortlos zum Launcher
zurückspringt.

## 1. Zuerst die Zielplattform klären

Frage den Nutzer am Anfang in **einer** Nachricht, falls es nicht aus dem
Projekt hervorgeht:

1. **Welche Uhr?** Meist Pebble Time 2 (`emery`). Davon hängen Auflösung,
   Farbe, Speicher und verfügbare Sensoren ab.
2. **Watchface oder Watchapp?** Ein Watchface hat keine Tasten zur freien
   Verfügung (Select/Up/Down gehören dem System) und muss extrem sparsam sein.
3. **Braucht es das Handy?** Sobald Wetter, Web-API oder eine
   Konfigurationsseite im Spiel ist, kommt PebbleKit JS dazu.

Die aktuelle Familie (offizielle Tabelle, Stand 2026; Details und alle Zeilen
in `references/platforms.md`):

| Plattform | Uhr | Display | Farben | App (Code+Heap) | Besonderes |
|---|---|---|---|---|---|
| `aplite` | Classic, Steel | 144 × 168 | 2 | **24 k** | Res. max 96 k |
| `basalt` | Time, Time Steel | 144 × 168 | 64 | 64 k | Mikro, Smartstrap |
| `chalk` | Time Round | 180 × 180 **rund** | 64 | 64 k | Mikro |
| `diorite` | Pebble 2 | 144 × 168 | 2 | 64 k | HRM, Barometer |
| `flint` | Pebble 2 Duo | 144 × 168 | 2 | 64 k | **Lautsprecher** |
| `emery` | Pebble Time 2 | 200 × 228 | 64 | **128 k** | Touch, HRM, Lautsprecher, RGB-Backlight |
| `gabbro` | Pebble Round 2 | 260 × 260 **rund** | 64 | **128 k** | Touch |

`targetPlatforms` in der `package.json` auf das beschränken, was wirklich
läuft. Eine Plattform, die nie getestet wurde, gehört nicht in die Liste — und
eine API, die es dort nicht gibt (Lautsprecher, HRM), lässt den Linker mit
`ld returned 1 exit status` stehen.

## 2. Toolchain

Einrichtung, Versionen und alle bekannten Fallstricke stehen in
`references/toolchain.md`. Kurzfassung:

```bash
uv tool install pebble-tool --python 3.13     # Python 3.14 wird noch nicht unterstützt
export PATH="$HOME/.local/bin:$PATH"
pebble sdk install latest
pebble sdk activate <version>                 # Installation aktiviert nicht immer selbst
```

Vier Dinge, die beim Arbeiten im Terminal (und besonders für einen Agenten)
zählen:

- **Jedes `pebble`-Kommando mit `< /dev/null` aufrufen.** Sonst wartet ein
  Erstlauf-Prompt auf stdin, und das Kommando hängt ohne Ausgabe bis zum
  Timeout — das sieht aus wie ein kaputtes SDK, ist aber nur eine Rückfrage.
- **`pebble sdk install` endet mit Exit-Code 1**, wenn die Version schon da
  ist (`SDK X is already installed.`). In Skripten als Erfolg behandeln.
- **Emulator aufräumen:** `qemu-pebble` dreht nach dem Start dauerhaft nahe
  100 % CPU. Nach jedem Test `pebble kill`.
- **Hängt der Emulator** (Timeouts bei `screenshot`/`ping`, obwohl `install`
  noch klappt), ist der simulierte Flash verwurstet:
  `pebble kill && pebble wipe`, dann neu installieren.

Auf einem Rechner ohne Bildschirm (Server, Container, Cloud-Sitzung) richtet
`scripts/setup_headless.sh` alles ein und prüft mit `--check` nur nach: SDL,
ein dauerhaftes Xvfb und der IPv4-Patch für pypkjs, ohne den der Emulator auf
Hosts ohne IPv6 mit `[Errno 111] Connection refused` stehen bleibt.

## 3. C oder Alloy

- **C-SDK**: alles, was Rechenzeit, Speicherkontrolle oder eine API ohne
  JavaScript-Pendant braucht. Läuft auf allen sieben Plattformen. Standard.
- **Alloy** (JavaScript auf der XS-Engine von Moddable, UI mit *Piu*, Grafik
  mit *Poco*): `pebble new-project --alloy <name>`. Läuft **nur auf `emery`
  und `gabbro`** und braucht ein vollständiges SDK mit Moddable-Tools.
  Sinnvoll für UI-lastige Apps ohne harte Speicher- oder Timing-Anforderungen.
  Einstieg und Struktur in `references/alloy.md`.

Im Zweifel C. Eine Alloy-App lässt sich nicht nachträglich auf ältere
Plattformen bringen.

## 4. Projekt anlegen

```bash
pebble new-project --javascript meine-app < /dev/null
```

```
meine-app/
  package.json         # Metadaten, UUID, Zielplattformen, Ressourcen, messageKeys
  src/c/main.c         # Code auf der Uhr
  src/pkjs/index.js    # Code auf dem Handy (optional)
  resources/           # Bilder, Fonts
  wscript              # Build-Skript, selten anzufassen
```

Pflichtfelder der `package.json` und die Fallen darin (Versionsformat, UUID,
`menuIcon`, `capabilities`) stehen in `references/platforms.md`; die
Store-relevanten Felder prüft `pebble-publish`. Vor dem ersten Build einmal
`scripts/check_project.py` über das Projekt laufen lassen — es findet die
Fehler, die sonst erst beim Einreichen auffallen.

## 5. Das Grundgerüst

`assets/main.c` ist ein vollständiges, kompilierbares Gerüst mit
Fensterlebenszyklus, Tasten, Tick, Einstellungen im Flash und AppMessage.
Als Startpunkt kopieren und ausdünnen, statt von null zu schreiben. Es wurde
mit SDK 4.33.1 für alle sieben Plattformen gebaut und auf `emery` und `gabbro`
im Emulator ausgeführt; `assets/skeleton_emery.png` und
`assets/skeleton_gabbro.png` zeigen das Ergebnis. Am runden Display ist gut zu
sehen, wie der Akzentbalken am Rand beschnitten wird — genau der Fall, für den
Abschnitt 7 gilt.

Der Aufbau jeder C-App ist immer derselbe:

```c
int main(void) { init(); app_event_loop(); deinit(); return 0; }
```

`app_event_loop()` blockiert bis zum Beenden der App. **Alles passiert in
Callbacks** — es gibt keine eigene Hauptschleife, und eine Funktion, die
länger als ein paar Millisekunden rechnet, blockiert Anzeige, Tasten und
Bluetooth gleichzeitig.

**Die wichtigste Konvention, und die häufigste Fehlerquelle:**

> Was in `window_load` erzeugt wird, wird in `window_unload` zerstört —
> vollständig, in umgekehrter Reihenfolge und ohne Ausnahme.

Das System entlädt Fenster, die nicht sichtbar sind, und lädt sie später neu.
Wer im Load erzeugt und im `deinit` aufräumt, verliert bei jedem Durchgang
Speicher; die App läuft im Emulator tadellos und stirbt auf der Uhr nach dem
zwölften Öffnen. Besonders gern vergessen: `fonts_unload_custom_font`,
`gbitmap_destroy`, `gdraw_command_image_destroy`, `menu_layer_destroy`.

Welche Layer-Typen es gibt, wann man selbst zeichnet und wie Animationen
funktionieren, steht in `references/c-api.md`.

## 6. Speicher — das eigentliche Thema

Das Budget aus der Tabelle in Abschnitt 1 ist **Code plus Heap**. Größerer
Code heißt also weniger Platz für Daten.

Rechne Bildgrößen aus, bevor du sie lädst. Ein Farb-`GBitmap` belegt **ein Byte
pro Pixel**, ein Schwarz-Weiß-Bitmap ein Bit:

| Fläche | Farbe (8 bpp) | S/W (1 bpp) |
|---|---|---|
| Vollbild `emery` 200 × 228 | 45.600 B (≈ 45 k) | — |
| Vollbild `gabbro` 260 × 260 | 67.600 B (≈ 66 k) | — |
| Vollbild `basalt` 144 × 168 | 24.192 B (≈ 24 k) | 3.024 B |

Ein einziges bildschirmfüllendes Farbbild frisst auf `gabbro` also über die
Hälfte des gesamten App-Speichers. Konsequenzen:

- **Hintergründe zeichnen statt laden.** Flächen, Verläufe und Formen kosten
  als `graphics_fill_*`-Aufrufe null Heap.
- **Bilder nur so groß wie nötig**, und nur solange im Speicher, wie sie
  gebraucht werden.
- **PDC statt PNG** für Symbole und Grafiken mit Flächen: Pebble Draw Commands
  sind Vektoren, skalieren über Plattformen hinweg und sind winzig.
- **Große Puffer nie auf den Stack.** Der Stack ist klein; ein `char
  buf[4096]` in einer Funktion ist ein Absturz mit Ansage. Statisch anlegen
  oder `malloc` mit `NULL`-Prüfung.
- **Jeden Rückgabewert von `*_create()` und `malloc()` prüfen.** Bei knappem
  Speicher liefern sie `NULL`, und der Absturz passiert erst beim nächsten
  Zugriff — weit weg von der Ursache.

Messen statt schätzen:

```c
APP_LOG(APP_LOG_LEVEL_INFO, "Heap frei: %u", (unsigned)heap_bytes_free());
```

Beim Beenden schreibt das System zusätzlich eine Heap-Statistik ins Log. Wächst
der belegte Anteil über mehrere Starts hinweg, ist ein `*_destroy` vergessen
worden.

## 7. Plattformunterschiede sauber abfangen

Nie auf Modelle abfragen, immer auf **Fähigkeiten**:

```c
window_set_background_color(win, PBL_IF_COLOR_ELSE(GColorJaegerGreen, GColorBlack));

#if defined(PBL_MICROPHONE)
  dictation_session_start(s_dictation);
#endif
```

Verfügbar sind unter anderem `PBL_COLOR`/`PBL_BW`, `PBL_RECT`/`PBL_ROUND`,
`PBL_MICROPHONE`, `PBL_HEALTH`, `PBL_TOUCH`, `PBL_SPEAKER`,
`PBL_RGB_BACKLIGHT`, `PBL_SMARTSTRAP` — zu den meisten gibt es ein
`PBL_IF_..._ELSE(a, b)` für einzelne Werte. Vollständige Liste in
`references/platforms.md`.

**Keine festen Pixelwerte.** Layouts immer aus den Fenstergrenzen ableiten:

```c
GRect bounds = layer_get_unobstructed_bounds(window_get_root_layer(window));
```

`unobstructed` statt `bounds`, weil Timeline Quick View den unteren Teil des
Bildschirms verdeckt. Runde Displays (`chalk`, `gabbro`) brauchen zusätzlich
Rand: Text am Bildschirmrand wird abgeschnitten, Menüs sehen mit
`menu_layer_set_center_focused(true)` richtig aus.

Ressourcen lassen sich pro Plattform packen — Dateisuffixe `~color`, `~bw`,
`~rect`, `~round` sowie Plattformnamen; das Build-System nimmt automatisch die
passende Variante und lässt die anderen weg.

## 8. Kommunikation mit dem Handy

Vollständig mit Code auf beiden Seiten in `references/communication.md`. Die
Regeln, an denen Projekte scheitern:

```c
app_message_register_inbox_received(inbox_received);
app_message_register_inbox_dropped(inbox_dropped);     // nie weglassen
app_message_register_outbox_failed(outbox_failed);     // nie weglassen
app_message_open(inbox_size, outbox_size);
```

- **Puffer ausrechnen**, nicht raten: Summe aus Schlüsseln und größten Werten,
  plus Overhead. Zu klein heißt `inbox_dropped` ohne jede Fehlermeldung in der
  App. Die Obergrenze liefern `app_message_inbox_size_maximum()` und
  `app_message_outbox_size_maximum()`.
- **Immer nur eine Nachricht unterwegs.** Ein zweites `app_message_outbox_send()`
  vor dem `outbox_sent`-Callback ergibt `APP_MSG_BUSY`. Mehrere Werte in *eine*
  Nachricht packen oder die nächste erst im `outbox_sent` starten.
- **Die vier Callbacks immer registrieren**, auch wenn sie nur loggen. Ohne
  `dropped`/`failed` debuggt man blind.
- **JS meldet sich zuerst.** Vor dem `ready`-Event von PebbleKit JS darf die
  Uhr nichts erwarten.
- **`pebble send-app-message` von der Kommandozeile erreicht den C-Inbox
  nicht** — es landet beim Handy-JS. Zustände, die per Einstellung gesetzt
  werden, lassen sich damit nicht durchtesten; dafür kurzzeitig den
  C-Standardwert ändern und neu bauen.

**Einstellungen** brauchen `"capabilities": ["configurable"]`, sonst zeigt die
Handy-App kein Zahnrad. Zwei Wege: Clay (`pebble package install @rebble/clay`,
deklarativ, Standardfall) oder eine handgeschriebene Seite als
`data:text/html,…`-URI ohne jede Abhängigkeit. Beide in
`references/communication.md`. Empfangene Einstellungen **sofort per
`persist_write_data` sichern**, damit die App beim nächsten Start ohne Handy
richtig aussieht.

## 9. Zeit, Timer und Hintergrund

| Zweck | Werkzeug | Hinweis |
|---|---|---|
| Uhrzeit-Anzeige | `tick_timer_service_subscribe(MINUTE_UNIT, …)` | `SECOND_UNIT` nur solange wirklich sichtbar |
| Kurze Verzögerung, Animationstakt | `app_timer_register(ms, cb, data)` | Handle merken, beim Unload `app_timer_cancel` |
| Zu fester Zeit aufwachen | `wakeup_schedule` | startet die App auch, wenn sie beendet ist |
| Dauerlauf ohne UI | Background Worker | eigener Prozess, sehr knapper Speicher |
| Weiche Bewegung | `Animation`/`PropertyAnimation` | nie Bewegung mit `psleep` bauen |

`psleep()` blockiert alles und gehört in keine App.

## 10. Akku

Ein Watchface läuft 24 Stunden am Tag; jede Aufwachphase kostet messbar
Laufzeit. Die offiziellen Empfehlungen, knapp:

- **`MINUTE_UNIT` statt `SECOND_UNIT`**, bei minimalistischen Faces `HOUR_UNIT`.
- Animationen nur beim Heben des Arms oder auf Geste, abschaltbar anbieten.
- Beschleunigungssensor **gebündelt** abholen (z. B. 10 Werte bei 10 Hz = ein
  Aufwachen pro Sekunde statt zehn), Kompass mit Winkelfilter.
- Bluetooth: Standard-Sniff-Intervall behalten, Daten (Wetter) zwischenspeichern
  statt häufig neu holen.
- Logging im Release abschalten — es läuft über dieselbe Bluetooth-Verbindung
  wie AppMessage und bremst sie aus.
- Vibration sparsam und abschaltbar.

## 11. Testen und Debuggen

Der Kreislauf, der ohne Bildschirm funktioniert (Details und alle
Emulator-Kommandos in `references/debugging.md`):

```bash
pebble build < /dev/null
pebble install --emulator emery < /dev/null        # beim ersten Mal ggf. einmal wiederholen
pebble emu-button --emulator emery click select < /dev/null
pebble screenshot --emulator emery --no-open shot.png < /dev/null
pebble kill < /dev/null
```

- **Screenshots ansehen, nicht nur erzeugen.** Ein Bild vom Launcher statt von
  der App ist der häufigste stille Fehler.
- **Langes Drücken** braucht `emu-button push` und später ein eigenes
  `release`; `click --duration` löst keinen `long_click`-Handler aus.
- **Abstürze:** `App fault!` mit `PC:`/`LR:`-Adressen. Damit und mit der
  `.elf`-Datei aus `build/` die Zeile bestimmen, siehe
  `references/debugging.md`. Die drei üblichen Ursachen sind ein `NULL`-Zeiger
  aus einem fehlgeschlagenen `_create()`, ein Zugriff hinter dem Array-Ende und
  ein Zeiger auf einen zerstörten Layer.
- **Der Emulator ist kein Beweis.** Er hat unbegrenzt Speicher im Vergleich zur
  Uhr, keinen echten Bluetooth-Stack, keinen Ton und keine Sensoren. Speicher-,
  Timing- und Akkuverhalten zeigen sich nur auf echter Hardware. Für Sideloading
  `pebble install --phone <IP>` bei aktivierter Developer Connection.

## 12. Vor dem Abliefern

Diese Punkte durchgehen — sie kosten zwei Minuten und sparen eine Runde:

1. `pebble build` läuft ohne Warnungen (Warnungen sind hier echte Hinweise).
2. Jedes `*_create` hat sein `*_destroy` im passenden `window_unload`.
3. App mehrfach hintereinander öffnen und schließen, Heap-Statistik im Log
   vergleichen — konstant?
4. Auf allen Einträgen aus `targetPlatforms` gebaut und je ein Screenshot
   angesehen; runde Plattformen extra prüfen.
5. Verhalten ohne Handy getestet (Bluetooth aus): keine leeren Felder, keine
   Hänger.
6. Logging aus dem Release entfernt oder hinter einem Schalter.
7. `scripts/check_project.py` läuft sauber durch.
8. `pebble kill` — kein Emulator läuft noch im Hintergrund.

Soll die App danach in den Store: weiter mit `pebble-publish`.

---

Die Zahlen, Kommandos und Fehlermeldungen in diesem Skill stammen aus der
offiziellen Dokumentation (`developer.repebble.com`, Stand September 2026) und
aus einem Durchlauf mit **pebble-tool 5.0.40** und **SDK 4.33.1**, in dem
Installation, Build für alle sieben Plattformen, kopfloser Emulator,
Screenshots und die Adressauflösung bei Abstürzen tatsächlich ausgeführt
wurden. Wo etwas nur aus der Dokumentation stammt, steht es dabei.
