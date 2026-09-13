# Store-Assets für HELO Remote

Alles hier wird per Skript erzeugt, damit Änderungen reproduzierbar bleiben.

```bash
python3 store/icon/make_icons.py cards    # Icons (144/80/48, RGB + transparent) + Launcher-Icon 25 px
python3 store/banner/make_banner.py       # Banner 720x320, Deutsch und Englisch
```

| Datei | Zweck |
|---|---|
| `icon/icon_80.png` | Store-Icon, Pflichtfeld im Portal, RGB ohne Alphakanal |
| `icon/icon_144.png`, `icon/icon_48.png` | Large/Small Icon, falls das Portal fragt |
| `icon/icon_*_transparent.png` | Reserve mit Alphakanal |
| `icon/icon_master_1024.png` | Master, Quelle für alle Größen |
| `icon/konzepte_uebersicht.png` | die vier Icon-Konzepte (rec, signal, cards, ring) in 160/80/25 px |
| `banner/banner_720x320_de.png`, `_en.png` | Kopfbild der Listung, Logo aus `banner/logo.png` |
| `../resources/images/menu_icon.png` | Launcher-Icon der Uhr (25 px), wird von `make_icons.py` mitgeschrieben |

Anderes Konzept wählen: `make_icons.py rec|signal|cards|ring`, danach `pebble build`.

## Release-Paket

```bash
pebble build
python3 store/make_release.py --zip   # -> store/release/ und store/HELORemote_Store_Paket.zip
```

| Ort | Inhalt |
|---|---|
| `screenshots_emery/`, `screenshots_basalt/`, `screenshots_diorite/` | je 5 Store-Screenshots in nativer Auflösung |
| `release/description_*.txt`, `release/beschreibung_de.txt` | Kurzbeschreibung (Zeile 1) + Beschreibung in 7 Sprachen, ≤ 1600 Zeichen |
| `RELEASE_NOTES.md` | Release Notes je Version |
| `VEROEFFENTLICHEN.md` | Schritt-für-Schritt-Anleitung fürs Portal und die CLI |
| `release/` | zusammengestellter Ordner, Inhalt des ZIP (ZIP selbst ist in .gitignore) |

Logo im Banner: eine `store/banner/logo.png` (transparenter Hintergrund) wird automatisch
185 px breit unten links eingesetzt.
