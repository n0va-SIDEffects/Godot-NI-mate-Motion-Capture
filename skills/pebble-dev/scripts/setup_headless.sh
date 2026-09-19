#!/usr/bin/env bash
# Macht den Pebble-Emulator auf einem Rechner ohne Bildschirm lauffaehig.
#
#   ./setup_headless.sh          # pruefen und reparieren
#   ./setup_headless.sh --check  # nur pruefen, nichts aendern
#
# Idempotent. Nach jedem "uv tool install --force pebble-tool" erneut laufen
# lassen - die Neuinstallation ueberschreibt den Patch.

set -uo pipefail

NUR_PRUEFEN=0
[ "${1:-}" = "--check" ] && NUR_PRUEFEN=1

ok()   { printf '  ok      %s\n' "$1"; }
tat()  { printf '  getan   %s\n' "$1"; }
warn() { printf '  achtung %s\n' "$1"; }

export PATH="$HOME/.local/bin:$PATH"

echo "Pebble-Emulator, kopfloser Betrieb"

# --- 1. pebble-tool vorhanden? -------------------------------------------
if ! command -v pebble >/dev/null 2>&1; then
  warn "pebble nicht gefunden. Installieren mit:"
  echo "          uv tool install pebble-tool --python 3.13"
  echo '          export PATH="$HOME/.local/bin:$PATH"'
  exit 1
fi
ok "$(pebble --version < /dev/null 2>&1 | head -1)"

# --- 2. SDK installiert und aktiv? ---------------------------------------
SDKS=$(pebble sdk list < /dev/null 2>&1)
if ! grep -q "(active)" <<<"$SDKS"; then
  warn "Kein aktives SDK. 'pebble sdk install latest', danach"
  echo "          'pebble sdk activate <version>'."
else
  ok "SDK $(grep "(active)" <<<"$SDKS" | head -1 | tr -d ' ')"
fi

# --- 3. SDL2 vorhanden? ---------------------------------------------------
if ldconfig -p 2>/dev/null | grep -q libSDL2-2.0.so.0; then
  ok "libSDL2 vorhanden"
else
  warn "libSDL2 fehlt - qemu-pebble startet nicht. Auf Ubuntu 24.04:"
  echo "          sudo apt-get install -y libsdl2-2.0-0 libglib2.0-0t64 \\"
  echo "            libpixman-1-0 zlib1g libsndio7.0 libpng16-16t64"
fi

# --- 4. Xvfb vorhanden und laufend? --------------------------------------
if ! command -v Xvfb >/dev/null 2>&1; then
  warn "Xvfb fehlt: sudo apt-get install -y xvfb"
else
  ok "Xvfb vorhanden"
  if pgrep -f "Xvfb :99" >/dev/null 2>&1; then
    ok "Xvfb :99 laeuft bereits"
  elif [ "$NUR_PRUEFEN" = "1" ]; then
    warn "Xvfb :99 laeuft nicht"
  else
    nohup Xvfb :99 -screen 0 1024x768x24 >/dev/null 2>&1 &
    sleep 2
    pgrep -f "Xvfb :99" >/dev/null 2>&1 && tat "Xvfb :99 gestartet" \
                                        || warn "Xvfb liess sich nicht starten"
  fi
fi

# --- 5. pypkjs-Bind auf IPv4 ---------------------------------------------
# Ohne IPv6 bindet pypkjs seinen WebSocket auf ("", port), gevent loest das
# nach IPv6 auf, der Bind scheitert lautlos, und pebble-tool laeuft in
# "[Errno 111] Connection refused".
WS=$(find "$HOME/.local/share/uv/tools/pebble-tool" \
          -path "*/pypkjs/runner/websocket.py" 2>/dev/null | head -1)

if [ -z "$WS" ]; then
  warn "pypkjs/runner/websocket.py nicht gefunden - Patch uebersprungen."
elif grep -q 'WSGIServer(("0.0.0.0"' "$WS"; then
  ok "pypkjs bindet bereits auf 0.0.0.0"
elif [ -d /proc/sys/net/ipv6 ]; then
  ok "IPv6 vorhanden - Patch nicht noetig"
elif [ "$NUR_PRUEFEN" = "1" ]; then
  warn "pypkjs bindet auf \"\" und dieser Host hat kein IPv6 - Patch noetig"
else
  cp "$WS" "$WS.bak"
  sed -i 's/pywsgi.WSGIServer(("", self.port)/pywsgi.WSGIServer(("0.0.0.0", self.port)/' "$WS"
  if grep -q 'WSGIServer(("0.0.0.0"' "$WS"; then
    tat "pypkjs auf 0.0.0.0 umgestellt (Sicherung: $WS.bak)"
  else
    warn "Patch griff nicht - Datei von Hand pruefen: $WS"
  fi
fi

cat <<'HINWEIS'

Fuer jede Sitzung setzen:

  export PATH="$HOME/.local/bin:$PATH"
  export DISPLAY=:99
  export NO_PROXY=localhost,127.0.0.1

Dann:

  pebble build < /dev/null
  pebble install --emulator emery < /dev/null
  pebble screenshot --emulator emery --no-open shot.png < /dev/null
  pebble kill < /dev/null

Nicht "xvfb-run" benutzen: es beendet das Display mit dem Kommando und nimmt
den gerade gestarteten Emulator mit. Der naechste Screenshot scheitert dann
mit "[Errno 111] Connection refused".
HINWEIS
