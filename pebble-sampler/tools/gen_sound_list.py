#!/usr/bin/env python3
"""
Generates src/pkjs/sounds.json: the ordered list of sound names exactly as
the watch app builds its SOUNDS[] table (samples.inc first, then the
synthesized / note based entries in sounds.c). The phone settings page uses
it for the "which sounds to show" checkboxes, so the order must match the
watch. Called automatically from the wscript on every build.
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def sound_names():
    names = []
    inc = open(os.path.join(ROOT, "src", "c", "samples.inc"), encoding="utf-8").read()
    for line in inc.splitlines():
        if line.lstrip().startswith("//"):
            continue
        m = re.match(r'\s*SAMPLE\("([^"]+)"', line)
        if m:
            names.append(m.group(1))
    src = open(os.path.join(ROOT, "src", "c", "sounds.c"), encoding="utf-8").read()
    bank = src[src.index("const Sound SOUNDS[] = {"):]
    for line in bank.splitlines():
        m = re.match(r'\s*(?:SYNTH|NOTES|TRACKS)\s*\("([^"]+)"', line)
        if m:
            names.append(m.group(1))
    return names


def main():
    names = sound_names()
    out = os.path.join(ROOT, "src", "pkjs", "sounds.json")
    new = json.dumps(names, ensure_ascii=False, indent=2) + "\n"
    old = open(out, encoding="utf-8").read() if os.path.exists(out) else None
    if new != old:
        with open(out, "w", encoding="utf-8") as f:
            f.write(new)
    if "--print" in sys.argv:
        print("\n".join("%2d %s" % (i, n) for i, n in enumerate(names)))
    return names


if __name__ == "__main__":
    main()
