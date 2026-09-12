#!/usr/bin/env python3
"""
Baut den Explosions-Sample fuer einen Lautsprecher ohne Bass.

Eine Explosion lebt vom Tiefbass, den die Uhr nicht wiedergibt. Das Skript
erzeugt deshalb aus dem Bassanteil dessen Obertoene (Saettigung fuer die
ungeraden, Gleichrichtung fuer die geraden), begrenzt sie auf das Band, das
der Lautsprecher schafft, und mischt sie zurueck. Das Ohr ergaenzt daraus
den fehlenden Grundton. Dazu ein heller Knall-Transient und ein Hochpass,
der den unhoerbaren Rest wegnimmt.

    python3 tools/build_explosion.py explosion_16k_mono.wav out.wav
"""
import math
import random
import struct
import sys
import wave

SR = 16000

def rd(p):
    with wave.open(p) as w:
        n = w.getnframes()
        return [v/32768.0 for v in struct.unpack('<%dh' % n, w.readframes(n))]

def save(p, x, peak=0.93):
    s = peak / max(1e-9, max(abs(v) for v in x))
    d = [max(-32768, min(32767, int(v*s*32767))) for v in x]
    with wave.open(p, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(struct.pack('<%dh' % len(d), *d))

def biquad(sig, fc, kind='hp', q=0.707):
    w0 = 2*math.pi*fc/SR; a = math.sin(w0)/(2*q); c = math.cos(w0)
    if kind == 'hp':   b0, b1, b2 = (1+c)/2, -(1+c), (1+c)/2
    elif kind == 'lp': b0, b1, b2 = (1-c)/2, 1-c, (1-c)/2
    else:              b0, b1, b2 = a, 0.0, -a          # bandpass
    a0 = 1+a; a1 = -2*c; a2 = 1-a
    b0, b1, b2, a1, a2 = b0/a0, b1/a0, b2/a0, a1/a0, a2/a0
    x1=x2=y1=y2=0.0; out=[]
    for x in sig:
        y = b0*x+b1*x1+b2*x2-a1*y1-a2*y2
        x2,x1 = x1,x; y2,y1 = y1,y
        out.append(y)
    return out

def enhance_bass(x, drive=9.0, amount=1.0):
    """Psychoakustik: Oberton-Reihe aus dem Bass erzeugen, damit der winzige
    Lautsprecher den fehlenden Grundton wieder suggeriert."""
    low = biquad(biquad(x, 380, 'lp'), 380, 'lp')
    odd = [math.tanh(v*drive) for v in low]              # 3f, 5f ... Tonhoehe bleibt
    even = [abs(v)*2 - 0.5 for v in low]                 # 2f, 4f ... Oktave drueber
    mix = [0.7*odd[i] + 0.5*even[i] for i in range(len(x))]
    harm = biquad(biquad(mix, 520, 'hp'), 2600, 'lp')
    env = biquad([abs(v) for v in low], 25, 'lp')        # nur wo wirklich Bass war
    peak = max(env) + 1e-9
    return [harm[i] * (env[i]/peak) * amount for i in range(len(x))]

def crack(n, seed=5):
    """Heller Knall-Transient, den der Lautsprecher sicher wiedergibt."""
    rnd = random.Random(seed)
    nz = [rnd.uniform(-1, 1) for _ in range(n)]
    hi = biquad(nz, 1800, 'hp')
    out = []
    for i, v in enumerate(hi):
        out.append(v * math.exp(-i/(SR*0.012)) * min(1.0, i/(SR*0.0004)))
    return out

def compress(x, thr_db=-20.0, ratio=4.0, atk_ms=1.0, rel_ms=90.0, makeup_db=6.0):
    thr = 10**(thr_db/20); ka = math.exp(-1/(SR*atk_ms/1000)); kr = math.exp(-1/(SR*rel_ms/1000))
    env = 0.0; mk = 10**(makeup_db/20); out=[]
    for v in x:
        a = abs(v)
        env = a + (ka if a > env else kr)*(env-a)
        g = 1.0 if env <= thr else (thr/env)**(1-1/ratio)
        out.append(max(-0.99, min(0.99, v*g*mk)))
    return out


def main():
    in_path, out_path = sys.argv[1:3]
    src = rd(in_path)[:int(2.1 * SR)]
    n = len(src)
    harm = enhance_bass(src, drive=14.0, amount=1.15)
    out = [src[i] * 0.9 + harm[i] for i in range(n)]
    for i, v in enumerate(crack(int(0.25 * SR))):
        if i < n:
            out[i] += v * 0.55
    out = biquad(out, 300, 'hp')          # unhoerbaren Tiefbass weglassen
    save(out_path, compress(out, -24, 5, 0.5, 80, 9))
    print("wrote %s (%.1f s)" % (out_path, n / SR))


if __name__ == "__main__":
    main()
