#!/usr/bin/env python3
"""
Builds the drum roll sample from a single real snare hit and a crash
cymbal: accelerating, with a crescendo, slightly humanized, ending in a
crash plus an accented hit. Produces a 16 kHz mono WAV for import_sample.py.

    python3 tools/build_drumroll.py snare.wav crash.wav out.wav [crash_start_s]
"""
import math
import random
import struct
import sys
import wave

SR = 16000


def load(path):
    with wave.open(path) as w:
        assert w.getframerate() == SR and w.getnchannels() == 1 and w.getsampwidth() == 2, "need 16 kHz mono 16-bit"
        n = w.getnframes()
        return list(struct.unpack("<%dh" % n, w.readframes(n)))


def main():
    snare_path, crash_path, out_path = sys.argv[1:4]
    crash_start = float(sys.argv[4]) if len(sys.argv) > 4 else 0.0
    random.seed(7)
    snare = load(snare_path)
    crash = load(crash_path)[int(crash_start * SR):int((crash_start + 1.5) * SR)]
    crisp = [v * math.exp(-i / (SR * 0.026)) for i, v in enumerate(snare[:int(0.14 * SR)])]
    roll_len = 2.6
    out = [0.0] * int((roll_len + 1.9) * SR)
    t = 0.0
    while t < roll_len:
        x = t / roll_len
        gain = (0.55 + 0.45 * (x ** 1.3)) * random.uniform(0.88, 1.0)
        pos = int((t + random.uniform(-0.002, 0.002)) * SR)
        for i, v in enumerate(crisp):
            if 0 <= pos + i < len(out):
                out[pos + i] += v * gain
        t += 0.055 - 0.022 * x
    end = int((roll_len + 0.02) * SR)
    for i, v in enumerate(snare):
        if end + i < len(out):
            out[end + i] += v * 1.0
    for i, v in enumerate(crash):
        if end + i < len(out):
            out[end + i] += v * 0.8
    scale = 30000 / max(abs(v) for v in out)
    data = [max(-32768, min(32767, int(v * scale))) for v in out]
    with wave.open(out_path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(struct.pack("<%dh" % len(data), *data))
    print("wrote %s (%.1f s)" % (out_path, len(data) / SR))


if __name__ == "__main__":
    main()
