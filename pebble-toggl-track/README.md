# Toggl Track für Pebble

Eine Watchapp für die **Pebble Time 2** (und alle anderen Pebbles), mit der du
deine Toggl-Track-Timer direkt vom Handgelenk startest, stoppst und wechselst.

```
┌──────────────────────┬───┐
│  ▌ Bühne ▐           │ ↻ │   UP      – Status neu laden
│                      │   │
│      1:23:45         │ ■ │   SELECT  – Timer stoppen / (wenn keiner läuft) neuen starten
│                      │   │            lang drücken: Aufgabe wechseln
│  Licht einrichten    │   │
│  Probe Hamlet        │ ≡ │   DOWN    – Liste: zuletzt verwendete Einträge + Projekte
│      seit 14:02      │   │
└──────────────────────┴───┘
```

* Die Uhr zeigt den laufenden Eintrag mit Projekt (in Projektfarbe), Beschreibung,
  Laufzeit (sekundengenau, lokal weitergezählt) und Startzeit.
* Die Liste enthält die zuletzt verwendeten Kombinationen aus Beschreibung und
  Projekt der letzten 30 Tage sowie alle aktiven Projekte. Ein Tipp startet den
  Eintrag; ein laufender Timer wird dabei automatisch gestoppt.
* Der letzte Status bleibt gespeichert und erscheint sofort beim Öffnen; im
  Launcher zeigt der App-Glance, welcher Timer gerade läuft.

Die Uhr spricht nur mit dem Handy (PebbleKit JS); das Handy spricht mit der
[Toggl Track API v9](https://engineering.toggl.com/docs/). Der API-Token bleibt
im lokalen Speicher der Pebble-App auf dem Handy.

## Einrichtung

1. App auf die Uhr installieren (siehe unten).
2. In der Pebble-App am Handy die App öffnen → **Einstellungen** (Zahnrad).
3. API-Token eintragen. Den findest du unter
   [track.toggl.com/profile](https://track.toggl.com/profile) ganz unten.
   Optional: Workspace-ID, sonst wird der Standard-Workspace verwendet.
4. Speichern. Die Uhr lädt sofort Status, Projekte und letzte Einträge.

## Bauen und installieren

Voraussetzung ist das aktuelle Pebble-SDK von Core Devices:

```bash
uv tool install pebble-tool     # einmalig
pebble sdk install latest       # einmalig
```

Dann im Ordner `pebble-toggl-track`:

```bash
pebble build                                   # baut build/pebble-toggl-track.pbw
pebble install --phone <IP-des-Handys>         # auf die Uhr (Developer Connection in der Pebble-App an)
pebble install --emulator emery                # oder im Emulator (emery = Pebble Time 2)
pebble logs --emulator emery                   # Logs von Uhr und JS
```

Der Emulator kann keine Einstellungsseite öffnen; dort helfen
`pebble emu-app-config --emulator emery` oder ein Test auf der echten Uhr.

Ohne Pebble-SDK lässt sich die Handy-Logik trotzdem testen:

```bash
node tests/pkjs_smoke.js
```

## Aufbau

```
package.json           App-Metadaten, Zielplattformen, AppMessage-Keys
wscript                Standard-Build-Skript des Pebble-SDK
src/c/main.c           Einstieg, App-Glance
src/c/model.[ch]       Gemeinsamer Zustand, Persistenz des letzten Status
src/c/comm.[ch]        AppMessage-Protokoll zur Handy-Seite
src/c/status_window.c  Hauptbildschirm mit gezeichneter Action-Bar
src/c/list_window.c    Auswahlliste (Zuletzt / Projekte)
src/pkjs/index.js      Handy-Seite: Protokoll, Cache, Aktionen
src/pkjs/toggl.js      Kleiner Client für die Toggl-API v9
src/pkjs/config.js     Einstellungsseite als data:-URL (kein Hosting nötig)
tests/pkjs_smoke.js    Node-Test mit nachgebauter Toggl-API
```

### AppMessage-Protokoll

| CMD | Richtung | Felder |
|----:|----------|--------|
| 1 `REFRESH` | Uhr → Handy | – |
| 2 `START`   | Uhr → Handy | `PROJECT_ID` (0 = ohne), `DESCRIPTION` |
| 3 `STOP`    | Uhr → Handy | – |
| 10 `STATUS` | Handy → Uhr | `RUNNING`, `DESCRIPTION`, `PROJECT_NAME`, `PROJECT_COLOR`, `START_TIME` (UTC) |
| 11 `PROJECT`| Handy → Uhr | `INDEX`, `COUNT`, `PROJECT_ID`, `PROJECT_NAME`, `PROJECT_COLOR` |
| 12 `RECENT` | Handy → Uhr | `INDEX`, `COUNT`, `PROJECT_ID`, `DESCRIPTION`, `PROJECT_NAME`, `PROJECT_COLOR` |
| 13 `ERROR` / 14 `INFO` | Handy → Uhr | `MESSAGE` |

Projektfarben werden am Handy auf die 64 Pebble-Farben (`GColor8`) umgerechnet.

## Bekannte Grenzen

* Die Beschreibung eines neuen Eintrags kommt aus der Liste der letzten
  Einträge; freie Texteingabe (Diktat) gibt es noch nicht.
* Toggl-Tags, Kunden und Abrechenbarkeit werden nicht angezeigt.
* Der Code wurde gegen die Pebble-SDK-Header für aplite, basalt, chalk, diorite
  und emery kompiliert und die Handy-Logik mit einer nachgebauten Toggl-API
  getestet. Ein Lauf auf echter Hardware oder im Emulator steht noch aus.
