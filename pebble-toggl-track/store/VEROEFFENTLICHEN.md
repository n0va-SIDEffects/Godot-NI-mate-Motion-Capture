# Toggl Timer im Pebble Appstore veröffentlichen

Alles Nötige liegt in `store/release/` (bzw. im ZIP `store/TogglTimer_Store_Paket.zip`).

| Datei | Verwendung im Portal |
|-------|----------------------|
| `TogglTimer-1.1.pbw` | Release hochladen |
| `icon_80.png` | Store-Icon (RGB, ohne Alpha) |
| `icon_144.png`, `icon_48.png` | Large / Small Icon, falls abgefragt |
| `banner_720x320.png` | Marketing Banner in jeder Asset Collection |
| `screenshots_en/1_…5_*.png` | Screenshots für emery (Pebble Time 2), in dieser Reihenfolge |
| `screenshots_en_basalt/*.png` | Screenshots für basalt/diorite/flint (144×168) |
| `description_en.txt` | Zeile 1 = Kurzbeschreibung, Rest = Beschreibung (≤ 1600 Zeichen) |
| `beschreibung_de.txt` | dasselbe auf Deutsch, falls eine deutsche Listung gewünscht ist |

## Portal (https://developer.repebble.com/dashboard)

1. Anmelden mit dem Konto der Pebble-Handy-App, **Add Watchapp**.
2. Grunddaten: Title **Toggl Timer**, Category **Tools & Utilities**,
   Support email eintragen, Website und Source code URL leer lassen (oder
   vollständig mit `https://`). Icon `icon_80.png` hochladen. **Create**.
3. **Add a release**: `TogglTimer-1.1.pbw` hochladen, Release Notes
   z. B. "First release". Seite neu laden, neben dem Release **Publish**.
4. **Manage Asset Collections** → **Create** für jede Plattform aus
   `targetPlatforms` (aplite, basalt, chalk, diorite, emery, flint, gabbro):
   Description aus `description_en.txt` (ohne die erste Zeile), bis zu 5
   Screenshots (emery: `screenshots_en`, 144×168-Plattformen:
   `screenshots_en_basalt`, chalk/gabbro: die emery-Bilder gehen notfalls
   auch), Banner `banner_720x320.png`. **Create Asset Collection**.
5. Oben **Publish** (öffentlich) oder **Publish Privately** (nur per Link
   zum Prüfen).

Nach einigen Minuten ist die App in der Suche der Pebble-Handy-App zu finden.

## Kommandozeile (für Updates schneller)

```bash
cd pebble-toggl-track
pebble login                                   # einmalig, öffnet den Browser
pebble build
pebble publish --release-notes "Was neu ist" --screenshots store/release/screenshots_en/*.png
```

## Checkliste

- [ ] `uuid` in `package.json` unverändert (`f71873c5-ed3c-40d7-9f27-ef8cf9e15c80`), sonst gilt das Update als neue App
- [ ] `version` im Format `Major.Minor` (aktuell `1.1`), vor jedem Update erhöhen
- [ ] Beschreibung ≤ 1600 Zeichen (`scripts/check_description.py` im Publish-Skill)
- [ ] `pebble build` ohne Fehler, `build/appinfo.json` enthält `configurable` und das Menü-Icon
- [ ] Auf der echten Uhr mit eigenem Token getestet
- [ ] Icon 80×80 ohne Alphakanal hochgeladen (bei "Server error 400" zuerst Versionsformat, dann Icon, dann Quell-URL prüfen)
