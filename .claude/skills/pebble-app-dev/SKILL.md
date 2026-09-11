---
name: pebble-app-dev
description: Build, debug and ship apps and watchfaces for Pebble smartwatches, especially the Pebble Time 2 (Core Time 2, platform "emery"), Pebble 2 Duo ("flint") and the classic Pebble Time / Pebble 2 models, using the Core Devices Pebble SDK (pebble-tool, C + PebbleKit JS). Use this skill whenever the user mentions Pebble, PebbleOS, Core Devices, pebble-tool, a .pbw, a watchapp or watchface, AppMessage, PebbleKit JS, an emulator platform name (aplite, basalt, chalk, diorite, emery, flint, gabbro), or wants any code that runs on the watch or talks to it from the phone — even if they only say "meine Uhr" or "die Pebble" in passing. Also use it when reviewing or fixing existing Pebble C/JS code.
---

# Pebble app development (Pebble Time 2 first)

A Pebble app has two halves that ship in one `.pbw`:

- **Watch side** — C, compiled per platform with the Pebble SDK. Draws the UI,
  handles buttons, persists small state. No network, no allocation beyond a
  small heap (128 KB on emery, 64 KB on basalt/chalk/diorite, 24 KB on aplite).
- **Phone side** — PebbleKit JS (`src/pkjs/index.js`), ES5 JavaScript run by the
  Pebble phone app. This is where HTTP, JSON, OAuth tokens and the settings page
  live. Watch and phone talk through **AppMessage** dictionaries.

Decide early which half owns what: anything that needs the internet or more
than a few KB of data belongs in JS; the watch only renders and reacts.

## Workflow

1. **Scaffold** — `pebble new-project --javascript <name>` when the SDK is
   installed, otherwise copy `assets/template/` (same layout, already set up
   for AppMessage + a settings page). Set `targetPlatforms`; keep at least
   `emery` for the Time 2. Register every AppMessage key under `messageKeys`
   — the build turns them into `MESSAGE_KEY_<Name>` in C and
   `require('message_keys')` in JS.
2. **Design for the display first.** Read `references/platforms.md` for sizes,
   colours and per-platform constants. Write layout from `layer_get_bounds()`
   and `PBL_DISPLAY_WIDTH/HEIGHT`, never from hard-coded 144×168, so the same
   code looks right on emery (200×228) and round chalk (180×180).
3. **Write the C** using the patterns in `references/c-api.md` (window
   lifecycle, layers, fonts, colours, MenuLayer, AppMessage, persist, timers).
4. **Write the JS** using `references/pebblekit-js.md` (message queue, XHR,
   settings page without hosting, caching).
5. **Verify before claiming success.**
   - With the SDK: `pebble build < /dev/null`, then run in the emulator and
     take a screenshot (`references/cli-and-emulator.md`). Look at the
     screenshot; fix what looks wrong. Test buttons with `pebble emu-button`.
   - Without the SDK (CI, sandbox, blocked download): run
     `scripts/check_compile.sh <project-dir>` — it compiles every C file per
     platform against the SDK headers with the SDK's warning flags, so real
     API mistakes surface. Syntax-check JS with `node --check` and, for
     anything with logic, write a Node smoke test on top of
     `assets/pkjs-mock.js` (fake `Pebble`, `localStorage`, `XMLHttpRequest`).
   - Say plainly which of these ran. A compile check is not a device test.
6. **Install** — `pebble install --phone <ip>` (Developer Connection enabled
   in the phone app) or `--emulator emery`. Watch logs with `pebble logs`.

## Things that bite (learned the hard way)

- `pebble` commands can block on an interactive first-run prompt: append
  `< /dev/null`. In headless environments add `--vnc` to every emulator command.
- The SDK compiles with `-Wall -Wextra -Werror`. Unused variables are OK,
  everything else fails the build. `PBL_API_EXISTS(...)` inside `#if` trips
  `-Wexpansion-to-defined`; use `#ifndef PBL_PLATFORM_APLITE` style guards.
- `app_message_outbox_send()` takes **no** argument; the iterator from
  `app_message_outbox_begin(&iter)` is implicit.
- App glances, `graphics_draw_arc`, `gcolor_legible_over`, dictation and
  health APIs are missing or macro-stubbed on **aplite**. Guard with
  `#ifndef PBL_PLATFORM_APLITE` / `#ifdef PBL_COLOR` / `PBL_IF_*_ELSE`.
- `time(NULL)` is UTC; use `localtime()` for display. `time_t` is 32-bit.
- `snprintf` on the watch has no `%f` and no `%lld`; cast to `int` and use `%d`.
  `-Werror=format-truncation` fires when a `%d` could overflow the buffer:
  clamp the value (e.g. minutes ≤ 99) or size the buffer for the worst case.
- Apps are killed when the user presses BACK. A countdown or alarm that must
  fire anyway needs `wakeup_schedule()` plus a persisted end timestamp and a
  `launch_reason() == APP_LAUNCH_WAKEUP` branch (see `references/c-api.md`).
- String buffers are fixed-size bytes. Cut strings on the phone on UTF-8
  boundaries before sending (umlauts are 2 bytes) so `strncpy` never splits a
  character.
- Persist slots hold at most `PERSIST_DATA_MAX_LENGTH` (256) bytes; add a
  `_Static_assert` on struct size. Version persisted structs with a key so an
  update doesn't read garbage.
- `GColorX` compound literals are not valid static initialisers; use
  `GColorXARGB8` integers in tables and build `(GColor){ .argb = v }` at runtime.
- Static `GPathInfo` uses `(GPoint[]){...}` compound literals — fine at file
  scope with GCC.
- A `MenuLayer` on a round watch wants `menu_layer_set_center_focused(m, true)`.
- Destroying a window from its own `unload` handler is the standard pattern
  for pushed sub-windows; for the root window destroy it in `deinit` after
  `window_stack_pop_all(false)`.
- JS runs as ES5 in some phone apps: no arrow functions, `let`, template
  literals or Promises. `btoa` is not guaranteed — bundle a base64 helper.
- `Pebble.sendAppMessage` must be serialised: send the next dict only from the
  success/failure callback (queue pattern in `references/pebblekit-js.md`).
- Incoming payload keys in JS may arrive as numeric ids or names depending on
  the phone app; look up both (`payload[keys.CMD] ?? payload.CMD`).
- The settings page needs `"capabilities": ["configurable"]` in package.json
  or the gear icon never appears. A `data:text/html,...` URL via
  `Pebble.openURL` works without hosting; close with `pebblejs://close#<json>`.
- `pebble send-app-message` reaches the JS side only, not the C inbox.
- Emulator quirks: first `install` after cold start may fail (retry once);
  `pebble kill && pebble wipe` fixes a wedged emulator; long press = `emu-button
  push` then `release`, not `click --duration`. Persistent `Connection refused`
  with QEMU running = pypkjs could not bind (no IPv6 in containers); fix and
  other container findings in `references/cli-and-emulator.md`.

## Design habits that paid off

- Give the phone side a **demo backend** behind a magic token (e.g. `demo`):
  the watch UI can be exercised and screenshotted without an account, and
  users can try the app before configuring it.
- Confirm every phone-side action on the watch (short vibe + "Gestartet: …"
  for two seconds, cleared by an `app_timer`) so users need not look twice.
- Keep a **sticky hint** separate from transient messages: reminders set the
  hint, normal status updates clear only the message, any button clears both.
- Version persisted structs by moving to a new persist key when the layout
  changes; reading an old layout into a grown struct yields garbage.

## Reference map

| Need | Read |
|------|------|
| Screen sizes, colours, buttons, memory, action bar widths, fonts | `references/platforms.md` |
| C API patterns and signatures used in almost every app | `references/c-api.md` |
| Phone-side JS: queue, HTTP, settings page, caching, testing | `references/pebblekit-js.md` |
| pebble-tool commands, emulator, screenshots, logs, install | `references/cli-and-emulator.md` |
| Compile check without the SDK build | `scripts/check_compile.sh` |
| Starter project (app + JS + settings page) | `assets/template/` |
| Node mock of the PebbleKit JS runtime for smoke tests | `assets/pkjs-mock.js` |

A complete worked example (Toggl Track timer app with status screen, menu,
AppMessage protocol, settings page, cache and Node smoke test) lives in this
repository under `pebble-toggl-track/`. Read it when you want to see all the
pieces wired together.

## Delivery checklist

- `package.json` has a fresh `uuid`, sensible `displayName`, `targetPlatforms`
  and every `messageKeys` entry that the C or JS code uses.
- Layout verified on emery and at least one 144×168 platform (screenshot or
  reasoning from bounds); text never overlaps the action bar.
- Every AppMessage command is handled on both sides and errors reach the
  watch as a visible message, not only `console.log`.
- Build or compile check is green with the SDK warning flags; JS passes
  `node --check`. State which verification actually ran.
