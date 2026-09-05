#!/bin/bash
# Compile every C file of a Pebble project per platform against the SDK
# headers, using the SDK's own warning flags. Catches wrong API names,
# missing platform guards and -Werror failures without a full `pebble build`
# (no linking, no resources, no .pbw).
#
# usage: check_compile.sh <project-dir> [platform ...]
#
# Finds SDK headers in (first match wins):
#   $PEBBLE_SDK_CORE           -> <dir>/pebble/<platform>/include
#   ~/.pebble-sdk/SDKs/current/sdk-core
#   any ~/.pebble-sdk/SDKs/*/sdk-core
# Requires arm-none-eabi-gcc (apt: gcc-arm-none-eabi).
set -u
PROJECT=${1:?project dir}; shift || true
HERE=$(cd "$(dirname "$0")" && pwd)
PLATFORMS=("$@")
[ ${#PLATFORMS[@]} -eq 0 ] && PLATFORMS=(aplite basalt chalk diorite emery)

if ! command -v arm-none-eabi-gcc >/dev/null; then
  echo "arm-none-eabi-gcc not found (apt-get install gcc-arm-none-eabi)"; exit 2
fi

SDK_CORE=${PEBBLE_SDK_CORE:-}
if [ -z "$SDK_CORE" ]; then
  for c in "$HOME/.pebble-sdk/SDKs/current/sdk-core" "$HOME"/.pebble-sdk/SDKs/*/sdk-core; do
    [ -d "$c/pebble" ] && SDK_CORE=$c && break
  done
fi
if [ -z "$SDK_CORE" ] || [ ! -d "$SDK_CORE/pebble" ]; then
  echo "SDK headers not found; set PEBBLE_SDK_CORE=<path to sdk-core>"; exit 2
fi
echo "SDK core: $SDK_CORE"

WORK=$(mktemp -d)
mkdir -p "$WORK/src" "$WORK/stub"
python3 "$HERE/gen_message_keys.py" "$PROJECT/package.json" "$WORK/message_keys.auto.h" || exit 2
# Resource ids: one define per resource name so RESOURCE_ID_* resolves.
python3 - "$PROJECT/package.json" "$WORK/src/resource_ids.auto.h" <<'PY'
import json, sys
pkg = json.load(open(sys.argv[1]))
media = pkg.get("pebble", {}).get("resources", {}).get("media", [])
with open(sys.argv[2], "w") as out:
    out.write("#pragma once\n")
    for i, m in enumerate(media, start=2):
        out.write("#define RESOURCE_ID_%s %d\n" % (m["name"], i))
PY
# The Pebble toolchain ships a minimal time.h; pebble.h defines struct tm itself.
printf '#pragma once\ntypedef long time_t;\ntypedef long clock_t;\n' > "$WORK/stub/time.h"

declare -A DEFS
DEFS[aplite]="-DPBL_PLATFORM_APLITE -DPBL_BW -DPBL_RECT -DPBL_COMPASS -DPBL_DISPLAY_WIDTH=144 -DPBL_DISPLAY_HEIGHT=168 -mcpu=cortex-m3"
DEFS[basalt]="-DPBL_PLATFORM_BASALT -DPBL_COLOR -DPBL_RECT -DPBL_MICROPHONE -DPBL_SMARTSTRAP -DPBL_HEALTH -DPBL_COMPASS -DPBL_SMARTSTRAP_POWER -DPBL_DISPLAY_WIDTH=144 -DPBL_DISPLAY_HEIGHT=168 -mcpu=cortex-m3"
DEFS[chalk]="-DPBL_PLATFORM_CHALK -DPBL_COLOR -DPBL_ROUND -DPBL_MICROPHONE -DPBL_SMARTSTRAP -DPBL_HEALTH -DPBL_COMPASS -DPBL_SMARTSTRAP_POWER -DPBL_DISPLAY_WIDTH=180 -DPBL_DISPLAY_HEIGHT=180 -mcpu=cortex-m3"
DEFS[diorite]="-DPBL_PLATFORM_DIORITE -DPBL_BW -DPBL_RECT -DPBL_MICROPHONE -DPBL_HEALTH -DPBL_SMARTSTRAP -DPBL_DISPLAY_WIDTH=144 -DPBL_DISPLAY_HEIGHT=168 -mcpu=cortex-m3"
DEFS[emery]="-DPBL_PLATFORM_EMERY -DPBL_COLOR -DPBL_RECT -DPBL_MICROPHONE -DPBL_SMARTSTRAP -DPBL_HEALTH -DPBL_SMARTSTRAP_POWER -DPBL_COMPASS -DPBL_DISPLAY_WIDTH=200 -DPBL_DISPLAY_HEIGHT=228 -mcpu=cortex-m4"
DEFS[flint]="-DPBL_PLATFORM_FLINT -DPBL_BW -DPBL_RECT -DPBL_MICROPHONE -DPBL_HEALTH -DPBL_DISPLAY_WIDTH=144 -DPBL_DISPLAY_HEIGHT=168 -mcpu=cortex-m4"

rc=0
for p in "${PLATFORMS[@]}"; do
  inc="$SDK_CORE/pebble/$p/include"
  if [ ! -d "$inc" ]; then echo "$p: no headers in this SDK, skipped"; continue; fi
  mkdir -p "$WORK/out/$p"
  ok=1
  for f in "$PROJECT"/src/c/*.c "$PROJECT"/src/c/**/*.c; do
    [ -f "$f" ] || continue
    arm-none-eabi-gcc -std=c11 -mthumb -ffunction-sections -fdata-sections -Os -g -fPIE \
      -Wall -Wextra -Werror -Wno-unused-parameter -Wno-error=unused-function \
      -Wno-error=unused-variable -Wno-builtin-declaration-mismatch \
      ${DEFS[$p]} -I"$WORK/stub" -I"$inc" -I"$WORK" -I"$PROJECT/src/c" \
      -c "$f" -o "$WORK/out/$p/$(basename "$f" .c).o" || { ok=0; rc=1; }
  done
  if [ $ok = 1 ]; then
    total=$(arm-none-eabi-size "$WORK/out/$p"/*.o | awk 'NR>1 {t+=$1; d+=$2; b+=$3} END {printf "text=%d data=%d bss=%d", t, d, b}')
    echo "$p: OK ($total)"
  else
    echo "$p: FAILED"
  fi
done
rm -rf "$WORK"
exit $rc
