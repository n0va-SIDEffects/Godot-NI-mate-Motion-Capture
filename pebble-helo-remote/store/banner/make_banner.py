#!/usr/bin/env python3
"""Store-Banner 720x320 für HELO Remote: der Screenshot steckt in einer
gezeichneten Pebble.

Layout: dunkler Grund, App-Icon und Text links, rechts die leicht gekippte
Uhr, Armbänder laufen oben und unten aus dem Bild, SIDE effect's Logo unten
links. Der Store ist einsprachig, deshalb nur die englische Fassung.

Die Uhr ist gezeichnet, nicht fotografiert (der Skill liefert kein Foto, und
ein lizenziertes bräuchte Rechte), bekommt hier aber einen Fotolook:
Metallverlauf auf Kante und Korpus, Verlauf auf den Armbändern, Spiegelung
auf dem Deckglas, weicherer Schlagschatten. `--flat` schaltet das ab und
liefert die flachen Flächen der Skill-Vorlage.

Ohne Argumente baut das Skript das fertige Banner des Projekts:

    python3 store/banner/make_banner.py        # -> store/banner/banner_720x320_en.png

Einzelne Angaben lassen sich überschreiben (`--shot`, `--title`, `--accent`,
`--tilt 0` für eine gerade Uhr, `--round` für runde Displays). Vorlage und
Maße stammen aus dem Skill `pebble-publish` (references/assets.md).
Ergebnis immer ansehen, bevor es in den Store geht.
"""
import argparse
import os
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

W, H, SS = 720, 320, 3          # SS: Supersampling, sonst zacken die Rundungen
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def rel(path):
    """Pfad relativ zum Projektordner, damit der Aufruf von überall klappt."""
    return os.path.join(ROOT, path)
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
FONTB = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'


def hex_rgb(s):
    s = s.lstrip('#')
    return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4))


def font(size, bold=False):
    try:
        return ImageFont.truetype(FONTB if bold else FONT, size * SS)
    except OSError:
        return ImageFont.load_default()


def _mix(c0, c1, t):
    return tuple(int(round(a + (b - a) * t)) for a, b in zip(c0, c1))


def _grad(size, tl, br):
    """Weicher diagonaler Verlauf: 2x2-Miniatur bikubisch hochskaliert."""
    small = Image.new('RGB', (2, 2))
    mid = _mix(tl, br, 0.5)
    small.putpixel((0, 0), tl)
    small.putpixel((1, 0), mid)
    small.putpixel((0, 1), mid)
    small.putpixel((1, 1), br)
    return small.resize((max(size[0], 1), max(size[1], 1)), Image.BICUBIC)


def _shade(img, box, tl, br, radius=None, ellipse=False, poly=None, outline=None):
    """Füllt eine Form mit einem Verlauf statt mit einer flachen Farbe:
    aus dem gezeichneten Gehäuse wird so gebürstetes Metall. Mit `outline`
    wird nur der Rand gefüllt, das ergibt die Fase am Gehäuse."""
    x0, y0, x1, y1 = [int(v) for v in box]
    w, h = x1 - x0, y1 - y0
    if w <= 0 or h <= 0:
        return
    mask = Image.new('L', (w, h), 0)
    md = ImageDraw.Draw(mask)
    if poly is not None:
        md.polygon([(px - x0, py - y0) for px, py in poly], fill=255)
    elif ellipse:
        if outline:
            md.ellipse((0, 0, w - 1, h - 1), outline=255, width=int(outline))
        else:
            md.ellipse((0, 0, w - 1, h - 1), fill=255)
    elif outline:
        md.rounded_rectangle((0, 0, w - 1, h - 1), radius=int(radius),
                             outline=255, width=int(outline))
    else:
        md.rounded_rectangle((0, 0, w - 1, h - 1), radius=int(radius), fill=255)
    img.paste(_grad((w, h), tl, br), (x0, y0), mask)


def _glass(img, box, radius, ellipse=False):
    """Spiegelung auf dem Deckglas: breite Lichtbahn über die obere linke
    Hälfte, dazu ein schmaler heller Streifen an der Kante."""
    x0, y0, x1, y1 = [int(v) for v in box]
    w, h = x1 - x0, y1 - y0
    shape = Image.new('L', img.size, 0)
    sd = ImageDraw.Draw(shape)
    if ellipse:
        sd.ellipse((x0, y0, x1, y1), fill=255)
    else:
        sd.rounded_rectangle((x0, y0, x1, y1), radius=int(radius), fill=255)
    light = Image.new('L', img.size, 0)
    ld = ImageDraw.Draw(light)
    # dezent halten: der Screenshot ist das Produkt und muss knackig bleiben
    ld.polygon([(x0, y0), (x0 + w * 0.70, y0), (x0, y0 + h * 0.76)], fill=13)
    ld.polygon([(x0, y0 + h * 0.10), (x0 + w * 0.30, y0),
                (x0 + w * 0.46, y0), (x0, y0 + h * 0.28)], fill=40)
    light = light.filter(ImageFilter.GaussianBlur(4 * SS))
    img.paste((255, 255, 255), (0, 0), ImageChops.multiply(light, shape))


def watch(shot_path, screen_w=150, strap_len=145, round_display=False, photo=True):
    """Screenshot in einem gezeichneten Uhrengehäuse (RGBA, SS-Maßstab).

    screen_w ist die Displaybreite in Bannereinheiten (emery 200x228,
    basalt 144x168, chalk 180x180 rund). Die Höhe folgt dem Screenshot.
    """
    s = Image.open(shot_path).convert('RGB')
    if round_display and s.width != s.height:     # runde Displays sind quadratisch
        side = min(s.width, s.height)
        s = s.crop(((s.width - side) // 2, (s.height - side) // 2,
                    (s.width + side) // 2, (s.height + side) // 2))
    sw = screen_w
    sh = int(round(sw * s.height / s.width))
    bx, bt, bb = 15, 25, 29                      # Rand links/rechts, oben, unten
    if round_display:
        bx = bt = bb = 16
    body_w, body_h = sw + 2 * bx, sh + bt + bb
    strap_w = int(body_w * 0.56)
    pad = 12                                     # Platz für die seitlichen Tasten
    img = Image.new('RGBA', ((body_w + 2 * pad) * SS, (body_h + 2 * strap_len) * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    ox, oy = pad * SS, strap_len * SS

    def rr(box, radius, **kw):
        d.rounded_rectangle([int(v) for v in box], radius=int(radius), **kw)

    # Armbänder, leicht verjüngt; der Bannerrand schneidet sie ab
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
        if photo:                                # Silikon: links Licht, rechts Schatten
            xs = [pt[0] for pt in poly]
            ys = [pt[1] for pt in poly]
            _shade(img, (min(xs), min(ys), max(xs), max(ys)),
                   (72, 79, 96), (30, 34, 43), poly=poly)
        d.line([poly[0], poly[-1]], fill=(60, 67, 80, 255), width=2 * SS)
        d.line([poly[1], poly[2]], fill=(14, 17, 22, 255), width=2 * SS)

    # weicher Schatten, damit sich das Gehäuse vom Grund abhebt
    shadow = Image.new('RGBA', img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    box = (ox + 4 * SS, oy + 8 * SS, ox + body_w * SS + 4 * SS, oy + body_h * SS + 10 * SS)
    if round_display:
        sd.ellipse([int(v) for v in box], fill=(0, 0, 0, 170 if photo else 150))
    else:
        sd.rounded_rectangle([int(v) for v in box], radius=int(26 * SS),
                             fill=(0, 0, 0, 170 if photo else 150))
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur((11 if photo else 9) * SS)))

    # Tasten: eine links (Back), drei rechts (Up, Select, Down)
    btn = (96, 102, 115, 255)
    bh, bw = 22 * SS, 8 * SS
    cy = oy + body_h / 2 * SS
    rr((ox - bw + 2 * SS, cy - bh / 2, ox + 10 * SS, cy + bh / 2), 4 * SS, fill=btn)
    for dy in (-58, 0, 58):
        y = cy + dy * SS
        rr((ox + (body_w - 10) * SS, y - bh / 2, ox + body_w * SS + bw - 2 * SS, y + bh / 2),
           4 * SS, fill=btn)

    # Gehäuse: dünne helle Kante, dunkler Korpus, vertieftes Display
    outer = (ox, oy, ox + body_w * SS, oy + body_h * SS)
    inner = (ox + 2 * SS, oy + 2 * SS, ox + (body_w - 2) * SS, oy + (body_h - 2) * SS)
    recess = (ox + (bx - 4) * SS, oy + (bt - 5) * SS,
              ox + (bx + sw + 4) * SS, oy + (bt + sh + 5) * SS)
    if round_display:
        d.ellipse([int(v) for v in outer], fill=(120, 128, 142, 255))
        d.ellipse([int(v) for v in inner], fill=(26, 29, 36, 255))
        d.ellipse([int(v) for v in recess], fill=(8, 9, 12, 255))
    else:
        rr(outer, 26 * SS, fill=(120, 128, 142, 255))
        rr(inner, 24 * SS, fill=(26, 29, 36, 255))
        rr(recess, 8 * SS, fill=(8, 9, 12, 255))
    if photo:                                    # Licht von oben links
        _shade(img, outer, (182, 190, 204), (56, 62, 74),
               radius=26 * SS, ellipse=round_display)
        _shade(img, inner, (50, 55, 67), (11, 13, 18),
               radius=24 * SS, ellipse=round_display)
        # Fase: heller Grat oben links, dunkel nach unten rechts
        _shade(img, inner, (158, 166, 180), (16, 18, 24), radius=24 * SS,
               ellipse=round_display, outline=3 * SS)
        rr(recess, 8 * SS, fill=(8, 9, 12, 255))
        # Glanz auf dem Gehäuse, damit die Kante nicht wie Papier wirkt
        gloss = Image.new('L', img.size, 0)
        gd = ImageDraw.Draw(gloss)
        gd.polygon([(ox, oy + body_h * 0.34 * SS), (ox + body_w * 0.42 * SS, oy),
                    (ox + body_w * 0.66 * SS, oy), (ox, oy + body_h * 0.60 * SS)], fill=46)
        shape = Image.new('L', img.size, 0)
        sh_d = ImageDraw.Draw(shape)
        if round_display:
            sh_d.ellipse([int(v) for v in outer], fill=255)
        else:
            sh_d.rounded_rectangle([int(v) for v in outer], radius=int(26 * SS), fill=255)
        gloss = gloss.filter(ImageFilter.GaussianBlur(7 * SS))
        img.paste((255, 255, 255), (0, 0), ImageChops.multiply(gloss, shape))

    disp = s.resize((sw * SS, sh * SS), Image.LANCZOS).convert('RGBA')
    if round_display:                            # rundes Display beschneiden
        mask = Image.new('L', disp.size, 0)
        ImageDraw.Draw(mask).ellipse((0, 0, disp.width, disp.height), fill=255)
        disp.putalpha(mask)
    img.alpha_composite(disp, (int(ox + bx * SS), int(oy + bt * SS)))
    if photo:
        _glass(img, (ox + bx * SS, oy + bt * SS,
                     ox + (bx + sw) * SS, oy + (bt + sh) * SS),
               6 * SS, ellipse=round_display)
    return img


def prepare_logo(path, width=185, split=0.42):
    """Firmenlogo für dunklen Grund: Weiß transparent, rechter Teil
    (Schriftzug) aufgehellt, das Bildzeichen links bleibt unverändert."""
    im = Image.open(path).convert('RGBA')
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
    cut = int(im.width * split)
    for y in range(im.height):
        for x in range(cut, im.width):
            r, g, b, a = px[x, y]
            if a > 0 and r < 90 and g < 90 and b < 90:
                px[x, y] = (225, 232, 240, a)
    return im


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--shot', default=rel('store/screenshots_emery/04_recording_streaming.png'),
                    help='Screenshot in nativer Auflösung')
    ap.add_argument('--out', default=rel('store/banner/banner_720x320_en.png'))
    ap.add_argument('--icon', default=rel('store/icon/icon_master_1024.png'),
                    help='App-Icon mit Alpha')
    ap.add_argument('--logo', default=rel('store/banner/logo.png'),
                    help='Firmenlogo, wird unten links gesetzt')
    ap.add_argument('--title', default='HELO Remote')
    ap.add_argument('--subtitle', default='for the AJA HELO')
    ap.add_argument('--line', action='append', default=None,
                    help='Slogan-Zeile; mehrfach angeben, letzte Zeile bekommt die Akzentfarbe')
    ap.add_argument('--bg', default='#161e2a')
    ap.add_argument('--accent', default='#ff5f5f')
    ap.add_argument('--title-size', type=int, default=38,
                    help='Titelgröße; der Titel darf die Uhr nicht berühren')
    ap.add_argument('--tilt', type=float, default=9.0, help='Neigung der Uhr in Grad (0 = gerade)')
    ap.add_argument('--screen-w', type=int, default=150, help='Displaybreite in Bannereinheiten')
    ap.add_argument('--watch-x', type=int, default=605, help='Mitte der Uhr auf der x-Achse')
    ap.add_argument('--round', action='store_true', help='rundes Display (chalk, gabbro)')
    ap.add_argument('--flat', action='store_true',
                    help='ohne Fotolook: flache Flächen wie in der Skill-Vorlage')
    a = ap.parse_args()
    if a.line is None:
        a.line = ['Start and stop recording and',
                  'streaming, see the live status.',
                  '7 languages · Timeline pins']

    bg, accent = hex_rgb(a.bg), hex_rgb(a.accent)
    ban = Image.new('RGBA', (W * SS, H * SS), bg + (255,))
    d = ImageDraw.Draw(ban)

    if a.icon:
        ic = Image.open(a.icon).convert('RGBA').resize((110 * SS, 110 * SS), Image.LANCZOS)
        ban.alpha_composite(ic, (34 * SS, 34 * SS))
    x = 166 if a.icon else 40
    d.text((x * SS, (40 + (42 - a.title_size) // 2) * SS), a.title,
           font=font(a.title_size, True), fill='white')
    d.text(((x + 2) * SS, 94 * SS), a.subtitle, font=font(20), fill=accent)
    for i, line in enumerate(a.line):
        col = accent if i == len(a.line) - 1 and len(a.line) > 1 else (200, 208, 220)
        d.text(((x + 2) * SS, (128 + 22 * i + (4 if i == len(a.line) - 1 else 0)) * SS),
               line, font=font(15), fill=col)

    w = watch(a.shot, screen_w=a.screen_w, round_display=a.round, photo=not a.flat)
    if a.tilt:
        w = w.rotate(a.tilt, resample=Image.BICUBIC, expand=True)
    ban.alpha_composite(w, (a.watch_x * SS - w.width // 2, H * SS // 2 - w.height // 2))

    if a.logo:
        lg = prepare_logo(a.logo)
        ban.alpha_composite(lg, (30 * SS, H * SS - lg.height - 8 * SS))

    ban.resize((W, H), Image.LANCZOS).convert('RGB').save(a.out)
    print(os.path.basename(a.out), 'geschrieben')


if __name__ == '__main__':
    main()
