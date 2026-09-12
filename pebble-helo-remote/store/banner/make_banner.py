#!/usr/bin/env python3
"""Store-Banner 720x320 für HELO Remote (Deutsch und Englisch).

Aufruf: python3 make_banner.py
Liest store/icon/icon_master_1024.png und docs/emery_04_aufnahme_und_stream.png.
Unten links bleibt Platz für ein Logo (store/banner/logo.png, optional, 185 px breit).
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..')
W, H, SS = 720, 320, 3
BG = (22, 30, 42)
RED = (226, 44, 44)
BLUE = (41, 121, 209)
FONT_B = '/usr/share/fonts/truetype/freefont/FreeSansBold.ttf'
FONT_R = '/usr/share/fonts/truetype/freefont/FreeSans.ttf'

TEXTS = {
    'de': dict(sub='AJA HELO vom Handgelenk steuern',
               l1='Aufnahme und Stream starten und stoppen, Laufzeit,',
               l2='freier Speicher und Temperatur auf einen Blick.'),
    'en': dict(sub='Control your AJA HELO from your wrist',
               l1='Start and stop recording and streaming, see run time,',
               l2='free media and temperature at a glance.'),
}


def background():
    """Dunkler Grund mit großen Stream-Bögen rechts der Mitte (3-fach überabgetastet)."""
    im = Image.new('RGB', (W * SS, H * SS), BG)
    d = ImageDraw.Draw(im)
    cx, cy = 560 * SS, 330 * SS
    for r, col, w in ((330, (34, 44, 60), 46), (250, (40, 52, 70), 40), (170, (46, 60, 82), 34)):
        d.arc([cx - r * SS, cy - r * SS, cx + r * SS, cy + r * SS], 180, 360, fill=col, width=w * SS)
    return im.resize((W, H), Image.LANCZOS)


def make(lang):
    t = TEXTS[lang]
    im = background()
    d = ImageDraw.Draw(im)
    icon = Image.open(os.path.join(ROOT, 'store', 'icon', 'icon_master_1024.png')).convert('RGBA').resize((120, 120), Image.LANCZOS)
    im.paste(icon, (32, 28), icon)
    f_title = ImageFont.truetype(FONT_B, 46)
    f_sub = ImageFont.truetype(FONT_R, 19)
    f_line = ImageFont.truetype(FONT_R, 15)
    d.text((170, 38), 'HELO Remote', font=f_title, fill=(255, 255, 255))
    d.text((172, 100), t['sub'], font=f_sub, fill=(200, 210, 225))
    d.text((34, 172), t['l1'], font=f_line, fill=(170, 180, 195))
    d.text((34, 194), t['l2'], font=f_line, fill=(170, 180, 195))
    # kleine Legende REC / STREAM
    d.ellipse([34, 232, 48, 246], fill=RED); d.text((56, 230), 'REC', font=f_line, fill=(230, 230, 230))
    d.rounded_rectangle([104, 232, 118, 246], radius=3, fill=BLUE); d.text((126, 230), 'STREAM', font=f_line, fill=(230, 230, 230))
    # Screenshot rechts, 5-px-Rahmen, vertikal zentriert
    shot = Image.open(os.path.join(ROOT, 'docs', 'emery_04_aufnahme_und_stream.png')).convert('RGB')
    shot = shot.resize((180, 205), Image.LANCZOS)
    fw, fh = shot.width + 10, shot.height + 10
    x, y = W - fw - 30, (H - fh) // 2
    d.rounded_rectangle([x, y, x + fw, y + fh], radius=8, fill=(240, 240, 240))
    im.paste(shot, (x + 5, y + 5))
    # optionales Logo unten links
    logo_path = os.path.join(HERE, 'logo.png')
    if os.path.exists(logo_path):
        logo = Image.open(logo_path).convert('RGBA')
        logo = logo.resize((185, int(logo.height * 185 / logo.width)), Image.LANCZOS)
        im.paste(logo, (30, H - logo.height - 8), logo)
    out = os.path.join(HERE, f'banner_720x320_{lang}.png')
    im.save(out); print(out, im.size)


if __name__ == '__main__':
    for lang in TEXTS:
        make(lang)
