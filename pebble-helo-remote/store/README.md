# Store-Assets für HELO Remote

Alles hier wird per Skript erzeugt, damit Änderungen reproduzierbar bleiben.

```bash
python3 store/icon/make_icons.py signal   # Icons (144/80/48, RGB + transparent) + Launcher-Icon 25 px
python3 store/banner/make_banner.py       # Banner 720x320, Deutsch und Englisch
```

| Datei | Zweck |
|---|---|
| `icon/icon_80.png` | Store-Icon, Pflichtfeld im Portal, RGB ohne Alphakanal |
| `icon/icon_144.png`, `icon/icon_48.png` | Large/Small Icon, falls das Portal fragt |
| `icon/icon_*_transparent.png` | Reserve mit Alphakanal |
| `icon/icon_master_1024.png` | Master, Quelle für alle Größen |
| `icon/konzepte_uebersicht.png` | die vier Icon-Konzepte (rec, signal, cards, ring) in 160/80/25 px |
| `banner/banner_720x320_de.png`, `_en.png` | Kopfbild der Listung |
| `../resources/images/menu_icon.png` | Launcher-Icon der Uhr (25 px), wird von `make_icons.py` mitgeschrieben |

Anderes Konzept wählen: `make_icons.py rec|signal|cards|ring`, danach `pebble build`.

Logo im Banner: eine `store/banner/logo.png` (transparenter Hintergrund) wird automatisch
185 px breit unten links eingesetzt.
