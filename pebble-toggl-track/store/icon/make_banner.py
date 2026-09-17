#!/usr/bin/env python3
"""Store banner 720x320: dark ground, app icon, title, slogan, the emery
screenshot inside a drawn, slightly tilted Pebble Time 2, SIDE effect's logo
bottom left.
Usage: python3 make_banner.py  (needs Pillow; run make_icons.py first)"""
import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
W, H, SS = 720, 320, 3          # SS: supersampling, curves alias without it
DARK = (22, 30, 42)
PINK = (229, 124, 216)
PINK_DIM = (120, 60, 112)
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
FONTB = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'

SCREENSHOT = os.path.join(HERE, '..', 'screenshots_en', '1_status.png')
WATCH_SCREEN_W = 150            # display width in banner units (emery is 200x228)
WATCH_CENTRE_X = 590
STRAP_LEN = 145                 # straps run out of the top and bottom edges
WATCH_TILT = 9                  # degrees; a tilted watch looks worn, not shelved


def font(size, bold=False):
    try:
        return ImageFont.truetype(FONTB if bold else FONT, size * SS)
    except OSError:
        return ImageFont.load_default()


def watch(screen_w=WATCH_SCREEN_W, strap_len=STRAP_LEN):
    """The screenshot in a Pebble Time 2 seen from the front (RGBA, SS scale)."""
    s = Image.open(SCREENSHOT).convert('RGB')
    sw = screen_w
    sh = int(round(sw * s.height / s.width))
    bx, bt, bb = 15, 25, 29                      # bezel left/right, top, bottom
    body_w, body_h = sw + 2 * bx, sh + bt + bb
    strap_w = int(body_w * 0.56)
    pad = 12                                     # room for the side buttons
    img = Image.new('RGBA', ((body_w + 2 * pad) * SS, (body_h + 2 * strap_len) * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    ox, oy = pad * SS, strap_len * SS

    def rr(box, radius, **kw):
        d.rounded_rectangle([int(v) for v in box], radius=int(radius), **kw)

    # straps, slightly tapered, cropped by the banner edge later
    for top in (True, False):
        x0 = ox + (body_w - strap_w) / 2 * SS
        x1 = x0 + strap_w * SS
        taper = strap_w * 0.09 * SS
        if top:
            y0, y1 = 0, oy + 40 * SS
            poly = [(x0 + taper, y0), (x1 - taper, y0), (x1, y1), (x0, y1)]
        else:
            y0, y1 = oy + (body_h - 40) * SS, img.height
            poly = [(x0, y0), (x1, y0), (x1 - taper, y1), (x0 + taper, y1)]
        d.polygon(poly, fill=(52, 58, 72, 255))
        d.line([poly[0], poly[-1]], fill=(60, 67, 80, 255), width=2 * SS)
        d.line([poly[1], poly[2]], fill=(14, 17, 22, 255), width=2 * SS)

    # soft shadow so the case lifts off the background
    shadow = Image.new('RGBA', img.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        (ox + 4 * SS, oy + 8 * SS, ox + body_w * SS + 4 * SS, oy + body_h * SS + 10 * SS),
        radius=int(26 * SS), fill=(0, 0, 0, 150))
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(9 * SS)))

    # side buttons: one left (back), three right (up, select, down)
    btn = (96, 102, 115, 255)
    bh, bw = 22 * SS, 8 * SS
    cy = oy + body_h / 2 * SS
    rr((ox - bw + 2 * SS, cy - bh / 2, ox + 10 * SS, cy + bh / 2), 4 * SS, fill=btn)
    for dy in (-58, 0, 58):
        y = cy + dy * SS
        rr((ox + (body_w - 10) * SS, y - bh / 2, ox + body_w * SS + bw - 2 * SS, y + bh / 2),
           4 * SS, fill=btn)

    # case: thin light rim, dark body, recessed display
    rr((ox, oy, ox + body_w * SS, oy + body_h * SS), 26 * SS, fill=(120, 128, 142, 255))
    rr((ox + 2 * SS, oy + 2 * SS, ox + (body_w - 2) * SS, oy + (body_h - 2) * SS),
       24 * SS, fill=(26, 29, 36, 255))
    rr((ox + (bx - 4) * SS, oy + (bt - 5) * SS, ox + (bx + sw + 4) * SS, oy + (bt + sh + 5) * SS),
       8 * SS, fill=(8, 9, 12, 255))
    img.paste(s.resize((sw * SS, sh * SS), Image.LANCZOS), (int(ox + bx * SS), int(oy + bt * SS)))
    return img


def logo(width=185):
    """SIDE effect's logo: white made transparent, wordmark lightened for the
    dark ground, Pac-Man and its black X untouched."""
    im = Image.open(os.path.join(HERE, 'side_effects_logo.png')).convert('RGBA')
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            if r > 235 and g > 235 and b > 235:
                px[x, y] = (r, g, b, 0)
    im = im.crop(im.getbbox())
    lw = width * SS
    im = im.resize((lw, int(im.height * lw / im.width)), Image.LANCZOS)
    px = im.load()
    split = int(im.width * 0.42)
    for y in range(im.height):
        for x in range(split, im.width):
            r, g, b, a = px[x, y]
            if a > 0 and r < 90 and g < 90 and b < 90:
                px[x, y] = (225, 232, 240, a)
    return im


ban = Image.new('RGBA', (W * SS, H * SS), DARK + (255,))
d = ImageDraw.Draw(ban)

# background: soft timeline bars (like a Toggl day), clear of logo and watch
for y, x0, x1, col in ((222, 250, 450, PINK_DIM), (244, 300, 420, (40, 70, 110)),
                       (266, 262, 380, (70, 60, 90))):
    d.rounded_rectangle((x0 * SS, y * SS, x1 * SS, (y + 14) * SS), radius=7 * SS, fill=col)

icon = Image.open(os.path.join(HERE, 'icon_512_transparent.png')).convert('RGBA')
ban.alpha_composite(icon.resize((110 * SS, 110 * SS), Image.LANCZOS), (34 * SS, 34 * SS))

d.text((166 * SS, 40 * SS), 'Toggl Timer', font=font(42, True), fill='white')
d.text((168 * SS, 94 * SS), 'for Pebble Time 2', font=font(20), fill=PINK)
d.text((168 * SS, 128 * SS), 'Start, stop and switch your', font=font(15), fill=(200, 208, 220))
d.text((168 * SS, 150 * SS), 'Toggl Track timers from the wrist.', font=font(15), fill=(200, 208, 220))
d.text((168 * SS, 176 * SS), 'Favourites · Dictation · Reminders', font=font(15), fill=PINK)

w = watch().rotate(WATCH_TILT, resample=Image.BICUBIC, expand=True)
ban.alpha_composite(w, (WATCH_CENTRE_X * SS - w.width // 2, H * SS // 2 - w.height // 2))

lg = logo()
ban.alpha_composite(lg, (30 * SS, H * SS - lg.height - 8 * SS))

ban.resize((W, H), Image.LANCZOS).convert('RGB').save(os.path.join(HERE, 'banner_720x320.png'))
print('banner_720x320.png written')
