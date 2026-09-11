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

# Banner 720x320: Icon, Titel, Text, Screenshot; Pac-Man als leises Wasserzeichen unten rechts
ban=Image.new('RGBA',(720,320),WHITE); d=ImageDraw.Draw(ban)
logo=Image.open(f'{base}/side_effects_logo.png').convert('RGBA')
# nur der Pac-Man (linker Teil des Logos), auf 15 % Deckkraft
pac=logo.crop((0,0,int(logo.width*0.40),logo.height))
pac=pac.resize((150,int(pac.height*150/pac.width)),Image.LANCZOS)
a=pac.getchannel('A').point(lambda v: int(v*0.15)); pac.putalpha(a)
ban.alpha_composite(pac,(16, 320-pac.height-4))          # unten links, hinter dem Namenszug
ic=icon(170); ban.alpha_composite(ic,(24,12))
try:
    f1=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',46)
    f2=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',20)
    f3=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',16)
    f4=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',12)
except Exception:
    f1=f2=f3=f4=ImageFont.load_default()
d.text((200,52),"Theremin",font=f1,fill=BLACK)
d.text((203,112),"für die Pebble Time 2",font=f2,fill=GREY)
d.text((203,158),"Hand heben: Tonhöhe",font=f3,fill=GREY)
d.text((203,182),"Handgelenk drehen: Lautstärke",font=f3,fill=GREY)
d.text((203,206),"Vier Wellenformen, Tonleitern,",font=f3,fill=GREY)
d.text((203,230),"Einstellungen auch am Handy",font=f3,fill=GREY)
d.text((26,298),"SIDE effect's",font=f4,fill=(150,150,150))
shot=Image.open(os.path.join(base,'..','screenshots','06_saegezahn.png')).convert('RGB')
frame=Image.new('RGB',(shot.width+12,shot.height+12),BLACK); frame.paste(shot,(6,6))
ban.paste(frame,(720-frame.width-18,(320-frame.height)//2))
ban.convert('RGB').save(f'{base}/banner_720x320.png')
print("icons ok")
