# Pebble platforms

| Platform | Watch | Display | Colour | Shape | Heap | Notes |
|----------|-------|---------|--------|-------|------|-------|
| aplite   | Pebble, Pebble Steel | 144×168 | 1-bit B/W | rect | 24 KB | no mic, no glances, many APIs stubbed |
| basalt   | Pebble Time, Time Steel | 144×168 | 64 colours | rect | 64 KB | |
| chalk    | Pebble Time Round | 180×180 | 64 colours | round | 64 KB | content must respect the circle |
| diorite  | Pebble 2 | 144×168 | 1-bit B/W | rect | 64 KB | heart rate |
| emery    | **Pebble Time 2 / Core Time 2** | **200×228** | 64 colours (e-paper) | rect | 128 KB | mic, heart rate, compass; primary target |
| flint    | Pebble 2 Duo / Core 2 Duo | 144×168 | 1-bit B/W | rect | 64 KB | new SDK only |
| gabbro   | Time 2 Round | round | 64 colours | round | 128 KB | new SDK only; layout from bounds |

`targetPlatforms` in package.json selects the builds; the default template
lists all seven. Old SDK cores (≤ 4.3) do not know flint/gabbro.

## Compile-time switches

```c
PBL_PLATFORM_APLITE / _BASALT / _CHALK / _DIORITE / _EMERY / _FLINT / _GABBRO
PBL_COLOR   PBL_BW          // colour depth
PBL_RECT    PBL_ROUND       // shape
PBL_MICROPHONE PBL_HEALTH PBL_COMPASS PBL_SMARTSTRAP
PBL_DISPLAY_WIDTH PBL_DISPLAY_HEIGHT   // integers, usable in #if
PBL_IF_COLOR_ELSE(a, b)  PBL_IF_BW_ELSE(a, b)
PBL_IF_RECT_ELSE(a, b)   PBL_IF_ROUND_ELSE(a, b)
PBL_IF_MICROPHONE_ELSE(a, b)  PBL_IF_HEALTH_ELSE(a, b)
```

Prefer `#if PBL_DISPLAY_HEIGHT >= 200` for "large screen" decisions over
naming platforms — it also covers future devices.

## Per-platform constants

| Constant | aplite | basalt | chalk | diorite | emery |
|----------|--------|--------|-------|---------|-------|
| `ACTION_BAR_WIDTH` | 30 | 30 | 40 | 30 | 34 |
| `STATUS_BAR_LAYER_HEIGHT` | 16 | 16 | 24 | 16 | 20 |
| `MENU_CELL_BASIC_HEADER_HEIGHT` | 16 | 16 | 16 | 16 | 16 |

Use the macros, not the numbers.

## Buttons

BACK (left), UP / SELECT / DOWN (right). Long press BACK exits the app in the
launcher; inside a window BACK pops the window unless you override it. The
Time 2 also has a touch layer, but button-first design still works everywhere.

## Colours

64 colours on colour models: `GColor8` is `0b11RRGGBB` (2 bits per channel,
top bits = opaque). Convert an RGB hex on the phone with
`0xC0 | (round(r/85) << 4) | (round(g/85) << 2) | round(b/85)`.
Named colours: `GColorBlack`, `GColorWhite`, `GColorRed`, `GColorFolly`,
`GColorDarkGray`, `GColorLightGray`, `GColorVividCerulean`, ... (full list in
`gcolor_definitions.h`). On B/W platforms every colour collapses to black or
white via `PBL_IF_COLOR_ELSE`. `gcolor_legible_over(bg)` picks black or white
text for a background (not on aplite).

The Time 2 e-paper display is reflective: prefer high-contrast palettes,
white background with black/dark text, and colour as accent (badges, bars).

## System fonts (`fonts_get_system_font(FONT_KEY_...)`)

Text: `GOTHIC_09`, `GOTHIC_14`, `GOTHIC_14_BOLD`, `GOTHIC_18`, `GOTHIC_18_BOLD`,
`GOTHIC_24`, `GOTHIC_24_BOLD`, `GOTHIC_28`, `GOTHIC_28_BOLD`,
`BITHAM_30_BLACK`, `BITHAM_42_BOLD`, `BITHAM_42_LIGHT`, `BITHAM_18_LIGHT_SUBSET`,
`BITHAM_34_LIGHT_SUBSET`, `ROBOTO_CONDENSED_21`, `ROBOTO_BOLD_SUBSET_49`,
`DROID_SERIF_28_BOLD`.

Numbers only (digits, colon, some punctuation): `LECO_20_BOLD_NUMBERS`,
`LECO_26_BOLD_NUMBERS_AM_PM`, `LECO_28_LIGHT_NUMBERS`, `LECO_32_BOLD_NUMBERS`,
`LECO_36_BOLD_NUMBERS`, `LECO_38_BOLD_NUMBERS`, `LECO_42_NUMBERS`,
`BITHAM_34_MEDIUM_NUMBERS`, `BITHAM_42_MEDIUM_NUMBERS`.

Gothic fonts cover Latin-1 (ä ö ü ß) — German text renders fine. Rough
widths: `LECO_36` digit ≈ 20 px, `LECO_26` digit ≈ 15 px, Gothic 24 bold ≈
12 px per character. A 200 px wide emery screen minus a 34 px action bar
leaves 166 px: `H:MM:SS` in LECO_36 fits, `HH:MM:SS` barely.

Custom fonts: add a `.ttf` under `resources/` with `"type": "font"` and a
name like `FONT_MY_20` (size suffix required); load with
`fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MY_20))`.

## Layout recipe

```c
GRect b = layer_get_bounds(layer);
int content_w = b.size.w - ACTION_BAR_WIDTH;            // if an action bar is used
int pad = PBL_IF_ROUND_ELSE(30, 8);                      // keep text inside the circle
#if PBL_DISPLAY_HEIGHT >= 200
  // bigger fonts / more rows for emery
#else
  // compact layout
#endif
```

Test both `emery` and `basalt` in the emulator; if text clips on chalk,
increase the round padding rather than shrinking fonts everywhere.
