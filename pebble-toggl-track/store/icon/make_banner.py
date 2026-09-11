#!/usr/bin/env python3
"""Store banner 720x320: dark ground, app icon, title, slogan, framed emery
screenshot on the right, SIDE effect's logo bottom left (per store guidance).
Usage: python3 make_banner.py  (needs Pillow; run make_icons.py first)"""
import math, os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
W, H, SS = 720, 320, 3
DARK = (22, 30, 42)
PINK = (229, 124, 216)
PINK_DIM = (120, 60, 112)

ban = Image.new('RGBA', (W * SS, H * SS), DARK + (255,))
d = ImageDraw.Draw(ban)

# background: two soft timeline bars (like a Toggl day) that stay clear of the logo
def bar(y, x0, x1, color, h=14):
    d.rounded_rectangle((x0 * SS, y * SS, x1 * SS, (y + h) * SS), radius=7 * SS, fill=color)
bar(222, 250, 450, PINK_DIM)
bar(244, 300, 420, (40, 70, 110))
bar(266, 262, 380, (70, 60, 90))

# icon top left
icon = Image.open(os.path.join(HERE, 'icon_512_transparent.png')).convert('RGBA').resize((110 * SS, 110 * SS), Image.LANCZOS)
ban.alpha_composite(icon, (34 * SS, 34 * SS))

try:
    f1 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 42 * SS)
    f2 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 20 * SS)
    f3 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 15 * SS)
except OSError:
    f1 = f2 = f3 = ImageFont.load_default()
d.text((166 * SS, 40 * SS), 'Toggl Timer', font=f1, fill='white')
d.text((168 * SS, 94 * SS), 'for Pebble Time 2', font=f2, fill=PINK)
d.text((168 * SS, 128 * SS), 'Start, stop and switch your', font=f3, fill=(200, 208, 220))
d.text((168 * SS, 150 * SS), 'Toggl Track timers from the wrist.', font=f3, fill=(200, 208, 220))
d.text((168 * SS, 176 * SS), 'Favourites · Dictation · Reminders', font=f3, fill=PINK)

# screenshot with frame, right side, vertically centred
shot = Image.open(os.path.join(HERE, '..', 'screenshots_en', '1_status.png')).convert('RGB')
frame = Image.new('RGB', (shot.width + 10, shot.height + 10), (60, 70, 85))
frame.paste(shot, (5, 5))
frame = frame.resize((frame.width * SS, frame.height * SS), Image.NEAREST)
ban.paste(frame, ((W - 30 - shot.width - 10) * SS, ((H - shot.height - 10) // 2) * SS))

# SIDE effect's logo: white background transparent, right part lightened, Pac-Man untouched
logo = Image.open(os.path.join(HERE, 'side_effects_logo.png')).convert('RGBA')
px = logo.load()
for yy in range(logo.height):
    for xx in range(logo.width):
        r, g, b, a = px[xx, yy]
        if r > 235 and g > 235 and b > 235:
            px[xx, yy] = (r, g, b, 0)
logo = logo.crop(logo.getbbox())
lw = 185 * SS
logo = logo.resize((lw, int(logo.height * lw / logo.width)), Image.LANCZOS)
px = logo.load()
split = int(logo.width * 0.42)
for yy in range(logo.height):
    for xx in range(split, logo.width):
        r, g, b, a = px[xx, yy]
        if a > 0 and r < 90 and g < 90 and b < 90:
            px[xx, yy] = (225, 232, 240, a)
ban.alpha_composite(logo, (30 * SS, H * SS - logo.height - 8 * SS))

ban = ban.resize((W, H), Image.LANCZOS)
ban.convert('RGB').save(os.path.join(HERE, 'banner_720x320.png'))
print('banner_720x320.png written')
