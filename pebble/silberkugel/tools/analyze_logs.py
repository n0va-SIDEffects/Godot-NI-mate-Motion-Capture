#!/usr/bin/env python3
"""Wertet die [P1]-Zeilen aus `pebble logs` aus.

Das Sekundenlog besteht aus mehreren kurzen Zeilen, weil APP_LOG bei rund
87 Zeichen abschneidet:
  [P1]  Bildschirm, fps, Zeichenzeit, groesste Tick-Luecke, Spieltick, Tonvorlauf, Unterlaeufe
  [P1p] Substeps, Kontakte nach Art, Teilschritte, Ausbrueche, Hoechsttempo, Kugelposition
  [P1t] Touch-Rate, Ereignisabstand, verlorene Finger, Magnetzeit, Verdeckungszeit
  [P1h] LRA: abgesetzt, verworfen, nachgeholt, Geiger-Ticks, kuerzester Abstand
  [P1a] Beschleunigung: Samples, Stoesse, Klapse, Masken, Spitze, Neigung, Tilt-Bob
  [P1+] Baelle, Abfluesse, Skill-Shots, Griffe, Wuerfe, freier Heap, Stream-Staus
  [P1b] Rohevents je Taste (beantwortet, ob Back als Flipperknopf taugt)

Aufruf:  python3 tools/analyze_logs.py e1.log
"""
import re
import statistics
import sys
from collections import defaultdict

SUMMARY = re.compile(
    r"\[P1\] (?P<mode>\w+) fps=(?P<fps>[\d.]+) rend=(?P<rend>[\d.]+)ms gap=(?P<gap>\d+)ms "
    r"spiel=(?P<spiel>\d+)ms q=(?P<q>\d+)ms ur=(?P<ur>\d+)")
PHYS = re.compile(
    r"\[P1p\] sub=(?P<sub>\d+) seg=(?P<seg>\d+) krs=(?P<krs>\d+) flp=(?P<flp>\d+) "
    r"split=(?P<split>\d+) esc=(?P<esc>\d+) vmax=(?P<vmax>-?\d+) "
    r"ball=(?P<bx>-?\d+),(?P<by>-?\d+) v=(?P<vx>-?\d+),(?P<vy>-?\d+)")
TOUCH = re.compile(
    r"\[P1t\] touch=(?P<touch>\d+)/s ivl=(?P<ivmin>\d+)\.\.(?P<ivmax>\d+)ms stale=(?P<stale>\d+) "
    r"mag=(?P<mag>\d+)ms verdeckt=(?P<occ>\d+)ms finger=(?P<finger>\d+)ms")
HAP = re.compile(
    r"\[P1h\] lra=(?P<lra>\d+) drop=(?P<drop>\d+) queue=(?P<queue>\d+) geiger=(?P<geiger>\d+) "
    r"mingap=(?P<mingap>\d+)ms wait=(?P<wait>\d+)ms")
ACC = re.compile(
    r"\[P1a\] acc=(?P<acc>\d+) nudge=(?P<nudge>\d+) tap=(?P<tap>\d+) "
    r"mask=(?P<mv>\d+)/(?P<mf>\d+)/(?P<mb>\d+) peak=(?P<peak>-?\d+)mg "
    r"lean=(?P<lx>-?\d+)/(?P<ly>-?\d+) bob=(?P<bob>-?\d+)")
EXTRA = re.compile(
    r"\[P1\+\] baelle=(?P<balls>\d+) ab=(?P<drains>\d+) skill=(?P<skill>\d+) "
    r"griff=(?P<grab>\d+) wurf=(?P<throw>\d+) heap=(?P<heap>\d+) stau=(?P<stall>\d+)")
BTN = re.compile(r"\[P1b\] back_rep=(?P<rep>\d+) raw: back=(?P<back>\d+)/\d+ "
                 r"down=(?P<down>\d+)/\d+ up=(?P<up>\d+)/\d+ select=(?P<sel>\d+)/\d+")
BALL = re.compile(r"\[P1\]\[BALL\] Abfluss nach (?P<ms>\d+) ms")
PLUNGER = re.compile(r"\[P1\]\[PLUNGER\] (?P<txt>.*)")
PANEL = re.compile(r"\[P1\]\[PANEL\] (?P<txt>.*)")
BENCH = re.compile(r"\[P1\]\[BENCH\] (?P<txt>.*)")
DRILL = re.compile(r"\[P1\]\[DRILL\] (?P<txt>.*)")
WURF = re.compile(r"\[P1\]\[WURF\] (?P<txt>.*)")
AUDIO = re.compile(r"\[P1\]\[AUDIO\] (?P<txt>.*)")
FAULT = re.compile(r"fault|assert|crash|Heap Usage for App", re.IGNORECASE)


def stat(vals, fmt="{:.1f}"):
    if not vals:
        return "-"
    lo, hi = min(vals), max(vals)
    mid = statistics.median(vals)
    return (fmt + " bis " + fmt + ", Mitte " + fmt).format(lo, hi, mid)


def main(path):
    per_mode = defaultdict(lambda: defaultdict(list))
    events = defaultdict(list)
    ball_ms = []
    last = {}
    faults = []
    mode = None

    with open(path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.rstrip("\n")
            m = SUMMARY.search(line)
            if m:
                new_mode = m.group("mode")
                if new_mode != mode:
                    # Bildschirmwechsel: Der Substep-Zaehler laeuft weiter,
                    # aber dazwischen liegt die Zeit im anderen Bildschirm.
                    # Ohne diese Truennung ergaebe die Differenz einen
                    # Phantomwert von mehreren tausend Substeps je Sekunde.
                    per_mode[new_mode]["sub"].append(None)
                mode = new_mode
                d = per_mode[mode]
                d["fps"].append(float(m.group("fps")))
                d["rend"].append(float(m.group("rend")))
                d["gap"].append(int(m.group("gap")))
                d["spiel"].append(int(m.group("spiel")))
                d["q"].append(int(m.group("q")))
                d["ur"].append(int(m.group("ur")))
                continue
            m = PHYS.search(line)
            if m and mode:
                d = per_mode[mode]
                d["sub"].append(int(m.group("sub")))
                d["split"].append(int(m.group("split")))
                d["esc"].append(int(m.group("esc")))
                d["vmax"].append(int(m.group("vmax")))
                continue
            for rx, key in ((TOUCH, "touch"), (HAP, "hap"), (ACC, "acc"),
                            (EXTRA, "extra"), (BTN, "btn")):
                m = rx.search(line)
                if m:
                    last[key] = m.groupdict()
                    break
            else:
                m = BALL.search(line)
                if m:
                    ball_ms.append(int(m.group("ms")))
                    continue
                for rx, key in ((PLUNGER, "Plunger"), (PANEL, "Panel"), (BENCH, "Physik-Test"),
                                (DRILL, "Magnet-Uebung"), (WURF, "Notwurf"), (AUDIO, "Ton")):
                    m = rx.search(line)
                    if m:
                        events[key].append(m.group("txt"))
                        break
                else:
                    if FAULT.search(line):
                        faults.append(line.strip())

    print("SILBERKUGEL, Phase 1: Auswertung von", path)
    print()
    for mode, d in per_mode.items():
        print("Bildschirm", mode)
        print("  Bildrate       ", stat(d["fps"]), "fps")
        print("  Zeichnen       ", stat(d["rend"]), "ms")
        print("  Tick-Luecke    ", stat(d["gap"], "{:.0f}"), "ms (Soll 20)")
        print("  Spieltick      ", stat(d["spiel"], "{:.0f}"), "ms")
        print("  Tonvorlauf     ", stat(d["q"], "{:.0f}"), "ms")
        if d["ur"]:
            print("  Unterlaeufe    ", max(d["ur"]))
        subs = [v for v in d["sub"] if v is not None]
        if subs:
            spans = [b - a for a, b in zip(d["sub"], d["sub"][1:])
                     if a is not None and b is not None and b >= a]
            if spans:
                print("  Substeps/s     ", stat(spans, "{:.0f}"), "(Soll 200)")
            print("  Teilschritte   ", max(d["split"]), "hoechstens")
            print("  Ausbrueche     ", max(d["esc"]), "(muss 0 sein)")
            print("  Hoechsttempo   ", max(d["vmax"]), "px/s")
        print()

    if ball_ms:
        print("Baelle: %d Abfluesse, Lebensdauer %s ms" % (len(ball_ms), stat(ball_ms, "{:.0f}")))
        print()

    if "btn" in last:
        b = last["btn"]
        print("Tasten (Rohevents): back %s, down %s, up %s, select %s; Back-Klicks %s"
              % (b["back"], b["down"], b["up"], b["sel"], b["rep"]))
        if b["back"] == "0" and b["rep"] != "0":
            print("  Back liefert keine Rohevents, nur Klicks: kein Halten des linken Flippers.")
        print()

    if "hap" in last:
        h = last["hap"]
        print("LRA: %s abgesetzt, %s verworfen, %s nachgeholt, %s Geiger-Ticks, "
              "kuerzester Abstand %s ms, laengste Wartezeit %s ms"
              % (h["lra"], h["drop"], h["queue"], h["geiger"], h["mingap"], h["wait"]))
        print()

    if "touch" in last:
        t = last["touch"]
        occ, mag, finger = int(t["occ"]), int(t["mag"]), int(t["finger"])
        print("Touch: %s Ereignisse/s, Abstand %s bis %s ms, %s verlorene Finger"
              % (t["touch"], t["ivmin"], t["ivmax"], t["stale"]))
        print("Magnet: %d ms aktiv, Kugel %d ms unter der Fingerkuppe, Finger %d ms auf dem Glas"
              % (mag, occ, finger))
        if finger:
            print("  Verdeckungsanteil der Fingerzeit: %d Prozent" % (occ * 100 // finger))
        print()

    if "acc" in last:
        a = last["acc"]
        print("Beschleunigung: %s Samples, %s Stoesse, %s Klapse, Spitze %s mg, "
              "Neigung %s/%s mg, Tilt-Bob %s" % (a["acc"], a["nudge"], a["tap"], a["peak"],
                                                 a["lx"], a["ly"], a["bob"]))
        print("  Maskiert: %s durch eigene Vibration, %s durch das Firmware-Flag, "
              "%s durch Tastendruck" % (a["mv"], a["mf"], a["mb"]))
        print()

    if "extra" in last:
        e = last["extra"]
        print("Spiel: %s Baelle, %s Abfluesse, %s Skill-Shots, %s Griffe, %s Wuerfe"
              % (e["balls"], e["drains"], e["skill"], e["grab"], e["throw"]))
        print("Heap frei: %s B, Stream-Staus: %s" % (e["heap"], e["stall"]))
        print()

    for key, lines in events.items():
        print(key + ":")
        for t in lines[-6:]:
            print("   ", t)
        print()

    if faults:
        print("Auffaellig:")
        for t in faults[-8:]:
            print("   ", t)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)
    main(sys.argv[1])
