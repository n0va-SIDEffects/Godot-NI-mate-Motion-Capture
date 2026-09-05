# pebble-tool: build, emulate, install, debug

## Install the SDK (Core Devices, 2025+)

```bash
uv tool install pebble-tool            # or: pipx install pebble-tool
export PATH="$HOME/.local/bin:$PATH"
pebble sdk install latest < /dev/null  # downloads sdk-core + ARM toolchain + QEMU
pebble sdk list
```

Needs Node (for PebbleKit JS bundling) and network access to
`sdk.repebble.com`. If that host is blocked, builds are impossible; fall back
to `scripts/check_compile.sh` (needs `arm-none-eabi-gcc`, e.g. apt package
`gcc-arm-none-eabi`, plus SDK headers) and say so.

## Everyday commands

```bash
pebble new-project --javascript myapp < /dev/null   # app + src/pkjs/index.js
pebble build < /dev/null                            # -> build/<name>.pbw
pebble clean

pebble install --emulator emery < /dev/null         # emery = Pebble Time 2
pebble install --emulator basalt < /dev/null        # 144x168 colour
pebble install --phone 192.168.1.23                 # Developer Connection in phone app
pebble install --cloudpebble                        # via CloudPebble proxy

pebble logs --emulator emery                        # APP_LOG + console.log
pebble screenshot --emulator emery --no-open shot.png
pebble screenshot --scale 3 --no-open shot.png      # bigger, for viewing

pebble emu-button --emulator emery click select
pebble emu-button click down --repeat 3 --interval 200
pebble emu-button push select ; sleep 1 ; pebble emu-button release select   # long press
pebble emu-app-config --emulator emery              # opens the settings page in a browser
pebble emu-time-format --emulator emery 24h
pebble emu-battery --pct 15
pebble emu-bt-connection --connected no
pebble kill                                         # stop QEMU
pebble wipe                                         # reset emulator flash (fixes wedged state)
pebble analyze-size
```

Always append `< /dev/null` (first-run prompts otherwise hang) and, on
headless machines, `--vnc` to every command that touches the emulator.

## A typical verification loop

```bash
pebble build < /dev/null && \
pebble install --emulator emery < /dev/null && \
sleep 2 && pebble screenshot --emulator emery --no-open /tmp/emery.png
```

Look at the PNG. Then press buttons with `emu-button` and screenshot again.
Repeat for `basalt` (small rectangular) and, if targeted, `chalk` (round).
Finish with `pebble kill`.

## Reading build errors

- `error: unknown type name` / `implicit declaration` → API missing on that
  platform; guard it or check the name in `pebble.h`.
- `-Werror=unused-value` on `app_glance_add_slice` → aplite stub; guard.
- `region 'APP' overflowed` → binary too big for the platform; drop aplite
  or shrink resources.
- JS bundling errors name the file and line; the bundler is plain webpack,
  so `require` paths must be relative and end up inside `src/pkjs/`.
- `Bad appinfo`/`messageKeys` complaints → package.json shape; compare with
  `assets/template/package.json`.

## Distribution

The `.pbw` sideloads through the phone app (open the file on the phone) or
the Rebble/Core appstore. Keep `uuid` stable across versions, bump `version`.
