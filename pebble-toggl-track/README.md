# Toggl Timer für Pebble

Eine Watchapp für die **Pebble Time 2** (und alle anderen Pebbles), mit der du
deine Toggl-Track-Timer direkt vom Handgelenk startest, stoppst und wechselst.
Kein offizielles Toggl-Produkt.

```
┌──────────────────────┬───┐
│  ▌ Website ▐         │ ↻ │   UP      – Status neu laden        Tipp auf Bildschirm: Projektliste
│                      │   │
│      1:23:45         │ ■ │   SELECT  – Timer stoppen / (wenn keiner läuft) neuen starten
│                      │   │            lang drücken: Aufgabe wechseln
│  Homepage relaunch   │   │
│  Design review       │ ≡ │   DOWN    – Liste: zuletzt verwendete Einträge + Projekte
│      seit 14:02      │   │
└──────────────────────┴───┘
```

* **Touch (Pebble Time 2):** Ein Tipp auf den Bildschirm öffnet die Favoriten
  (oder die Projektliste, wenn keine Favoriten angelegt sind), ein Tipp auf ein
  Icon der Action-Bar löst die Taste daneben aus. Nach links wischen wechselt
  zum vorherigen Eintrag, nach rechts wischen stoppt den Timer.
* **Favoriten:** Bis zu vier Kacheln (Beschreibung + Projekt), in den
  Einstellungen der Pebble-App angelegt. Dazu die Kacheln "Diktieren" und
  "Liste". UP/DOWN wählen, SELECT startet, oder direkt antippen.
* **Diktat:** SELECT lang drücken (oder Kachel "Diktieren"), Beschreibung
  sprechen, Projekt wählen, fertig.
* **Status-Screen:** laufender Eintrag mit Projekt (in Projektfarbe), Kunde und
  Tags, Laufzeit sekundengenau, Startzeit und Tagessumme. Nach Start/Stopp
  vibriert die Uhr kurz und bestätigt "Gestartet: …" bzw. "Gestoppt".
* **Liste:** zuletzt verwendete Einträge der letzten 30 Tage (mit der heute
  darauf gebuchten Zeit) sowie alle aktiven Projekte. Ein laufender Timer wird
  beim Start eines neuen automatisch gestoppt.
* **Erinnerungen:** Die Uhr weckt die App, wenn ein Timer länger als N Stunden
  oder über eine Uhrzeit hinaus läuft ("Läuft noch?"), und optional werktags
  morgens, wenn kein Timer läuft. Beides in den Einstellungen konfigurierbar.
* **Rundung:** Optional wird ein gestoppter Eintrag auf 5, 15 oder 30 Minuten
  gerundet.
* Der letzte Status bleibt gespeichert und erscheint sofort beim Öffnen; im
  Launcher zeigt der App-Glance den laufenden Eintrag mit fortlaufender Dauer.
* **Demo-Modus:** Mit dem API-Token `demo` läuft die App gegen Beispieldaten,
  ohne Toggl-Konto.
* **Sprachen:** Deutsch, Englisch, Französisch, Italienisch, Spanisch. Die App
  folgt der Sprache der Uhr; alles andere fällt auf Englisch zurück.

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
src/c/status_window.c  Hauptbildschirm mit gezeichneter Action-Bar, Touch, Wischgesten
src/c/favorites_window.c Kacheln: Favoriten, Diktieren, Liste
src/c/list_window.c    Auswahlliste (Zuletzt / Projekte), auch Projektwahl nach Diktat
src/c/dictation.c      Spracheingabe der Beschreibung
src/c/reminders.c      Wakeup-Erinnerungen
src/pkjs/index.js      Handy-Seite: Protokoll, Cache, Aktionen
src/pkjs/toggl.js      Kleiner Client für die Toggl-API v9 plus Demo-Backend
src/pkjs/config.js     Einstellungsseite als data:-URL (kein Hosting nötig), Spendenlink
src/pkjs/strings.js    Übersetzungen der Handy-Seite (de, en, fr, it, es)
src/c/i18n.[ch]        Übersetzungen der Uhr-Seite
tests/pkjs_smoke.js    Node-Test mit nachgebauter Toggl-API
```

### AppMessage-Protokoll

| CMD | Richtung | Felder |
|----:|----------|--------|
| 1 `REFRESH` | Uhr → Handy | – |
| 2 `START`   | Uhr → Handy | `PROJECT_ID` (0 = ohne), `DESCRIPTION` |
| 3 `STOP`    | Uhr → Handy | – |
| 4 `PREVIOUS`| Uhr → Handy | – (laufenden Eintrag stoppen, vorherigen starten) |
| 10 `STATUS` | Handy → Uhr | `RUNNING`, `DESCRIPTION`, `PROJECT_NAME`, `PROJECT_COLOR`, `START_TIME` (UTC), `TODAY_SECONDS`, `CLIENT_NAME` |
| 11 `PROJECT`| Handy → Uhr | `INDEX`, `COUNT`, `PROJECT_ID`, `PROJECT_NAME`, `PROJECT_COLOR` |
| 12 `RECENT` | Handy → Uhr | `INDEX`, `COUNT`, `PROJECT_ID`, `DESCRIPTION`, `PROJECT_NAME`, `PROJECT_COLOR`, `TODAY_SECONDS` |
| 13 `ERROR` / 14 `INFO` | Handy → Uhr | `MESSAGE` |
| 15 `FAVORITE` | Handy → Uhr | `INDEX`, `COUNT`, `PROJECT_ID`, `DESCRIPTION`, `PROJECT_NAME`, `PROJECT_COLOR` |
| 16 `CONFIG` | Handy → Uhr | `REMIND_FLAGS`, `REMIND_MAX_HOURS`, `REMIND_LATE_HOUR`, `REMIND_START_HOUR` |

Projektfarben werden am Handy auf die 64 Pebble-Farben (`GColor8`) umgerechnet.

## Veröffentlichen

Store-Icons, Banner, Screenshots und Texte liegen in `store/`; die Schritte
für das Pebble-Entwicklerportal stehen in `store/VEROEFFENTLICHEN.md`.
`python3 store/icon/make_icons.py` und `make_banner.py` erzeugen die Grafiken
neu (brauchen Pillow).

## Unterstützen

Die App ist kostenlos und ohne Werbung. Wenn sie dir den Tag ein bisschen
leichter macht, freue ich mich über einen Kaffee:
[buymeacoffee.com/SIDEffects](https://buymeacoffee.com/SIDEffects). Der Link
steht auch unten auf der Einstellungsseite in der Pebble-App.

## Bekannte Grenzen

* Timeline-Pins sind nicht umgesetzt: Die Timeline-API braucht eine im
  Appstore veröffentlichte App, eine sideloaded App bekommt kein Token.
* Abrechenbarkeit (billable) wird nicht angezeigt oder gesetzt.
* Gebaut mit Pebble SDK 4.33.1 für alle sieben Plattformen und im Emulator
  (emery, basalt) getestet: Status, Favoriten, Liste, Tasten, Touch,
  Wischgesten, Diktat, Glance und Wakeup-Erinnerung (mit Demo-Backend). Die Handy-Logik ist
  mit einer nachgebauten Toggl-API getestet. Der Lauf mit echtem API-Token und
  auf echter Hardware steht noch aus.
