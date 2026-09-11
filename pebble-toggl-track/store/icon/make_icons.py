#!/usr/bin/env python3
"""Renders the app icon from a 1024 px master: a Toggl-pink rounded tile with a
black stopwatch whose pink face shows a black elapsed-time wedge (concept B,
chosen by the author). Writes store sizes (RGB and transparent) and the
simplified 25 px launcher icon.
Usage: python3 make_icons.py  (needs Pillow; run from anywhere)"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
PINK = (229, 124, 216)      # Toggl brand pink
INK = (28, 24, 32)          # near-black lines

def master(size=1024, simple=False):
    ss = 4                                   # supersampling against jaggies
    S = size * ss
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, S - 1, S - 1), radius=S * 0.22, fill=PINK)
    cx, cy = S / 2, S * 0.56
    r = S * 0.33
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=INK)                       # body
    d.rounded_rectangle((cx - S * 0.08, S * 0.10, cx + S * 0.08, S * 0.24), radius=S * 0.03, fill=INK)  # crown
    d.rectangle((cx - S * 0.04, S * 0.20, cx + S * 0.04, S * 0.30), fill=INK)   # stem
    ri = S * (0.23 if simple else 0.26)
    d.ellipse((cx - ri, cy - ri, cx + ri, cy + ri), fill=PINK)                  # face
    d.pieslice((cx - ri, cy - ri, cx + ri, cy + ri), start=-90, end=0, fill=INK)  # elapsed wedge
    if not simple:
        d.ellipse((cx - S * 0.03, cy - S * 0.03, cx + S * 0.03, cy + S * 0.03), fill=PINK)  # hub
    return im.resize((size, size), Image.LANCZOS)

if __name__ == '__main__':
    m = master()
    m.save(os.path.join(HERE, 'master_1024.png'))
    for s in (512, 144, 96, 80, 48):
        ic = m.resize((s, s), Image.LANCZOS)
        ic.save(os.path.join(HERE, f'icon_{s}_transparent.png'))
        bg = Image.new('RGBA', (s, s), (255, 255, 255, 255)); bg.alpha_composite(ic)
        bg.convert('RGB').save(os.path.join(HERE, f'icon_{s}.png'))
    menu = master(200, simple=True).resize((25, 25), Image.LANCZOS)
    out = os.path.join(HERE, '..', '..', 'resources', 'images', 'menu_icon.png')
    menu.save(out)
    print('icons written; launcher icon ->', os.path.normpath(out))
