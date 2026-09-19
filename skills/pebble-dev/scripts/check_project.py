#!/usr/bin/env python3
"""Prueft ein Pebble-Projekt auf die Fehler, die sonst erst spaet auffallen.

    python3 check_project.py [projektverzeichnis]

Liest package.json, die Quellen und - falls vorhanden - den letzten Build.
Meldet FEHLER (bricht etwas) und WARNUNG (faellt spaeter auf die Fuesse).
Rueckgabewert 1, wenn mindestens ein FEHLER gefunden wurde.
"""

import json
import os
import re
import sys

PLATTFORMEN = {"aplite", "basalt", "chalk", "diorite", "flint", "emery", "gabbro"}

# Fähigkeit -> (Plattformen, die sie haben, Funktionsnamen, die sie brauchen)
HARDWARE = {
    "Lautsprecher": ({"flint", "emery"},
                     ("speaker_stream_open", "speaker_play_tone", "speaker_play_notes")),
    "Mikrofon/Diktat": ({"basalt", "chalk", "diorite", "flint", "emery", "gabbro"},
                        ("dictation_session_create", "dictation_session_start")),
    "Touch": ({"emery", "gabbro"}, ("touch_handler", "TouchHandler")),
    "Herzfrequenz": ({"diorite", "emery"},
                     ("HealthMetricHeartRateBPM", "health_service_peek_current_value")),
}

# create -> destroy
PAARE = {
    "window_create": "window_destroy",
    "layer_create": "layer_destroy",
    "text_layer_create": "text_layer_destroy",
    "bitmap_layer_create": "bitmap_layer_destroy",
    "menu_layer_create": "menu_layer_destroy",
    "scroll_layer_create": "scroll_layer_destroy",
    "action_bar_layer_create": "action_bar_layer_destroy",
    "status_bar_layer_create": "status_bar_layer_destroy",
    "gbitmap_create_with_resource": "gbitmap_destroy",
    "gbitmap_create_blank": "gbitmap_destroy",
    "fonts_load_custom_font": "fonts_unload_custom_font",
    "gdraw_command_image_create_with_resource": "gdraw_command_image_destroy",
    "gdraw_command_sequence_create_with_resource": "gdraw_command_sequence_destroy",
    "gbitmap_sequence_create_with_resource": "gbitmap_sequence_destroy",
}

fehler, warnungen = [], []


def f(msg):
    fehler.append(msg)


def w(msg):
    warnungen.append(msg)


def lies_quellen(root):
    text, dateien = "", []
    for unter in ("src", "worker_src"):
        pfad = os.path.join(root, unter)
        for basis, _, namen in os.walk(pfad):
            for n in namen:
                if n.endswith((".c", ".h")):
                    p = os.path.join(basis, n)
                    dateien.append(p)
                    with open(p, encoding="utf-8", errors="replace") as fh:
                        text += fh.read() + "\n"
    return text, dateien


def pruefe_metadaten(pkg):
    p = pkg.get("pebble", {})

    if not p:
        f('package.json enthaelt keinen "pebble"-Block.')
        return set()

    if not p.get("uuid"):
        f("pebble.uuid fehlt.")
    elif not re.fullmatch(r"[0-9a-fA-F-]{36}", p["uuid"]):
        f("pebble.uuid ist keine gueltige UUID: %r" % p["uuid"])

    if not p.get("displayName"):
        w("pebble.displayName fehlt - im Launcher steht dann der npm-Name.")

    version = str(pkg.get("version", ""))
    if not re.fullmatch(r"\d+\.\d+(\.\d+)?", version):
        f("version %r hat kein gueltiges Format." % version)
    elif version.count(".") == 2 and not version.endswith(".0"):
        w("version %r: der Appstore erwartet Major.Minor - siehe Skill "
          "pebble-publish." % version)

    ziele = set(p.get("targetPlatforms") or PLATTFORMEN)
    unbekannt = ziele - PLATTFORMEN
    if unbekannt:
        f("Unbekannte Plattform(en) in targetPlatforms: %s" % ", ".join(sorted(unbekannt)))
    if not p.get("targetPlatforms"):
        w("targetPlatforms fehlt - es wird fuer alle sieben Plattformen gebaut, "
          "auch fuer ungetestete.")

    medien = (p.get("resources") or {}).get("media") or []
    if len(medien) > 256:
        f("resources.media hat %d Eintraege, erlaubt sind 256." % len(medien))
    menu_icons = [m for m in medien if m.get("menuIcon")]
    if len(menu_icons) > 1:
        f("Mehr als ein Medieneintrag mit menuIcon: true.")
    elif not menu_icons and not (p.get("watchapp") or {}).get("watchface"):
        w("Kein menuIcon gesetzt - der Launcher zeigt ein generisches Symbol "
          "(PNG 25x25).")

    for m in medien:
        datei = m.get("file")
        if datei and not os.path.exists(os.path.join(ROOT, "resources", datei)):
            f("Ressource fehlt auf der Platte: resources/%s" % datei)

    wa = p.get("watchapp") or {}
    if wa.get("hiddenApp") and wa.get("onlyShownOnCommunication"):
        w("hiddenApp und onlyShownOnCommunication schliessen sich aus - "
          "hiddenApp gewinnt.")

    return ziele


def pruefe_hardware(quellen, ziele):
    for name, (kann, symbole) in HARDWARE.items():
        if any(s in quellen for s in symbole):
            ohne = ziele - kann
            if ohne:
                f("%s wird benutzt, aber %s kann das nicht. Das Linken bricht "
                  "dort ab." % (name, ", ".join(sorted(ohne))))


def pruefe_schluessel(pkg, quellen):
    deklariert = pkg.get("pebble", {}).get("messageKeys") or []
    if isinstance(deklariert, dict):
        deklariert = list(deklariert)
    deklariert = set(deklariert)
    benutzt = set(re.findall(r"MESSAGE_KEY_([A-Za-z0-9_]+)", quellen))
    fehlend = benutzt - deklariert
    if fehlend:
        f("MESSAGE_KEY_* im Code, aber nicht in messageKeys: %s"
          % ", ".join(sorted(fehlend)))
    unbenutzt = deklariert - benutzt - {"dummy"}
    if unbenutzt:
        w("In messageKeys deklariert, im C-Code nicht benutzt: %s"
          % ", ".join(sorted(unbenutzt)))

    if "app_message_open" in quellen:
        for cb in ("app_message_register_inbox_dropped",
                   "app_message_register_outbox_failed"):
            if cb not in quellen:
                w("%s fehlt - Fehler bleiben unsichtbar." % cb)
    if deklariert - {"dummy"} and "configurable" not in (
            pkg.get("pebble", {}).get("capabilities") or []):
        w('Einstellungen per messageKeys, aber capabilities enthaelt nicht '
          '"configurable" - die Handy-App zeigt dann kein Zahnrad.')


def pruefe_lebenszyklus(quellen):
    for create, destroy in PAARE.items():
        n_c = len(re.findall(r"\b%s\s*\(" % create, quellen))
        n_d = len(re.findall(r"\b%s\s*\(" % destroy, quellen))
        if n_c and not n_d:
            f("%s wird benutzt, %s kommt nirgends vor - das leckt bei jedem "
              "Oeffnen." % (create, destroy))
        elif n_c > n_d:
            w("%s: %dx erzeugt, nur %dx zerstoert." % (create, n_c, n_d))


def pruefe_stil(dateien):
    for pfad in dateien:
        with open(pfad, encoding="utf-8", errors="replace") as fh:
            for nr, zeile in enumerate(fh, 1):
                ort = "%s:%d" % (os.path.relpath(pfad, ROOT), nr)
                if re.search(r"\bpsleep\s*\(", zeile):
                    f("%s: psleep() blockiert die ganze App." % ort)
                m = re.search(r"^\s*(?:char|uint8_t|int8_t)\s+\w+\s*\[\s*(\d{4,})\s*\]",
                              zeile)
                if m and "static" not in zeile:
                    w("%s: Puffer mit %s Byte auf dem Stack - statisch anlegen "
                      "oder mallocen." % (ort, m.group(1)))
                if re.search(r"GRect\s*\(\s*\d+\s*,\s*\d+\s*,\s*(1[0-9]{2}|2[0-9]{2})\s*,",
                             zeile):
                    w("%s: feste Pixelmasse - aus layer_get_unobstructed_bounds() "
                      "rechnen." % ort)


def pruefe_build(root, ziele):
    build = os.path.join(root, "build")
    if not os.path.isdir(build):
        return
    for plattform in sorted(ziele):
        elf = os.path.join(build, plattform, "pebble-app.elf")
        if os.path.exists(elf):
            continue
        w("Kein Build fuer %s vorhanden (build/%s/pebble-app.elf fehlt)."
          % (plattform, plattform))


def main():
    global ROOT
    ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
    pfad = os.path.join(ROOT, "package.json")
    if not os.path.exists(pfad):
        print("Keine package.json in %s - ist das ein Pebble-Projekt?" % ROOT)
        return 1

    with open(pfad, encoding="utf-8") as fh:
        try:
            pkg = json.load(fh)
        except json.JSONDecodeError as exc:
            print("package.json ist kein gueltiges JSON: %s" % exc)
            return 1

    quellen, dateien = lies_quellen(ROOT)
    ziele = pruefe_metadaten(pkg)
    if dateien:
        pruefe_hardware(quellen, ziele)
        pruefe_schluessel(pkg, quellen)
        pruefe_lebenszyklus(quellen)
        pruefe_stil(dateien)
    pruefe_build(ROOT, ziele)

    print("Geprueft: %s (%d Quelldatei(en), Ziele: %s)"
          % (ROOT, len(dateien), ", ".join(sorted(ziele)) or "-"))
    for m in fehler:
        print("  FEHLER   %s" % m)
    for m in warnungen:
        print("  WARNUNG  %s" % m)
    if not fehler and not warnungen:
        print("  Nichts zu beanstanden.")
    return 1 if fehler else 0


if __name__ == "__main__":
    sys.exit(main())
