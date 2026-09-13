# HELO Remote im Pebble Appstore veröffentlichen

Stand: September 2026. Portal: https://developer.repebble.com/dashboard, Anmeldung mit dem
Konto der Pebble-App auf dem Handy. Hintergrund: https://developer.rebble.io/guides/appstore-publishing/

Der Ordner `release/` (Inhalt von `HELORemote_Store_Paket.zip`) enthält alles, was der Store abfragt:

| Datei | Verwendung im Portal |
|-------|----------------------|
| `HELORemote-1.2.pbw` | Release-Datei (die App, alle drei Plattformen in einer Datei) |
| `icon_80.png` | Icon (80 × 80, RGB ohne Alphakanal, so verlangt es das Portal) |
| `icon_144.png`, `icon_48.png` | Large und Small Icon, falls das Portal zusätzlich danach fragt |
| `banner_720x320_en.png` | Marketing Banner (Kopfbild der Listung) |
| `screenshots_emery/` | 5 Screenshots Pebble Time 2, 200 × 228 |
| `screenshots_basalt/` | 5 Screenshots Pebble Time / Time Steel, 144 × 168 Farbe |
| `screenshots_diorite/` | 5 Screenshots Pebble 2 / Core 2 Duo, 144 × 168 schwarz-weiß |
| `description_en.txt` | Kurzbeschreibung (erste Zeile) und Beschreibung, Englisch |
| `RELEASE_NOTES.md` | Release Notes je Version in kurzer und einzeiliger Fassung |

Der Store zeigt pro Plattform genau eine Beschreibung (Limit 1600 Zeichen) und einen
Screenshot-Satz, ohne Sprachvarianten. Die Listung ist deshalb nur englisch; die App selbst
folgt der Sprache der Uhr.

## Update auf 1.2 (Timeline-Pins)

Im Dashboard die App öffnen, „Add a release“ mit `HELORemote-1.2.pbw`, Release Notes aus
`RELEASE_NOTES.md` (kurze Fassung), publizieren, Beschreibung durch `description_en.txt`
ersetzen. **Zusätzlich einmalig:** auf der App-Seite im Dashboard „Enable timeline“ klicken,
sonst bekommt die Telefon-Seite keinen Timeline-Token und es erscheinen keine Pins.

## Update auf 1.1 (sieben Sprachen, Layout-Korrekturen)

Die Listung existiert seit 1.0 (am PC im Browser eingereicht; vom Handy aus antwortete das
Portal mit „Server error (400)“). Für das Update im Dashboard die App öffnen, „Add a release“
mit `HELORemote-1.1.pbw`, Release Notes aus `RELEASE_NOTES.md` (kurze Fassung), Release
publizieren. Danach in den drei Asset Collections die Screenshots durch die englischen aus
`screenshots_<plattform>/` ersetzen und die Beschreibung durch `description_en.txt`.

## Weg 1: Über das Entwicklerportal (empfohlen fürs erste Mal)

1. https://developer.repebble.com/dashboard öffnen und anmelden.
2. „Add Watchapp“ wählen (Watchapp, kein Watchface).
3. Grunddaten eintragen:
   - Title: `HELO Remote`
   - Category: `Tools & Utilities`
   - Source code URL: **Pflichtfeld** (laut Rebble-Doku), vollständig mit `https://`:
     `https://github.com/n0va-SIDEffects/Godot-NI-mate-Motion-Capture/tree/claude/pebble-watch-aja-helo-app-yr3c5o/pebble-helo-remote`
     Leer oder ohne `https://` antwortet das Portal mit „Server error (400)“.
     Website: optional, leer lassen oder ebenfalls vollständige URL.
   - Support email: deine Adresse
   - Icon: `icon_80.png`
4. „Create“.
5. „Add a release“: `HELORemote-1.2.pbw` hochladen, Release Notes aus `RELEASE_NOTES.md`
   (kurze Fassung). Seite neu laden, neben dem Release auf „Publish“.
6. „Manage Asset Collections“ → „Create“ für **jede** der drei Plattformen `emery`, `basalt`,
   `diorite` (alle stehen in `targetPlatforms`, ohne Asset Collection wird die Plattform nicht
   gelistet). Pro Plattform:
   - Description: Inhalt von `description_en.txt` ohne die erste Zeile (die Kurzbeschreibung
     wird separat abgefragt, falls das Feld existiert)
   - Screenshots aus dem passenden `screenshots_<plattform>/`-Ordner in dieser Reihenfolge:
     `03_recording`, `04_recording_streaming`, `02_ready`, `05_confirm_stop`, `01_no_ip`
   - Marketing Banner: `banner_720x320_en.png`
   - „Create Asset Collection“
7. Oben „Publish“ (öffentlich) oder „Publish Privately“ (nur per Link, zum Prüfen der Listung).

Die App erscheint nach einigen Minuten in der Suche der Pebble-App auf dem Handy.

## Weg 2: Per Kommandozeile (für Updates)

```bash
cd pebble-helo-remote
pebble login                                  # öffnet den Browser, einmalig
pebble build
pebble publish --release-notes "First release" \
  --screenshots store/release/screenshots_emery/*.png
```

Beim ersten Aufruf fragt das Werkzeug Name, Beschreibung, Kategorie und Quell-URL ab. Ohne
`--is-published` bleibt das Release zunächst unveröffentlicht und kann im Portal geprüft werden.

Für Updates: `version` in `package.json` erhöhen (Format `Major.Minor`, z. B. `1.1`),
`pebble build`, `python3 store/make_release.py --zip`, `pebble publish`. Die UUID
`2d250b0c-a72f-4830-b6a7-b16adf357abe` darf sich nie ändern, sonst gilt die App als neu.

## Fehlersuche

| Symptom | Ursache | Abhilfe |
|---------|---------|---------|
| „Server error (400)“ beim Release | `version` nicht `Major.Minor` | steht auf `1.2`, nach Änderung neu bauen |
| „Server error (400)“ im ersten Schritt vom Handy aus | mobiler Browser | am PC im Browser einreichen (so ging 1.0 durch) |
| Icon abgelehnt | Alphakanal | `icon_80.png` ist RGB; `_transparent` nur als Reserve |
| 400 beim Anlegen der Listung | Source code URL leer oder ohne `https://` | Pflichtfeld, vollständige URL eintragen |
| 400 beim Release-Upload nach einem Fehlversuch | UUID gilt schon als belegt (halb angelegte Listung) | im Dashboard die vorhandene Listung öffnen und dort weitermachen, nicht neu anlegen |
| 400 bei der Asset Collection | Screenshot-Größe passt nicht zur Plattform | emery 200×228, basalt/diorite 144×168, jeweils aus dem passenden Ordner |
| Gear/Einstellungen fehlt im Store | `configurable` fehlt | `build/appinfo.json` prüfen (`enableMultiJS` + `messageKeys`) |

## Vor dem Absenden prüfen

- UUID in `package.json` unverändert.
- Gebaut mit dem regulären SDK 4.33.1, `pebble build` ohne Warnungen.
- Beschreibung ≤ 1600 Zeichen (geprüft: 1387).
- Der Spendenlink darf in der Beschreibung stehen; Werbung von Drittanbietern ist nicht enthalten.
- **Auf der echten Uhr mit echtem HELO testen:** `pebble install --phone <IP>`, einmal Aufnahme
  und Stream starten und stoppen, Einstellungen inkl. Kaffee-Button durchgehen. Das ist der
  letzte offene Punkt, bevor die Listung öffentlich geht.
