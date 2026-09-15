#!/usr/bin/env python3
"""
Erzeugt die Testbilder fuer test/jpeg.test.js.

    python3 test/fixtures/erzeugen.py     (benoetigt Pillow)

Das Motiv wird gemalt statt fotografiert: eine dunkle Buehne mit einem
Lichtkegel ist genau der Fall, an dem sich ein Bilddekoder und eine
16-Farben-Palette bewaehren muessen. Aus demselben Motiv entstehen mehrere
JPEG-Spielarten (Farbunterabtastung, Restart-Marker, ungerade Masse), dazu
je ein Vergleichsbild: das Original, mit einem Mittelwertfilter auf ein
Achtel verkleinert - also genau das, was die DC-Koeffizienten eines JPEG
enthalten.
"""
import json
import os
from PIL import Image, ImageDraw, ImageFilter

HIER = os.path.dirname(os.path.abspath(__file__))


def motiv(breite=1280, hoehe=720):
    img = Image.new("RGB", (breite, hoehe), (8, 8, 12))
    d = ImageDraw.Draw(img)
    s = breite / 1280.0
    d.rectangle([0, 0, breite, int(470 * s)], fill=(18, 16, 30))
    d.rectangle([0, int(470 * s), breite, hoehe], fill=(28, 24, 20))
    d.polygon([(int(600 * s), 0), (int(760 * s), 0),
               (int(980 * s), hoehe), (int(420 * s), hoehe)], fill=(120, 105, 70))
    d.ellipse([int(560 * s), int(430 * s), int(820 * s), int(560 * s)], fill=(150, 132, 88))
    d.ellipse([int(655 * s), int(250 * s), int(715 * s), int(320 * s)], fill=(196, 160, 130))
    d.polygon([(int(660 * s), int(320 * s)), (int(710 * s), int(320 * s)),
               (int(730 * s), int(500 * s)), (int(640 * s), int(500 * s))], fill=(140, 40, 45))
    d.rectangle([int(648 * s), int(500 * s), int(672 * s), int(560 * s)], fill=(30, 30, 40))
    d.rectangle([int(700 * s), int(500 * s), int(724 * s), int(560 * s)], fill=(30, 30, 40))
    d.rectangle([0, int(120 * s), int(90 * s), hoehe], fill=(10, 10, 14))
    d.rectangle([int(1050 * s), int(380 * s), int(1180 * s), int(520 * s)], fill=(60, 50, 40))
    return img.filter(ImageFilter.GaussianBlur(max(1, int(3 * s))))


VARIANTEN = [
    ("420",     dict(subsampling=2, quality=80), (640, 360)),
    ("422",     dict(subsampling=1, quality=80), (640, 360)),
    ("444",     dict(subsampling=0, quality=90), (640, 360)),
    ("restart", dict(subsampling=2, quality=80, restart_marker_blocks=4), (640, 360)),
    ("odd",     dict(subsampling=2, quality=75), (317, 181)),
    ("grau",    dict(subsampling=0, quality=85), (320, 180)),
]


def main():
    quelle = motiv()
    meta = {}
    for name, opts, groesse in VARIANTEN:
        img = quelle.resize(groesse, Image.LANCZOS)
        if name == "grau":
            img = img.convert("L")
        pfad = os.path.join(HIER, name + ".jpg")
        try:
            img.save(pfad, "JPEG", **opts)
        except TypeError:
            opts.pop("restart_marker_blocks", None)
            img.save(pfad, "JPEG", **opts)

        w8, h8 = -(-groesse[0] // 8), -(-groesse[1] // 8)
        ref = img.convert("RGB").resize((w8, h8), Image.BOX)
        with open(os.path.join(HIER, name + ".ref"), "wb") as f:
            f.write(ref.tobytes())

        meta[name] = {"file": name + ".jpg", "ref": name + ".ref",
                      "width": groesse[0], "height": groesse[1],
                      "dcWidth": w8, "dcHeight": h8}
        print("%-9s %sx%s -> DC %sx%s, %s Byte" %
              (name, groesse[0], groesse[1], w8, h8, os.path.getsize(pfad)))

    with open(os.path.join(HIER, "referenz.json"), "w") as f:
        json.dump(meta, f, indent=1)
    print("\nreferenz.json geschrieben")


if __name__ == "__main__":
    main()
