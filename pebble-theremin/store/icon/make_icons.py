# Erzeugt Icon (Konzept B: Antenne + Welle), Launcher-Icon und Banner.
# Aufruf aus dem Projektordner: python3 store/icon/make_icons.py  (braucht Pillow)
from PIL import Image, ImageDraw, ImageFont
import math, os
BLACK=(25,25,25); WHITE=(255,255,255); BLUE=(0,150,230); GREY=(90,90,90)
N=1024

def sine(d, x0, x1, cy, amp, cycles, color, width):
    pts=[(x, cy - amp*math.sin(2*math.pi*cycles*(x-x0)/(x1-x0))) for x in range(x0, x1+1, 4)]
    d.line(pts, fill=color, width=width, joint='curve')

def icon(size, launcher=False):
    im=Image.new('RGBA',(N,N),(0,0,0,0)); d=ImageDraw.Draw(im)
    if launcher:
        # Launcher (25 px): dickere Striche, eine Wellenperiode, damit es lesbar bleibt
        sine(d, 60, 964, 560, 150, 1.0, BLUE, 110)
        lw=120
        d.rounded_rectangle([512-lw//2,150,512+lw//2,900], radius=lw//2, fill=BLACK)
        d.ellipse([512-120,60,512+120,300], fill=BLACK)
    else:
        sine(d, 80, 944, 560, 120, 1.5, BLUE, 40)
        lw=56
        d.rounded_rectangle([512-lw//2,150,512+lw//2,880], radius=lw//2, fill=BLACK)
        d.ellipse([512-58,90,512+58,206], fill=BLACK)
        d.rounded_rectangle([420,840,604,900], radius=24, fill=BLACK)
    return im.resize((size,size), Image.LANCZOS)

base=os.path.dirname(os.path.abspath(__file__))
for s in (512,144,96,48):
    ic=icon(s); ic.save(f'{base}/icon_{s}_transparent.png')
    bg=Image.new('RGBA',(s,s),WHITE); bg.alpha_composite(ic); bg.convert('RGB').save(f'{base}/icon_{s}.png')
icon(25, launcher=True).save(os.path.join(base,'..','..','resources','images','menu_icon.png'))

# Banner 720x320 (Richtung "Dunkel mit Welle", englischer Text):
# dunkler Grund, grosse blaue Welle, Icon und Titel in Weiss, Screenshot rechts,
# SIDE effect's Logo klein unten links.
W,H=720,320; SS=3
DARK=(22,30,42); SUB=(170,190,210)
def wave_poly(d, y, amp, cycles, color, width, x0=0, x1=W, s=1):
    top=[(x*s, (y-amp*math.sin(2*math.pi*cycles*(x-x0)/(x1-x0))-width/2)*s) for x in range(x0,x1+1,2)]
    bot=[(x*s, (y-amp*math.sin(2*math.pi*cycles*(x-x0)/(x1-x0))+width/2)*s) for x in range(x1,x0-1,-2)]
    d.polygon(top+bot, fill=color)
def icon_colored(size, color, wave):
    im=Image.new('RGBA',(N,N),(0,0,0,0)); d=ImageDraw.Draw(im)
    wave_poly(d, 560, 120, 1.5, wave, 40, x0=80, x1=944)
    lw=56
    d.rounded_rectangle([512-lw//2,150,512+lw//2,880], radius=lw//2, fill=color)
    d.ellipse([512-58,90,512+58,206], fill=color)
    d.rounded_rectangle([420,840,604,900], radius=24, fill=color)
    return im.resize((size,size), Image.LANCZOS)
big=Image.new('RGBA',(W*SS,H*SS),DARK); d=ImageDraw.Draw(big)
wave_poly(d, 224, 40, 2.2, (45,75,105), 22, s=SS)   # tiefer, damit der Text frei bleibt
wave_poly(d, 224, 40, 2.2, BLUE, 7, s=SS)
ban=big.resize((W,H),Image.LANCZOS); d=ImageDraw.Draw(ban)
ban.alpha_composite(icon_colored(120, WHITE, BLUE),(30,26))
try:
    f1=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',52)
    f2=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',20)
    f3=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',15)
except Exception:
    f1=f2=f3=ImageFont.load_default()
d.text((160,48),"Theremin",font=f1,fill=WHITE)
d.text((164,112),"for the Pebble Time 2",font=f2,fill=SUB)
d.text((164,140),"Raise your hand for pitch",font=f3,fill=SUB)
d.text((164,160),"Twist your wrist for volume",font=f3,fill=SUB)
shot=Image.open(os.path.join(base,'..','screenshots','en_sawtooth.png')).convert('RGB')
frame=Image.new('RGB',(shot.width+10,shot.height+10),(60,70,85)); frame.paste(shot,(5,5))
ban.paste(frame,(W-frame.width-26,(H-frame.height)//2))
# Logo: weissen Hintergrund entfernen, klein unten links
logo=Image.open(f'{base}/side_effects_logo.png').convert('RGBA'); px=logo.load()
for yy in range(logo.height):
    for xx in range(logo.width):
        r,g,b,a=px[xx,yy]
        if r>235 and g>235 and b>235: px[xx,yy]=(r,g,b,0)
bbox=logo.getbbox(); logo=logo.crop(bbox)
lw_=150; logo=logo.resize((lw_, int(logo.height*lw_/logo.width)), Image.LANCZOS)
# Pulslinie und Schriftzug (rechter Teil) auf dem dunklen Grund aufhellen;
# der Pac-Man samt schwarzem X bleibt unveraendert
px=logo.load(); split=int(logo.width*0.42)
for yy in range(logo.height):
    for xx in range(split, logo.width):
        r,g,b,a=px[xx,yy]
        if a>0 and r<90 and g<90 and b<90: px[xx,yy]=(225,232,240,a)
ban.alpha_composite(logo,(30, H-logo.height-8))
ban.convert('RGB').save(f'{base}/banner_720x320.png')
print("icons ok")
