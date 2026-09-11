#!/usr/bin/env python3
"""
Import an audio clip (Suno export, Freesound download, own recording, ...)
into the Pebble sampler app.

    python3 tools/import_sample.py clip.mp3 --name "Applaus" --hint "Echte Menge"

The clip is converted to mono 16 kHz audio, de-noised, trimmed, normalized
and stored as IMA ADPCM (4 bit/sample = 8 KB per second, decoded to 16-bit
on the watch) in resources/samples/<slug>.ima. package.json and
src/c/samples.inc are updated so the sound shows up in the app after the
next `pebble build`.

ffmpeg does the decoding, resampling and de-noising and should be
installed. Without it only WAV input works, with a cruder resampler.
"""
import argparse
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PACKAGE = os.path.join(ROOT, "package.json")
SAMPLES_INC = os.path.join(ROOT, "src", "c", "samples.inc")
SAMPLES_DIR = os.path.join(ROOT, "resources", "samples")
RESOURCE_BUDGET = 256 * 1024

COLORS = ["GColorOrangeARGB8", "GColorVividCeruleanARGB8", "GColorFollyARGB8",
          "GColorJaegerGreenARGB8", "GColorPurpleARGB8", "GColorChromeYellowARGB8",
          "GColorCobaltBlueARGB8", "GColorSunsetOrangeARGB8", "GColorMagentaARGB8",
          "GColorIslamicGreenARGB8"]


def slugify(name):
    s = name.lower()
    for a, b in (("ä", "ae"), ("ö", "oe"), ("ü", "ue"), ("ß", "ss")):
        s = s.replace(a, b)
    s = re.sub(r"[^a-z0-9]+", "_", s).strip("_")
    return s or "sample"


def decode_to_wav(path, rate, start, max_seconds, highpass_hz, denoise_db):
    """Returns (wav path, temp path or None, True if ffmpeg did the processing)."""
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        if not path.lower().endswith(".wav"):
            sys.exit("ffmpeg not found: install it or hand me a WAV file.")
        return path, None, False
    filters = []
    if highpass_hz > 0:
        filters.append("highpass=f=%g:poles=2" % highpass_hz)
    if denoise_db > 0:
        # FFT de-noiser with automatic noise floor tracking, then a gentle gate
        filters.append("afftdn=nr=%g:nf=-45:tn=1" % denoise_db)
    filters.append("aresample=resampler=soxr:precision=28" if _has_soxr(ffmpeg) else "aresample")
    tmp = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
    tmp.close()
    cmd = [ffmpeg, "-y", "-loglevel", "error", "-ss", "%g" % start, "-t", "%g" % (max_seconds + 0.5),
           "-i", path, "-ac", "1", "-ar", str(rate), "-af", ",".join(filters), "-sample_fmt", "s16", tmp.name]
    subprocess.run(cmd, check=True)
    return tmp.name, tmp.name, True


def _has_soxr(ffmpeg):
    out = subprocess.run([ffmpeg, "-hide_banner", "-buildconf"], capture_output=True, text=True).stdout
    return "--enable-libsoxr" in out


def read_wav(path):
    """Returns (samples as floats in -1..1, sample rate)."""
    with wave.open(path, "rb") as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 1:
        data = [(b - 128) / 128.0 for b in raw]
    elif width == 2:
        data = [v / 32768.0 for v in struct.unpack("<%dh" % (len(raw) // 2), raw)]
    elif width == 3:
        data = [int.from_bytes(raw[i:i + 3], "little", signed=True) / 8388608.0 for i in range(0, len(raw), 3)]
    elif width == 4:
        data = [v / 2147483648.0 for v in struct.unpack("<%di" % (len(raw) // 4), raw)]
    else:
        sys.exit("unsupported WAV sample width %d" % width)
    if ch > 1:  # mix down
        data = [sum(data[i:i + ch]) / ch for i in range(0, len(data), ch)]
    return data, rate


def resample(data, src_rate, dst_rate):
    if src_rate == dst_rate:
        return data
    # crude anti-aliasing: box average when downsampling a lot
    ratio = src_rate / dst_rate
    if ratio > 1.5:
        k = int(ratio)
        data = [sum(data[i:i + k]) / k for i in range(0, len(data) - k + 1, k)]
        src_rate = src_rate / k
        ratio = src_rate / dst_rate
    out = []
    n = int(len(data) / ratio)
    for i in range(n):
        pos = i * ratio
        j = int(pos)
        frac = pos - j
        a = data[j]
        b = data[j + 1] if j + 1 < len(data) else a
        out.append(a + (b - a) * frac)
    return out


def trim(data, rate, threshold_db, pad_ms):
    thr = 10 ** (threshold_db / 20.0)
    start, end = 0, len(data)
    while start < end and abs(data[start]) < thr:
        start += 1
    while end > start and abs(data[end - 1]) < thr:
        end -= 1
    pad = int(rate * pad_ms / 1000)
    return data[max(0, start - pad):min(len(data), end + pad)]


def fade(data, rate, ms):
    n = min(int(rate * ms / 1000), len(data) // 2)
    for i in range(n):
        g = i / n
        data[i] *= g
        data[-1 - i] *= g
    return data


def normalize(data, peak_db):
    peak = max((abs(v) for v in data), default=0)
    if peak == 0:
        return data
    g = (10 ** (peak_db / 20.0)) / peak
    return [max(-1.0, min(1.0, v * g)) for v in data]


def highpass(data, rate, cutoff_hz):
    """One-pole high-pass: the watch speaker cannot reproduce the lows anyway."""
    if cutoff_hz <= 0:
        return data
    import math
    rc = 1.0 / (2 * math.pi * cutoff_hz)
    dt = 1.0 / rate
    a = rc / (rc + dt)
    out = [0.0] * len(data)
    prev_x = prev_y = 0.0
    for i, x in enumerate(data):
        y = a * (prev_y + x - prev_x)
        out[i] = y
        prev_x, prev_y = x, y
    return out


def encode(data, bits):
    if bits == 8:
        return bytes((max(-128, min(127, int(round(v * 127)))) & 0xFF) for v in data)
    return b"".join(struct.pack("<h", max(-32768, min(32767, int(round(v * 32767))))) for v in data)


IMA_INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]
IMA_STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
    2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767]


def encode_ima_adpcm(data):
    """Continuous IMA ADPCM stream (no block headers), matching player.c."""
    pred, index = 0, 0
    out = bytearray()
    nibbles = []
    for v in data:
        sample = max(-32768, min(32767, int(round(v * 32767))))
        step = IMA_STEP_TABLE[index]
        diff = sample - pred
        nib = 0
        if diff < 0:
            nib = 8
            diff = -diff
        delta = step >> 3
        if diff >= step:
            nib |= 4; diff -= step; delta += step
        if diff >= step >> 1:
            nib |= 2; diff -= step >> 1; delta += step >> 1
        if diff >= step >> 2:
            nib |= 1; delta += step >> 2
        pred += -delta if nib & 8 else delta
        pred = max(-32768, min(32767, pred))
        index = max(0, min(88, index + IMA_INDEX_TABLE[nib]))
        nibbles.append(nib)
    if len(nibbles) % 2:
        nibbles.append(0)
    for i in range(0, len(nibbles), 2):
        out.append(nibbles[i] | (nibbles[i + 1] << 4))
    return bytes(out)


def update_package(resource_name, rel_path):
    with open(PACKAGE) as f:
        pkg = json.load(f)
    media = pkg["pebble"]["resources"].setdefault("media", [])
    entry = {"type": "raw", "name": resource_name, "file": rel_path}
    for m in list(media):  # a re-import may change the extension: drop stale files
        if m.get("name") == resource_name and m.get("file") != rel_path:
            stale = os.path.join(ROOT, "resources", m["file"])
            if os.path.exists(stale):
                os.unlink(stale)
            media.remove(m)
    for i, m in enumerate(media):
        if m.get("name") == resource_name:
            media[i] = entry
            break
    else:
        media.append(entry)
    with open(PACKAGE, "w") as f:
        json.dump(pkg, f, indent=2)
        f.write("\n")


def update_samples_inc(resource_name, line):
    existing = open(SAMPLES_INC).read() if os.path.exists(SAMPLES_INC) else ""
    lines = [l for l in existing.splitlines() if ("RESOURCE_ID_" + resource_name) not in l]
    lines.append(line)
    with open(SAMPLES_INC, "w") as f:
        f.write("\n".join(lines) + "\n")


def resource_usage():
    total = 0
    with open(PACKAGE) as f:
        pkg = json.load(f)
    for m in pkg["pebble"]["resources"].get("media", []):
        p = os.path.join(ROOT, "resources", m["file"])
        if os.path.exists(p):
            total += os.path.getsize(p)
    return total


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", help="audio file (WAV directly, anything else via ffmpeg)")
    ap.add_argument("--name", required=True, help="menu title, e.g. 'Applaus'")
    ap.add_argument("--hint", default="Sample", help="menu subtitle")
    ap.add_argument("--color", default=None, help="GColor...ARGB8 constant for the menu accent")
    ap.add_argument("--rate", type=int, choices=[8000, 16000], default=16000)
    ap.add_argument("--codec", choices=["adpcm", "pcm8", "pcm16"], default="adpcm",
                    help="adpcm: 4 bit/sample, good quality (default); pcm8/pcm16: raw")
    ap.add_argument("--denoise", type=float, default=8.0, help="ffmpeg afftdn noise reduction in dB (0 = off)")
    ap.add_argument("--max-seconds", type=float, default=4.0, help="hard cut after this many seconds")
    ap.add_argument("--start", type=float, default=0.0, help="skip this many seconds at the start")
    ap.add_argument("--trim-db", type=float, default=-45.0, help="silence threshold for trimming")
    ap.add_argument("--peak-db", type=float, default=-1.0, help="normalize to this peak level")
    ap.add_argument("--highpass", type=float, default=150.0, help="high-pass cutoff in Hz (0 = off)")
    ap.add_argument("--fade-ms", type=float, default=8.0)
    ap.add_argument("--replace", action="store_true", help="overwrite an existing sample of the same name")
    args = ap.parse_args()

    wav_path, tmp, processed = decode_to_wav(args.input, args.rate, args.start, args.max_seconds,
                                             args.highpass, args.denoise)
    try:
        data, rate = read_wav(wav_path)
    finally:
        if tmp:
            os.unlink(tmp)

    if not processed:
        data = data[int(args.start * rate):]
    data = trim(data, rate, args.trim_db, 20)
    data = data[:int(args.max_seconds * rate)]
    if not processed:
        data = resample(data, rate, args.rate)
        data = highpass(data, args.rate, args.highpass)
    data = normalize(data, args.peak_db)
    data = fade(data, args.rate, args.fade_ms)
    if args.codec == "adpcm":
        pcm = encode_ima_adpcm(data)
    else:
        pcm = encode(data, 8 if args.codec == "pcm8" else 16)

    slug = slugify(args.name)
    resource_name = "SAMPLE_" + slug.upper()
    ext = ".ima" if args.codec == "adpcm" else ".pcm"
    rel_path = "samples/%s%s" % (slug, ext)
    out_path = os.path.join(SAMPLES_DIR, slug + ext)
    if os.path.exists(out_path) and not args.replace:
        sys.exit("%s exists already, use --replace to overwrite" % out_path)
    os.makedirs(SAMPLES_DIR, exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(pcm)

    bits = 8 if args.codec == "pcm8" else 16
    fmt = "SpeakerPcmFormat_%dkHz_%dbit" % (args.rate // 1000, bits)
    codec = "SampleCodecImaAdpcm" if args.codec == "adpcm" else "SampleCodecRaw"
    color = args.color or COLORS[sum(map(ord, slug)) % len(COLORS)]
    update_package(resource_name, rel_path)
    update_samples_inc(resource_name, 'SAMPLE("%s", "%s", %s, RESOURCE_ID_%s, %s, %s)' % (
        args.name.replace('"', ""), args.hint.replace('"', ""), color, resource_name, fmt, codec))

    used = resource_usage()
    print("wrote %s: %.2f s, %d bytes (%s, %s)" % (out_path, len(data) / args.rate, len(pcm), fmt, args.codec))
    print("resource budget: %d / %d bytes used (%.0f%%)" % (used, RESOURCE_BUDGET, 100.0 * used / RESOURCE_BUDGET))
    if used > RESOURCE_BUDGET:
        print("WARNING: over the 256 KB app resource limit, the build will fail. Shorten or remove samples.")
    print("now run: pebble build")


if __name__ == "__main__":
    main()
