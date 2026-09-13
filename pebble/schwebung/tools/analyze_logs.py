#!/usr/bin/env python3
"""Wertet die [E1]-Zeilen aus `pebble logs` aus."""
import re
import statistics
import sys
from collections import defaultdict

SUMMARY = re.compile(
    r"\[E1\] (?P<mode>\w+) fps=(?P<fps>[\d.]+) rend=(?P<rend>[\d.]+)ms q=(?P<q>\d+)ms "
    r"ur=(?P<ur>\d+) sw=(?P<sw>\d+) touch=(?P<touch>\d+)/s jit=(?P<jit>[\d.]+)px")
LRA = re.compile(r"\[E1\]\[LRA\] t=(?P<t>\d+) enqueue mode=\w+ period=(?P<p>\d+) ms")
PROBE = re.compile(r"\[E1\]\[PROBE\] (?P<txt>.*)")
JUMP = re.compile(r"\[E1\]\[LATENZ\] (?P<txt>.*)")
PANEL = re.compile(r"\[E1\]\[PANEL\] (?P<txt>.*)")
LICHT = re.compile(r"\[E1\]\[LICHT\] (?P<txt>.*)")
GAME = re.compile(r"\[E1\]\[GAME\] (?P<txt>.*)")


def main(path):
    per_mode = defaultdict(lambda: defaultdict(list))
    lra_times = []
    events = defaultdict(list)
    with open(path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            m = SUMMARY.search(line)
            if m:
                d = per_mode[m.group("mode")]
                for k in ("fps", "rend", "q", "ur", "sw", "touch", "jit"):
                    d[k].append(float(m.group(k)))
                continue
            m = LRA.search(line)
            if m:
                lra_times.append((int(m.group("t")), int(m.group("p"))))
                continue
            for name, rx in (("Probe", PROBE), ("Latenz", JUMP), ("Panel", PANEL),
                             ("Licht", LICHT), ("Spiel", GAME)):
                m = rx.search(line)
                if m:
                    events[name].append(m.group("txt").strip())
                    break

    print("== Sekundenlog je Modus ==")
    for mode, d in per_mode.items():
        n = len(d["fps"])
        print(f"{mode}: {n} s  fps {statistics.mean(d['fps']):.1f} (min {min(d['fps']):.1f})  "
              f"render {statistics.mean(d['rend']):.1f} ms (max {max(d['rend']):.1f})  "
              f"vorlauf {statistics.mean(d['q']):.0f} ms (min {min(d['q']):.0f})  "
              f"unterlaeufe {int(max(d['ur']))}  backpressure {int(max(d['sw']))}  "
              f"touch {max(d['touch']):.0f}/s  jitter {statistics.mean(d['jit']):.2f} px")

    if lra_times:
        print("\n== LRA-Aufrufe ==")
        by_period = defaultdict(list)
        for (t0, p0), (t1, p1) in zip(lra_times, lra_times[1:]):
            if p0 == p1:
                by_period[p0].append(t1 - t0)
        for p, gaps in sorted(by_period.items()):
            print(f"Periode {p} ms ({1000 / p:.1f} Hz): {len(gaps)} Muster, Abstand der Aufrufe "
                  f"{statistics.mean(gaps):.0f} ms (min {min(gaps)}, max {max(gaps)})")

    for name in ("Probe", "Latenz", "Panel", "Licht", "Spiel"):
        if events[name]:
            print(f"\n== {name} ==")
            for e in events[name][:40]:
                print("  " + e)
            if len(events[name]) > 40:
                print(f"  ... {len(events[name]) - 40} weitere")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Aufruf: analyze_logs.py <logdatei>")
        sys.exit(1)
    main(sys.argv[1])
