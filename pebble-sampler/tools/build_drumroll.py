#!/usr/bin/env python3
"""
Baut den Trommelwirbel-Sample.

Der Wirbel wird synthetisiert statt aus Einzelanschlaegen zusammengesetzt:
gefiltertes Rauschen (Schnarrsaiten-Teppich) mit einem durchgehenden
Grundpegel plus Anschlags-Transienten obendrauf. Genau dieser Grundpegel
macht den Unterschied zwischen einem echten Wirbel und einem
Maschinengewehr-Effekt; ausserdem vermeidet er die Kammfilter-Faerbung,
die entsteht, wenn man dasselbe Snare-Sample dutzendfach ueberlagert.

    python3 tools/build_drumroll.py snare.wav crash.wav out.wav [crash_start_s]

Die beiden Eingangsdateien sind Mono 16 kHz 16 Bit (siehe ATTRIBUTION.md);
sie liefern nur das Finale: Beckenschlag plus betonter Schlussschlag.
"""
import math
import random
import struct
import sys
import wave

SR = 16000
ROLL = 2.2      # Sekunden Wirbel
TAIL = 1.3      # Sekunden fuer Becken-Ausklang


def load(path):
    with wave.open(path) as w:
        assert w.getframerate() == SR and w.getnchannels() == 1 and w.getsampwidth() == 2, \
            "erwarte Mono 16 kHz 16 Bit: %s" % path
        n = w.getnframes()
        return [v / 32768.0 for v in struct.unpack("<%dh" % n, w.readframes(n))]


def bandpass(sig, fc, q):
    """Biquad-Bandpass nach RBJ-Kochbuch, bis Nyquist stabil."""
    w0 = 2 * math.pi * fc / SR
    alpha = math.sin(w0) / (2 * q)
    a0 = 1 + alpha
    b0, b2 = alpha / a0, -alpha / a0
    a1, a2 = (-2 * math.cos(w0)) / a0, (1 - alpha) / a0
    x1 = x2 = y1 = y2 = 0.0
    out = []
    for x in sig:
        y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, x
        y2, y1 = y1, y
        out.append(y)
    return out


def build(snare, crash):
    rnd = random.Random(23)
    n = int((ROLL + TAIL) * SR)
    n_roll = int(ROLL * SR)

    noise = [rnd.uniform(-1, 1) for _ in range(n)]
    wire = [0.75 * a + 0.45 * b for a, b in
            zip(bandpass(noise, 2400, 0.9), bandpass(noise, 5200, 1.4))]

    # Anschlagszeiten: beschleunigend von 40 auf 26 ms, leicht vermenschlicht
    env = [0.0] * n
    t = 0.0
    while t < ROLL:
        pos = int(max(0.0, t + rnd.uniform(-0.0022, 0.0022)) * SR)
        amp = rnd.uniform(0.75, 1.0)
        dec = SR * rnd.uniform(0.013, 0.020)
        for i in range(int(0.06 * SR)):
            if pos + i >= n:
                break
            attack = min(1.0, i / (0.0006 * SR))
            env[pos + i] += amp * attack * math.exp(-i / dec)
        t += 0.040 - 0.014 * (t / ROLL)

    def crescendo(x):
        return 0.45 + 0.55 * (x ** 1.25)

    out = [0.0] * n
    phase = 0.0
    for i in range(n_roll):
        x = i / n_roll
        out[i] = wire[i] * (0.30 + 0.90 * min(1.6, env[i])) * crescendo(x)
        phase += 2 * math.pi * 195 / SR          # Kesselton, auf der Uhr kaum hoerbar
        out[i] += 0.30 * math.sin(phase) * min(1.0, env[i]) * crescendo(x)

    finale = int((ROLL + 0.02) * SR)
    for i, v in enumerate(snare):
        if finale + i < n:
            out[finale + i] += v * 0.95
    for i, v in enumerate(crash):
        if finale + i < n:
            out[finale + i] += v * 0.8
    return out


def main():
    snare_path, crash_path, out_path = sys.argv[1:4]
    crash_start = float(sys.argv[4]) if len(sys.argv) > 4 else 0.0
    snare = load(snare_path)
    crash = load(crash_path)[int(crash_start * SR):int((crash_start + 1.5) * SR)]
    out = build(snare, crash)
    scale = 0.92 / max(abs(v) for v in out)
    data = [max(-32768, min(32767, int(v * scale * 32767))) for v in out]
    with wave.open(out_path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(struct.pack("<%dh" % len(data), *data))
    print("wrote %s (%.1f s)" % (out_path, len(data) / SR))


if __name__ == "__main__":
    main()
