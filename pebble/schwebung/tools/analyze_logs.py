#!/usr/bin/env python3
"""Wertet die [E1]-Zeilen aus `pebble logs` aus.

Das Sekundenlog besteht aus drei Zeilen (APP_LOG schneidet bei ~87 Zeichen ab):
  [E1]  Modus, fps, Renderzeit, Vorlauf, Vorlauf-Tiefstand, groesste Tick-Luecke, Eichungen, Unterlaeufe
  [E1t] Touch-Rate, Jitter, Ruheabweichung, Dead-Zone-Ausreisser, min. Intervall, LRA- und Backlight-Aufrufe, Stream-Staus
  [E1+] Gabel, Blume, Schwebung, Spielzustand, Uhr-Resyncs/lange Pausen, Heap, Uhr-Glitches
"""
import re
import statistics
import sys
from collections import defaultdict

SUMMARY = re.compile(
    r"\[E1\] (?P<mode>\w+) fps=(?P<fps>[\d.]+) rend=(?P<rend>[\d.]+)ms q=(?P<q>\d+)ms "
    r"lo=(?P<lo>\d+)ms gap=(?P<gap>\d+)ms cal=(?P<cal>\d+) ur=(?P<ur>\d+)")
TOUCH = re.compile(
    r"\[E1t\] touch=(?P<touch>\d+)/s jit=(?P<jit>[\d.]+)px still=(?P<still>\d+)px "
    r"dz=(?P<dz>\d+) ivl=(?P<ivl>\d+)ms lra=(?P<lra>\d+) bl=(?P<bl>\d+)(?: stau=(?P<stau>\d+))?")
EXTRA = re.compile(r"\[E1\+\] fork=(?P<fork>[\d.]+) fl=(?P<fl>[\d.]+) beat=(?P<beat>[\d.]+) "
                   r"st=(?P<st>\w+) rs=(?P<rs>\d+)/(?P<gaps>\d+) heap=(?P<heap>\d+) gl=(?P<gl>\d+)")
LRA = re.compile(r"\[E1\]\[LRA\] t=(?P<t>\d+) pulse mode=\w+ period=(?P<p>\d+) ms len=(?P<len>\d+) ms")
PROBE = re.compile(r"\[E1\]\[PROBE\] (?P<txt>.*)")
JUMP = re.compile(r"\[E1\]\[LATENZ\] (?P<txt>.*)")
PANEL = re.compile(r"\[E1\]\[PANEL\] (?P<txt>.*)")
LICHT = re.compile(r"\[E1\]\[LICHT\] (?P<txt>.*)")
GAME = re.compile(r"\[E1\]\[GAME\] (?P<txt>.*)")
TON = re.compile(r"\[E1\]\[TON\] (?P<txt>.*)")
AUDIO = re.compile(r"\[E1\]\[AUDIO\] (?P<txt>.*)")
FAULT = re.compile(r"fault|assert|crash", re.IGNORECASE)


def main(path):
    per_mode = defaultdict(lambda: defaultdict(list))
    lra_times = []
    events = defaultdict(list)
    faults = []
    mode = None
    with open(path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.rstrip("\n")
            m = SUMMARY.search(line)
            if m:
                mode = m.group("mode")
                d = per_mode[mode]
                for k in ("fps", "rend", "q", "lo", "gap", "cal", "ur"):
                    d[k].append(float(m.group(k)))
                continue
            m = TOUCH.search(line)
            if m and mode:
                d = per_mode[mode]
                for k in ("touch", "jit", "still", "dz", "ivl", "lra", "bl", "stau"):
                    if m.group(k) is not None:
                        d[k].append(float(m.group(k)))
                continue
            m = EXTRA.search(line)
            if m and mode:
                for k in ("rs", "gaps", "heap", "gl"):
                    per_mode[mode][k].append(float(m.group(k)))
                continue
            m = LRA.search(line)
            if m:
                lra_times.append((int(m.group("t")), int(m.group("p"))))
                continue
            if FAULT.search(line) and "[E1]" not in line:
                faults.append(line.strip())
            for name, rx in (("Audio", AUDIO), ("Ton", TON), ("Probe", PROBE), ("Latenz", JUMP),
                             ("Panel", PANEL), ("Licht", LICHT), ("Spiel", GAME)):
                m = rx.search(line)
                if m:
                    events[name].append(m.group("txt").strip())
                    break

    print("== Sekundenlog je Modus ==")
    for mode, d in per_mode.items():
        n = len(d["fps"])
        out = (f"{mode}: {n} s  fps {statistics.mean(d['fps']):.1f} (min {min(d['fps']):.1f})  "
               f"render {statistics.mean(d['rend']):.1f} ms (max {max(d['rend']):.1f})  "
               f"vorlauf {statistics.mean(d['q']):.0f} ms (tiefstand {min(d['lo']):.0f} ms)  "
               f"groesste tick-luecke {max(d['gap']):.0f} ms  eichungen {int(max(d['cal']))}  "
               f"unterlaeufe {int(max(d['ur']))}")
        if d["touch"]:
            ivl = [x for x in d["ivl"] if x > 0]
            out += (f"\n    touch {max(d['touch']):.0f}/s  jitter {statistics.mean(d['jit']):.2f} px  "
                    f"ruhe max {max(d['still']):.0f} px  dz-ausreisser {int(max(d['dz']))}  "
                    f"min-intervall {min(ivl) if ivl else 0:.0f} ms  "
                    f"lra-aufrufe {int(max(d['lra']))}  backlight-aufrufe {int(max(d['bl']))}"
                    + (f"  stream-staus {int(max(d['stau']))}" if d["stau"] else ""))
        if d["rs"]:
            out += (f"\n    heap min {min(d['heap']):.0f} B  uhr-glitches {int(max(d['gl']))}  "
                    f"uhr-resyncs {int(max(d['rs']))}  lange pausen {int(max(d['gaps']))}")
        print(out)

    if lra_times:
        print("\n== LRA-Einzelpulse ==")
        by_period = defaultdict(list)
        for (t0, p0), (t1, p1) in zip(lra_times, lra_times[1:]):
            if p0 == p1:
                by_period[p0].append(t1 - t0)
        for p, gaps in sorted(by_period.items()):
            print(f"Periode {p} ms ({1000 / p:.1f} Hz): {len(gaps)} Pulse, Abstand der Pulse "
                  f"{statistics.mean(gaps):.0f} ms (min {min(gaps)}, max {max(gaps)}, "
                  f"Streuung {statistics.pstdev(gaps):.1f} ms)")

    for name in ("Audio", "Ton", "Probe", "Latenz", "Panel", "Licht", "Spiel"):
        if events[name]:
            print(f"\n== {name} ==")
            for e in events[name][:40]:
                print("  " + e)
            if len(events[name]) > 40:
                print(f"  ... {len(events[name]) - 40} weitere")

    if faults:
        print("\n== ACHTUNG: Absturz-/Fehlerzeilen ==")
        for line in faults[:20]:
            print("  " + line)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Aufruf: analyze_logs.py <logdatei>")
        sys.exit(1)
    try:
        main(sys.argv[1])
    except BrokenPipeError:
        pass   # z. B. beim Weiterleiten an head
