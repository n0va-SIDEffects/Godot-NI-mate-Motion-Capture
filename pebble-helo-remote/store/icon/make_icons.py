#!/usr/bin/env python3
"""Icon-Generator für HELO Remote.

Zeichnet die Icon-Konzepte als 1024-px-Master (3-fach überabgetastet) und
erzeugt daraus die Store-Größen (144, 80, 48; RGB ohne Alpha + transparent)
sowie das 25-px-Launcher-Icon für die Uhr.

Aufruf:  python3 make_icons.py [konzept]  (Standard: signal)
Konzepte: rec, signal, cards, ring
"""
import os, sys
from PIL import Image, ImageDraw

SS = 3                      # Supersampling
M = 1024                    # Master-Kantenlänge
DARK = (28, 34, 46, 255)    # Hintergrund (wie App-Kopfzeile)
RED = (226, 44, 44, 255)    # REC
BLUE = (41, 121, 209, 255)  # STREAM
WHITE = (255, 255, 255, 255)
GREY = (150, 158, 170, 255)


def canvas():
    im = Image.new('RGBA', (M * SS, M * SS), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)


def rounded_bg(d, radius=0.22, color=DARK, inset=0.0):
    s = M * SS
    i = int(s * inset)
    d.rounded_rectangle([i, i, s - 1 - i, s - 1 - i], radius=int(s * radius), fill=color)


def dot(d, cx, cy, r, color):
    s = M * SS
    d.ellipse([(cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s], fill=color)


def ring(d, cx, cy, r, w, color):
    s = M * SS
    d.ellipse([(cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s], outline=color, width=int(w * s))


def arc(d, cx, cy, r, w, a0, a1, color):
    s = M * SS
    d.arc([(cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s], a0, a1, fill=color, width=int(w * s))


def concept_rec(launcher=False):
    """Klassisch: roter REC-Punkt mit weißem Ring auf dunklem Grund."""
    im, d = canvas(); rounded_bg(d)
    ring(d, .5, .5, .30, .045 if launcher else .035, WHITE)
    dot(d, .5, .5, .19, RED)
    return im


def concept_signal(launcher=False):
    """REC-Punkt plus zwei Stream-Wellen nach rechts oben (Aufnahme + Stream)."""
    im, d = canvas(); rounded_bg(d)
    cx, cy = .40, .60
    dot(d, cx, cy, .17, RED)
    w = .085 if launcher else .07
    arc(d, cx, cy, .30, w, -80, 10, WHITE)
    arc(d, cx, cy, .44, w, -80, 10, WHITE if not launcher else GREY)
    return im


def concept_cards(launcher=False):
    """Mini-Abbild der App: rote REC-Karte oben, blaue STREAM-Karte unten."""
    im, d = canvas(); rounded_bg(d)
    s = M * SS
    pad, gap = .12, .05
    h = (1 - 2 * pad - gap) / 2
    r = int(.07 * s)
    d.rounded_rectangle([pad * s, pad * s, (1 - pad) * s, (pad + h) * s], radius=r, fill=RED)
    d.rounded_rectangle([pad * s, (pad + h + gap) * s, (1 - pad) * s, (1 - pad) * s], radius=r, fill=BLUE)
    dot(d, .5, pad + h / 2, .11 if launcher else .09, WHITE)
    tri_cy = pad + h + gap + h / 2; tr = .12 if launcher else .10
    d.polygon([((.5 - tr * .8) * s, (tri_cy - tr) * s), ((.5 - tr * .8) * s, (tri_cy + tr) * s), ((.5 + tr * 1.0) * s, tri_cy * s)], fill=WHITE)
    return im


def concept_ring(launcher=False):
    """Roter Punkt in blauem Stream-Ring: Aufnahme im Stream."""
    im, d = canvas(); rounded_bg(d)
    ring(d, .5, .5, .36, .09 if launcher else .075, BLUE)
    dot(d, .5, .5, .19, RED)
    return im


CONCEPTS = {'rec': concept_rec, 'signal': concept_signal, 'cards': concept_cards, 'ring': concept_ring}


def down(im, size):
    return im.resize((size, size), Image.LANCZOS)


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else 'signal'
    here = os.path.dirname(os.path.abspath(__file__))
    fn = CONCEPTS[name]
    master = down(fn(), M)
    master.save(os.path.join(here, 'icon_master_1024.png'))
    for size in (144, 80, 48):
        ic = down(master, size)
        ic.save(os.path.join(here, f'icon_{size}_transparent.png'))
        bg = Image.new('RGBA', (size, size), WHITE); bg.alpha_composite(ic)
        bg.convert('RGB').save(os.path.join(here, f'icon_{size}.png'))
    # Launcher-Icon: vereinfachte Variante mit dickeren Strichen, ohne Hintergrund-Rundung
    launcher = down(fn(launcher=True), 25)
    launcher.save(os.path.join(here, '..', '..', 'resources', 'images', 'menu_icon.png'))
    print('Icons erzeugt für Konzept:', name)


if __name__ == '__main__':
    main()
