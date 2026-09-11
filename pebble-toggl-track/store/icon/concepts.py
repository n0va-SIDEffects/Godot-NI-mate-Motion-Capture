#!/usr/bin/env python3
"""Four icon directions for review. Writes concept_<x>_1024.png and a sheet."""
import os, math
from PIL import Image, ImageDraw, ImageFont
HERE = os.path.dirname(os.path.abspath(__file__))
PINK = (229, 124, 216); CHAR = (36, 32, 44); WHITE = (255, 255, 255)

def canvas(size, bg, radius=0.22):
    ss = 4; S = size * ss
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, S - 1, S - 1), radius=S * radius, fill=bg)
    return im, d, S

def finish(im, size):
    return im.resize((size, size), Image.LANCZOS)

# A: bold play in a thick ring
def concept_a(size, simple=False):
    im, d, S = canvas(size, PINK); cx = cy = S / 2
    r = S * 0.34; w = S * (0.10 if simple else 0.075)
    d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=WHITE, width=int(w))
    t = S * (0.16 if simple else 0.14)
    d.polygon([(cx - t * 0.75, cy - t), (cx - t * 0.75, cy + t), (cx + t * 1.05, cy)], fill=WHITE)
    return finish(im, size)

# B: solid stopwatch silhouette, pink face with a bold hand
def concept_b(size, simple=False):
    im, d, S = canvas(size, PINK); cx, cy = S / 2, S * 0.56
    r = S * 0.33
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=WHITE)
    d.rounded_rectangle((cx - S * 0.08, S * 0.10, cx + S * 0.08, S * 0.24), radius=S * 0.03, fill=WHITE)
    d.rectangle((cx - S * 0.04, S * 0.2, cx + S * 0.04, S * 0.3), fill=WHITE)
    ri = S * (0.24 if simple else 0.26)
    d.ellipse((cx - ri, cy - ri, cx + ri, cy + ri), fill=PINK)
    # elapsed wedge (about 100 minutes on a 60 dial: quarter past)
    d.pieslice((cx - ri, cy - ri, cx + ri, cy + ri), start=-90, end=0, fill=WHITE)
    if not simple:
        d.ellipse((cx - S * 0.03, cy - S * 0.03, cx + S * 0.03, cy + S * 0.03), fill=PINK)
    return finish(im, size)

# C: charcoal tile, pink disc, white play
def concept_c(size, simple=False):
    im, d, S = canvas(size, CHAR); cx = cy = S / 2
    r = S * 0.36
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=PINK)
    t = S * (0.17 if simple else 0.15)
    d.polygon([(cx - t * 0.7, cy - t), (cx - t * 0.7, cy + t), (cx + t * 1.1, cy)], fill=WHITE)
    return finish(im, size)

# D: watch outline (case + strap stubs) with a play triangle inside
def concept_d(size, simple=False):
    im, d, S = canvas(size, PINK); cx = cy = S / 2
    w = S * (0.09 if simple else 0.07)
    case = S * 0.30
    d.rounded_rectangle((cx - case, cy - case, cx + case, cy + case), radius=S * 0.09, outline=WHITE, width=int(w))
    strap = S * 0.14
    d.rounded_rectangle((cx - strap, S * 0.06, cx + strap, cy - case + w * 0.3), radius=S * 0.03, fill=WHITE)
    d.rounded_rectangle((cx - strap, cy + case - w * 0.3, cx + strap, S * 0.94), radius=S * 0.03, fill=WHITE)
    t = S * (0.13 if simple else 0.11)
    d.polygon([(cx - t * 0.7, cy - t), (cx - t * 0.7, cy + t), (cx + t * 1.1, cy)], fill=WHITE)
    return finish(im, size)

CONCEPTS = [('A', 'Play im Ring', concept_a), ('B', 'Stoppuhr solid', concept_b), ('C', 'Dunkel + Disc', concept_c), ('D', 'Uhr mit Play', concept_d)]

if __name__ == '__main__':
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 22)
    sheet = Image.new('RGB', (4 * 260 + 20, 330), (60, 60, 60)); sd = ImageDraw.Draw(sheet)
    for i, (key, label, fn) in enumerate(CONCEPTS):
        big = fn(1024); big.save(os.path.join(HERE, f'concept_{key}_1024.png'))
        x = 20 + i * 260
        sd.text((x, 12), f'{key}: {label}', font=font, fill=WHITE)
        ic = big.resize((144, 144), Image.LANCZOS); bg = Image.new('RGBA', ic.size, (255, 255, 255, 255)); bg.alpha_composite(ic)
        sheet.paste(bg.convert('RGB'), (x, 48))
        small = fn(200, simple=True).resize((25, 25), Image.LANCZOS)
        # show the 25px launcher icon at 1:1 and 4x, on a white launcher-like row
        row = Image.new('RGBA', (240, 120), (255, 255, 255, 255)); row.alpha_composite(small, (8, 8))
        row.alpha_composite(small.resize((100, 100), Image.NEAREST), (60, 10))
        sheet.paste(row.convert('RGB'), (x, 200))
        sd.text((x + 170, 240), '25 px', font=font, fill=(180, 180, 180))
    out = os.path.join(HERE, '..', '..', '..', 'concepts_sheet.png')
    sheet.save('/tmp/claude-0/-home-user-Godot-NI-mate-Motion-Capture/4c676543-0b2b-563a-b43a-1cae7de694f1/scratchpad/icon-concepts.png')
    print('sheet written')
