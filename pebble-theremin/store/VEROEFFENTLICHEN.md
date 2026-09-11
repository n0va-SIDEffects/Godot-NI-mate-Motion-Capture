# Theremin im Pebble Appstore veröffentlichen

Stand: September 2026. Der Appstore wird von Core Devices (Pebble) betrieben,
das Entwicklerportal liegt unter https://developer.repebble.com/dashboard.
Die Anmeldung läuft mit demselben Konto wie in der Pebble-App auf dem Handy.

Der Ordner `release/` enthält alles, was der Store abfragt:

| Datei | Verwendung im Portal |
|-------|----------------------|
| `Theremin-1.0.0.pbw` | Release-Datei (die App) |
| `icon_80.png` | Icon (80 × 80, so verlangt es das Portal) |
| `icon_144.png`, `icon_48.png` | Large und Small Icon, falls das Portal zusätzlich danach fragt |
| `banner_720x320.png` | Marketing Banner (Kopfbild der Listung) |
| `screenshots_en/` | bis zu 5 Screenshots, englische Oberfläche, 200 × 228 |
| `screenshots_de/` | dieselben Motive auf Deutsch (optional, für Beschreibungen) |
| `description_en.txt` | Kurzbeschreibung (erste Zeile) und Beschreibung, Englisch |
| `beschreibung_de.txt` | dieselben Texte auf Deutsch |

Der Store zeigt pro App eine Beschreibung; Englisch ist die sichere Wahl,
weil der Store international ist. Der deutsche Text kann als zweiter Absatz
angehängt werden (Limit: 1600 Zeichen pro Beschreibung, beide Texte zusammen
sprengen das, also eine Sprache wählen oder kürzen).

## Weg 1: Über das Entwicklerportal (empfohlen fürs erste Mal)

1. https://developer.repebble.com/dashboard öffnen und anmelden.
2. "Add Watchapp" wählen (die App ist eine Watchapp, kein Watchface).
3. Grunddaten eintragen:
   - Title: `Theremin`
   - Category: `Games` oder `Tools & Utilities` (Musik gibt es nicht als eigene Kategorie; Games passt am besten)
   - Website / Source code URL: das GitHub-Repository oder leer lassen
   - Support email: deine Adresse
   - Icon: `icon_80.png` (80 × 80)
4. Listing anlegen ("Create").
5. "Add a release": `Theremin-1.0.0.pbw` hochladen, optional Release Notes
   (z. B. "Erste Version"). Seite neu laden, dann neben dem Release auf
   "Publish" klicken.
6. "Manage Asset Collections" → "Create" für die Plattform **emery**
   (Pebble Time 2). Dort:
   - Description: Inhalt von `description_en.txt` (Kurzbeschreibung als erste Zeile weglassen, sie wird separat abgefragt, falls das Feld existiert)
   - Screenshots: bis zu 5 aus `screenshots_en/` in dieser Reihenfolge:
     `03_playing_sine`, `06_sawtooth`, `05_square`, `07_settings`, `09_muted_gate`
   - Marketing Banner: `banner_720x320.png`
   - "Create Asset Collection"
7. Oben auf "Publish" klicken. Mit "Publish Privately" ist die App nur über
   den direkten Link erreichbar, praktisch zum Testen der Listung.

Nach dem Veröffentlichen erscheint die App in der Suche der Pebble-App auf
dem Handy. Das kann einige Minuten dauern.

## Weg 2: Per Kommandozeile (für Updates)

Das SDK kann seit 2026 direkt veröffentlichen. Im Projektordner:

```bash
pebble login          # öffnet den Browser, einmalig
pebble build
pebble publish --release-notes "Erste Version"
```

Beim ersten Aufruf fragt das Werkzeug Name, Beschreibung, Kategorie und
Quell-URL ab und legt die Listung an. Screenshots und Rollover-GIFs erzeugt
es selbst im Emulator; mit `--screenshots store/release/screenshots_en/*.png`
lassen sich stattdessen die vorbereiteten Bilder hochladen. Ohne
`--is-published` bleibt das Release zunächst unveröffentlicht und kann im
Portal geprüft werden.

Für spätere Updates: Version in `package.json` erhöhen (z. B. `1.0.1`),
`pebble build`, `pebble publish`. Die UUID der App darf sich nie ändern,
sonst gilt sie als neue App.

## Vor dem Absenden prüfen

- Die UUID in `package.json` ist einmalig und bleibt für alle Versionen gleich.
- Die App ist mit dem regulären SDK (4.33.1, kein Beta) gebaut.
- Die Beschreibung hat höchstens 1600 Zeichen.
- Der Spendenlink darf in der Beschreibung stehen; Werbung im Sinne von
  Drittanbietern ist nicht enthalten.
- Auf der echten Uhr einmal frisch installieren und Ton, Menü und
  Handy-Einstellungen durchgehen.
