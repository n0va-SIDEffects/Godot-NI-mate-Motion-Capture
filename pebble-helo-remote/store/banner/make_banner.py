#!/usr/bin/env python3
"""
Store-Banner (720 x 320) fuer HELO Remote: der Emery-Screenshot sitzt im
Display einer freigestellten Aufnahme der Pebble Time 2.

Ohne Argumente baut das Skript das fertige Banner des Projekts:

    python3 store/banner/make_banner.py     # -> store/banner/banner_720x320_en.png

Uebernommen aus dem Skill `pebble-publish` (scripts/make_banner.py), hier
nur mit den Projektwerten als Voreinstellung. Die Uhrenaufnahme und
uhren.json liegen in store/banner/assets/, damit das Banner ohne den Skill
reproduzierbar bleibt.

`--watch drawn` zeichnet das Gehaeuse statt der Aufnahme (fuer Plattformen
ohne Foto), `--round` macht daraus ein rundes Gehaeuse. `--tilt 0` stellt
die Uhr gerade, `--watch-x` verschiebt sie, `--watch-h` setzt ihre Hoehe.
Der Store ist einsprachig, deshalb nur die englische Fassung.
"""
from PIL import Image, ImageDraw, ImageFilter, ImageFont
import argparse
import json
import os

W, H = 720, 320
DARK = (22, 30, 42)
LIGHT = (238, 242, 247)
MUTED = (150, 162, 180)
F = 3                      # Ueberabtastung

# Gehaeusefarben (references/assets.md)
KANTE = (120, 128, 142)
KORPUS = (26, 29, 36)
SCHEIBE = (8, 9, 12)
TASTE = (96, 102, 115)
BAND = (52, 58, 72)

PROJEKT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "assets")


def im_projekt(pfad):
    """Pfad relativ zum Projektordner, damit der Aufruf von ueberall klappt."""
    return os.path.join(PROJEKT, pfad)

F_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
F_REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


def schrift(pfad, groesse):
    try:
        return ImageFont.truetype(pfad, groesse)
    except OSError:
        return ImageFont.load_default()


def breite(text, pfad, groesse):
    return schrift(pfad, groesse).getbbox(text)[2]


def hex_zu_rgb(wert):
    wert = wert.lstrip("#")
    return tuple(int(wert[i:i + 2], 16) for i in (0, 2, 4))


def uhr_aus_aufnahme(shot, name, hoehe):
    """
    Screenshot in die Displayflaeche einer freigestellten Aufnahme setzen.

    Seitenverhaeltnis bleibt erhalten; der schmale Rest oben und unten
    bleibt schwarz und faellt auf dem schwarzen Display nicht auf. Die
    Ecken werden mit dem in uhren.json hinterlegten Radius maskiert, sonst
    legt sich ein hartes Rechteck ueber die Rundung.
    """
    if not os.path.isdir(ASSETS):
        raise SystemExit(
            "Uhren-Verzeichnis nicht gefunden: " + ASSETS +
            "\nMit --assets <pfad> auf den Ordner zeigen, in dem uhren.json liegt.")
    with open(os.path.join(ASSETS, "uhren.json"), encoding="utf-8") as f:
        uhren = json.load(f)
    if name not in uhren:
        raise SystemExit("Unbekannte Uhr: " + name + "\nBekannt: " +
                         ", ".join(k for k in uhren if not k.startswith("_")) + ", drawn")
    meta = uhren[name]

    uhr = Image.open(os.path.join(ASSETS, meta["datei"])).convert("RGBA")
    x0, y0, x1, y1 = meta["display"]
    dw, dh = x1 - x0 + 1, y1 - y0 + 1

    innen = Image.new("RGBA", (dw, dh), (0, 0, 0, 255))
    sh = shot.convert("RGBA").resize((dw, max(1, round(dw * shot.height / shot.width))),
                                     Image.LANCZOS)
    if sh.height > dh:
        schnitt = (sh.height - dh) // 2
        sh = sh.crop((0, schnitt, sh.width, schnitt + dh))
    innen.alpha_composite(sh, (0, (dh - sh.height) // 2))

    maske = Image.new("L", (dw, dh), 0)
    ImageDraw.Draw(maske).rounded_rectangle([0, 0, dw - 1, dh - 1], meta["eckradius"], fill=255)
    uhr.paste(innen, (x0, y0), maske)

    uhr = uhr.resize((max(1, round(uhr.width * hoehe / uhr.height)), hoehe), Image.LANCZOS)
    return uhr, meta.get("gehaeuse", "")


def uhr_zeichnen(shot, schirm_b=150, rund=False):
    """
    Gehaeuse um den Screenshot zeichnen, dreifach ueberabgetastet.

    Rand oben und unten ist absichtlich ungleich: ein Gehaeuse mit ueberall
    gleichem Rand sieht falsch aus.
    """
    if rund:
        seite = min(shot.size)
        shot = shot.crop(((shot.width - seite) // 2, (shot.height - seite) // 2,
                          (shot.width + seite) // 2, (shot.height + seite) // 2))
        schirm_h = schirm_b
    else:
        schirm_h = round(schirm_b * shot.height / shot.width)

    rand_x, rand_o, rand_u = 15, 25, 29
    geh_b = schirm_b + 2 * rand_x
    geh_h = schirm_h + rand_o + rand_u
    band_b = int(geh_b * 0.56)
    # Baender nur so lang, dass sie bis ueber den Bildrand reichen. Laenger
    # bringt nichts und laesst die gedrehte Uhr stark in die Breite wachsen:
    # aus 9 Grad Neigung wird sonst ein Bild, das rechts hinausragt.
    band_l = (H - geh_h) // 2 + 24

    lein = Image.new("RGBA", (geh_b * F, (geh_h + 2 * band_l) * F), (0, 0, 0, 0))
    d = ImageDraw.Draw(lein)
    oben = band_l * F
    unten = (band_l + geh_h) * F

    # Armbaender, nach aussen verjuengt
    schmaler = int(band_b * 0.09)
    for y0, y1, spitze_oben in ((0, oben + 8 * F, True),
                                (unten - 8 * F, lein.height, False)):
        x0 = (geh_b - band_b) // 2
        aussen = x0 + schmaler
        if spitze_oben:
            ecken = [(aussen * F, y0), ((x0 + band_b - schmaler) * F, y0),
                     ((x0 + band_b) * F, y1), (x0 * F, y1)]
        else:
            ecken = [(x0 * F, y0), ((x0 + band_b) * F, y0),
                     ((x0 + band_b - schmaler) * F, y1), (aussen * F, y1)]
        d.polygon(ecken, fill=BAND + (255,))

    # Gehaeuse
    if rund:
        d.ellipse([0, oben, geh_b * F, unten], fill=KANTE + (255,))
        d.ellipse([2 * F, oben + 2 * F, (geh_b - 2) * F, unten - 2 * F], fill=KORPUS + (255,))
    else:
        d.rounded_rectangle([0, oben, geh_b * F, unten], 26 * F, fill=KANTE + (255,))
        d.rounded_rectangle([2 * F, oben + 2 * F, (geh_b - 2) * F, unten - 2 * F],
                            24 * F, fill=KORPUS + (255,))

    # Vertiefte Displayflaeche mit 4 px Ueberstand
    sx, sy = rand_x, band_l + rand_o
    if rund:
        d.ellipse([(sx - 4) * F, (sy - 4) * F, (sx + schirm_b + 4) * F, (sy + schirm_h + 4) * F],
                  fill=SCHEIBE + (255,))
    else:
        d.rounded_rectangle([(sx - 4) * F, (sy - 4) * F,
                             (sx + schirm_b + 4) * F, (sy + schirm_h + 4) * F],
                            8 * F, fill=SCHEIBE + (255,))

    # Tasten 22 x 8: eine links auf halber Hoehe, drei rechts bei -58, 0, +58.
    # Ohne sie wirkt das Gehaeuse wie ein Telefon.
    mitte = oben + (unten - oben) // 2
    d.rounded_rectangle([-4 * F, mitte - 11 * F, 4 * F, mitte + 11 * F],
                        3 * F, fill=TASTE + (255,))
    for dy in (-58, 0, 58):
        d.rounded_rectangle([(geh_b - 4) * F, mitte + (dy - 11) * F,
                             (geh_b + 4) * F, mitte + (dy + 11) * F],
                            3 * F, fill=TASTE + (255,))

    uhr = lein.resize((geh_b, geh_h + 2 * band_l), Image.LANCZOS)

    # Screenshot einsetzen, mit abgerundeten beziehungsweise runden Ecken
    bild = shot.convert("RGBA").resize((schirm_b, schirm_h), Image.LANCZOS)
    maske = Image.new("L", (schirm_b, schirm_h), 0)
    md = ImageDraw.Draw(maske)
    if rund:
        md.ellipse([0, 0, schirm_b - 1, schirm_h - 1], fill=255)
    else:
        md.rounded_rectangle([0, 0, schirm_b - 1, schirm_h - 1], 6, fill=255)
    uhr.paste(bild, (sx, sy), maske)
    return uhr


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--shot", default=im_projekt("store/screenshots_emery/04_recording_streaming.png"))
    p.add_argument("--icon", default=im_projekt("store/icon/icon_master_1024.png"))
    p.add_argument("--logo", default=im_projekt("store/banner/logo.png"))
    p.add_argument("--title", default="HELO Remote")
    p.add_argument("--subtitle", default="for the AJA HELO")
    p.add_argument("--line", action="append", default=None)
    p.add_argument("--accent", default="#ff5f5f")
    p.add_argument("--tilt", type=float, default=9)
    p.add_argument("--watch", default="pebble_time_2",
                   help="pebble_time_2 (Standard), pebble_time_steel oder drawn")
    p.add_argument("--watch-h", type=int, default=460,
                   help="Hoehe der freigestellten Uhr; 460 statt 340, damit der "
                        "Screenshot im Display lesbar bleibt")
    p.add_argument("--round", action="store_true",
                   help="rundes Gehaeuse, nur mit --watch drawn")
    p.add_argument("--screen-w", type=int, default=150)
    p.add_argument("--watch-x", type=int, default=578,
                   help="Mitte der Uhr; bei 460 px Hoehe passt das Gehaeuse so "
                        "vollstaendig ins Bild")
    p.add_argument("--assets", help="Ordner mit uhren.json und den Uhrenbildern")
    p.add_argument("--out", default=im_projekt("store/banner/banner_720x320_en.png"))
    a = p.parse_args()
    if a.line is None:
        a.line = ["Start and stop recording and",
                  "streaming, see the live status.",
                  "7 languages - Timeline pins"]

    if a.assets:
        global ASSETS
        ASSETS = a.assets

    if not os.path.exists(a.logo):
        raise SystemExit("Logo fehlt: " + a.logo + "\nEs gehoert auf jedes Banner.")
    accent = hex_zu_rgb(a.accent)

    img = Image.new("RGBA", (W, H), DARK + (255,))
    d = ImageDraw.Draw(img)

    # Dezente Grafik im Hintergrund: helle Kurve mit wenig Deckkraft, davor
    # eine duenne volle Linie in der Akzentfarbe. Eine teildeckende
    # Akzentfarbe wuerde auf dunklem Grund zu Schlamm verlaufen.
    lein = Image.new("RGBA", (W * F, H * F), (0, 0, 0, 0))
    ld = ImageDraw.Draw(lein)
    cx, cy = int(W * 0.72 * F), int(H * 1.30 * F)
    r = int(H * 1.02 * F)
    ld.arc([cx - r, cy - r, cx + r, cy + r], 232, 308, fill=LIGHT + (26,), width=18 * F)
    r2 = int(H * 0.80 * F)
    ld.arc([cx - r2, cy - r2, cx + r2, cy + r2], 228, 312, fill=accent + (255,), width=3 * F)
    img.alpha_composite(lein.resize((W, H), Image.LANCZOS))

    if a.watch == "drawn":
        uhr = uhr_zeichnen(Image.open(a.shot), a.screen_w, a.round)
        gehaeuse = ""
    else:
        uhr, gehaeuse = uhr_aus_aufnahme(Image.open(a.shot), a.watch, a.watch_h)

    # Ein schwarzes Gehaeuse verschwindet auf dunklem Grund. Ein weicher
    # heller Schein dahinter loest es, ohne den dunklen Eindruck aufzugeben.
    if gehaeuse == "schwarz":
        fleck = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        r = int(uhr.width * 0.62)
        ImageDraw.Draw(fleck).ellipse(
            [a.watch_x - r, H // 2 - int(r * 1.25), a.watch_x + r, H // 2 + int(r * 1.25)],
            fill=(150, 168, 195, 46))
        img.alpha_composite(fleck.filter(ImageFilter.GaussianBlur(58)))

    # Schatten in derselben Form
    schatten = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sil = Image.new("RGBA", uhr.size, (0, 0, 0, 0))
    sil.paste((0, 0, 0, 150), (0, 0), uhr.split()[-1])
    if a.tilt:
        sil = sil.rotate(a.tilt, resample=Image.BICUBIC, expand=True)
    schatten.alpha_composite(sil, (a.watch_x - sil.width // 2 + 4,
                                   (H - sil.height) // 2 + 8))
    img.alpha_composite(schatten.filter(ImageFilter.GaussianBlur(9)))

    if a.tilt:
        # Gerade wirkt wie im Schaufenster, gekippt wie getragen.
        uhr = uhr.rotate(a.tilt, resample=Image.BICUBIC, expand=True)
    ux, uy = a.watch_x - uhr.width // 2, (H - uhr.height) // 2
    if uy < 0:
        uhr = uhr.crop((0, -uy, uhr.width, -uy + H))
        uy = 0
    img.alpha_composite(uhr, (ux, uy))

    # Text: Icon 110 oben links, Titel 42 pt bei x = 166
    if a.icon and os.path.exists(a.icon):
        ic = Image.open(a.icon).convert("RGBA").resize((110, 110), Image.LANCZOS)
        img.alpha_composite(ic, (36, 30))
    text_x = 166 if a.icon else 42

    grenze = 430
    groesse = 42
    while groesse > 22 and text_x + breite(a.title, F_BOLD, groesse) > grenze:
        groesse -= 1
    if groesse < 42:
        print(f"Titel auf {groesse} pt verkleinert, sonst liefe er in die Uhr.")
    d.text((text_x, 44), a.title, font=schrift(F_BOLD, groesse), fill=LIGHT)
    if a.subtitle:
        d.text((text_x + 2, 100), a.subtitle, font=schrift(F_REG, 20), fill=accent)

    # Letzte Sloganzeile in der Akzentfarbe.
    zeilen = a.line[:3]
    for i, zeile in enumerate(zeilen):
        farbe = accent if i == len(zeilen) - 1 and len(zeilen) > 1 else MUTED
        if 42 + breite(zeile, F_REG, 15) > grenze + 20:
            print("WARNUNG: Sloganzeile ist breiter als der Textbereich: " + zeile)
        d.text((42, 158 + i * 24), zeile, font=schrift(F_REG, 15), fill=farbe)

    # Logo unten links, 185 px breit, volle Deckkraft
    logo = Image.open(a.logo).convert("RGBA")
    px = logo.load()
    for y in range(logo.height):
        for x in range(logo.width):
            r_, g_, b_, al = px[x, y]
            if r_ > 235 and g_ > 235 and b_ > 235:
                px[x, y] = (r_, g_, b_, 0)
    logo = logo.crop(logo.getbbox())
    schwelle = int(logo.width * 0.42)
    px = logo.load()
    for y in range(logo.height):
        for x in range(schwelle, logo.width):
            r_, g_, b_, al = px[x, y]
            if al > 0 and r_ < 90 and g_ < 90 and b_ < 90:
                px[x, y] = (225, 232, 240, al)
    logo = logo.resize((185, int(logo.height * 185 / logo.width)), Image.LANCZOS)
    if 158 + len(zeilen) * 24 > H - logo.height - 8:
        print("WARNUNG: Sloganzeilen reichen bis ins Logo.")
    img.alpha_composite(logo, (30, H - logo.height - 8))

    img.convert("RGB").save(a.out)
    print("geschrieben:", a.out)


if __name__ == "__main__":
    main()
