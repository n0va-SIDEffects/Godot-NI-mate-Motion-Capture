#!/usr/bin/env python3
"""Renders the app icon from a 1024 px master: a Toggl-pink rounded tile with a
white stopwatch ring and a play triangle. Writes store sizes (RGB and
transparent) and the simplified 25 px launcher icon.
Usage: python3 make_icons.py  (needs Pillow; run from anywhere)"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
PINK = (229, 124, 216)      # Toggl brand pink
DARK = (38, 22, 40)

def master(size=1024, simple=False):
    ss = 4                                   # supersampling against jaggies
    S = size * ss
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    r = S * 0.22
    d.rounded_rectangle((0, 0, S - 1, S - 1), radius=r, fill=PINK)
    cx, cy = S / 2, S * 0.54
    ring = S * (0.30 if simple else 0.29)
    width = S * (0.085 if simple else 0.065)
    d.ellipse((cx - ring, cy - ring, cx + ring, cy + ring), outline='white', width=int(width))
    # stopwatch crown + button
    d.rounded_rectangle((cx - S * 0.07, S * 0.13, cx + S * 0.07, S * 0.24), radius=S * 0.02, fill='white')
    if not simple:
        d.rounded_rectangle((cx + S * 0.20, S * 0.17, cx + S * 0.29, S * 0.26), radius=S * 0.02, fill='white')
        d.rectangle((cx - S * 0.035, S * 0.24, cx + S * 0.035, S * 0.30), fill='white')
    # play triangle
    t = S * (0.15 if simple else 0.13)
    d.polygon([(cx - t * 0.8, cy - t), (cx - t * 0.8, cy + t), (cx + t * 1.1, cy)], fill='white')
    return im.resize((size, size), Image.LANCZOS)

if __name__ == '__main__':
    m = master()
    m.save(os.path.join(HERE, 'master_1024.png'))
    for s in (512, 144, 96, 80, 48):
        ic = m.resize((s, s), Image.LANCZOS)
        ic.save(os.path.join(HERE, f'icon_{s}_transparent.png'))
        bg = Image.new('RGBA', (s, s), (255, 255, 255, 255)); bg.alpha_composite(ic)
        bg.convert('RGB').save(os.path.join(HERE, f'icon_{s}.png'))
    # launcher icon: simplified drawing, then downscale
    menu = master(200, simple=True).resize((25, 25), Image.LANCZOS)
    out = os.path.join(HERE, '..', '..', 'resources', 'images', 'menu_icon.png')
    menu.save(out)
    print('icons written; launcher icon ->', os.path.normpath(out))
