#!/usr/bin/env python3
"""Store-Banner 720x320 für HELO Remote (Deutsch und Englisch).

Aufruf: python3 make_banner.py
Liest store/icon/icon_master_1024.png und screenshots_emery/04_aufnahme_stream.png.
Das SIDE effect's Logo (store/banner/logo.png) kommt 185 px breit unten links hinein:
weißer Hintergrund wird transparent, Pulslinie/Schriftzug werden auf dem dunklen Grund
aufgehellt, der Pac-Man samt schwarzem X bleibt unverändert.
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


def prepare_logo(path, width=185):
    logo = Image.open(path).convert('RGBA')
    px = logo.load()
    for yy in range(logo.height):                      # weißen Hintergrund transparent
        for xx in range(logo.width):
            r, g, b, a = px[xx, yy]
            if r > 235 and g > 235 and b > 235:
                px[xx, yy] = (r, g, b, 0)
    logo = logo.crop(logo.getbbox())
    logo = logo.resize((width, int(logo.height * width / logo.width)), Image.LANCZOS)
    px = logo.load(); split = int(logo.width * 0.42)   # rechter Teil: Schwarz -> Hellgrau
    for yy in range(logo.height):
        for xx in range(split, logo.width):
            r, g, b, a = px[xx, yy]
            if a > 0 and r < 90 and g < 90 and b < 90:
                px[xx, yy] = (225, 232, 240, a)
    return logo


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
    d.text((34, 160), t['l1'], font=f_line, fill=(170, 180, 195))
    d.text((34, 182), t['l2'], font=f_line, fill=(170, 180, 195))
    # kleine Legende REC / STREAM
    d.ellipse([34, 210, 48, 224], fill=RED); d.text((56, 208), 'REC', font=f_line, fill=(230, 230, 230))
    d.rounded_rectangle([104, 210, 118, 224], radius=3, fill=BLUE); d.text((126, 208), 'STREAM', font=f_line, fill=(230, 230, 230))
    # Screenshot rechts, 5-px-Rahmen, vertikal zentriert
    shot = Image.open(os.path.join(ROOT, 'store', 'screenshots_emery', '04_aufnahme_stream.png')).convert('RGB')
    shot = shot.resize((180, 205), Image.LANCZOS)
    fw, fh = shot.width + 10, shot.height + 10
    x, y = W - fw - 30, (H - fh) // 2
    d.rounded_rectangle([x, y, x + fw, y + fh], radius=8, fill=(240, 240, 240))
    im.paste(shot, (x + 5, y + 5))
    # SIDE effect's Logo unten links (Aufbereitung wie beim Theremin-Banner)
    logo_path = os.path.join(HERE, 'logo.png')
    if os.path.exists(logo_path):
        logo = prepare_logo(logo_path)
        im.paste(logo, (30, H - logo.height - 8), logo)
    out = os.path.join(HERE, f'banner_720x320_{lang}.png')
    im.save(out); print(out, im.size)


if __name__ == '__main__':
    for lang in TEXTS:
        make(lang)
