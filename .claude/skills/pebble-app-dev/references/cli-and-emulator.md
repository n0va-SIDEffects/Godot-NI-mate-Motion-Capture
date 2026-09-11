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

Verified 2026-09 in a Claude Code cloud container (SDK 4.33.1, firmware 4.33.2):

- The install downloads ~48 MB sdk-core plus ~190 MB toolchain. pebble-tool
  streams them in 512-byte chunks, which through a slow proxy took hours.
  If it crawls, kill it, fetch both tarballs with parallel range requests
  (`scripts/pdl.py URL OUT SIZE 12` — ~1 MB/s vs 10 KB/s), then install them
  with `scripts/install_sdk_local.py core.tar.gz toolchain.tar.gz`. URLs come
  from `https://sdk.repebble.com/v1/files/sdk-core/latest?channel=` and
  `https://sdk.repebble.com/releases/<ver>/toolchain-linux-x86_64.tar.gz`.
- The SDK lands in `~/.local/share/pebble-sdk/SDKs/<ver>` (XDG), not
  `~/.pebble-sdk`, unless the legacy dir already exists.
- `qemu-pebble` needs `libSDL2-2.0.so.0` even with `--vnc`:
  `apt-get install -y libsdl2-2.0-0`.
- **No IPv6 in the container → pypkjs never opens its port** and every
  `install`/`screenshot` says `[Errno 111] Connection refused` while QEMU is
  visibly running. pypkjs binds `("", port)` which gevent maps to AF_INET6.
  Fix in the installed package (path from `pebble --version` venv):
  `pypkjs/runner/websocket.py` → `WSGIServer(("127.0.0.1", self.port), …)`
  and `pypkjs/runner/terminal.py` → `HTTPServer(('127.0.0.1', port), …)`,
  then `pebble kill` and install again. Diagnose with
  `PYTHONFAULTHANDLER=1 timeout -s ABRT 30 <venv>/bin/python -m pypkjs …`
  (the command line is in `/proc/<pid>/cmdline` of the running pypkjs).
- Only **one** `--vnc` emulator at a time: a second platform fights for VNC
  display `:1` (`Failed to find an available port`) and the first one dies.
  Test platforms sequentially with `pebble kill` in between.
- Re-installing while the app is open sometimes leaves the launcher on
  "Install an app to continue"; run `pebble install` once more.
- `pebble logs` is noisy with `[PHONESIM] Exception decoding
  QemuInboundPacket.footer` warnings; they are harmless. Filter them out.
- **Touch in the emulator**: pebble-tool has no touch command, but QEMU
  exposes the panel as "Pebble Touch (absolute)" and the VNC framebuffer is
  exactly the display (200×228 on emery), so VNC pointer events are touches:
  `pip install vncdotool`, then
  `vncdo -s localhost::5901 move X Y mousedown 1 pause 0.2 mouseup 1`.
  A bare `click 1` is too short to register. The QEMU monitor port from
  `/tmp/pb-emulator.json` accepts `mouse_move`/`mouse_button` as well.
- Screenshots take 1–2 s; sleep ~10 s after install before the first one so
  the JS side has answered.

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
