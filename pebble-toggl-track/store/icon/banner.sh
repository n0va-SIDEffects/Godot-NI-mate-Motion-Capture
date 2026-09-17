#!/bin/sh
# Store banner for Toggl Timer: the emery screenshot in the cut-out
# Pebble Time 2 from ./watch. Run from this folder; needs Pillow.
cd "$(dirname "$0")" && python3 make_banner.py \
  --shot ../screenshots_en/1_status.png \
  --icon icon_512_transparent.png \
  --logo side_effects_logo.png \
  --title "Toggl Timer" --subtitle "for Pebble Time 2" \
  --line "Start, stop and switch your" \
  --line "Toggl Track timers from the wrist." \
  --line "Favourites · Dictation · Reminders" \
  --accent "#e57cd8" --out banner_720x320.png
