#!/usr/bin/env bash
# Build and flash the firmware over USB.
#
#   bash scripts/flash.sh                 build, then flash
#   bash scripts/flash.sh --monitor       ... and open the serial console
#   bash scripts/flash.sh --port /dev/... when auto-detection picks the wrong one
#   bash scripts/flash.sh --any          skip the board identity check
#   bash scripts/flash.sh --fs            also upload the LittleFS image
#   bash scripts/flash.sh --erase         wipe the whole chip first - see below
#
# Everything runs through PlatformIO, with no hard-coded toolchain paths, so it
# behaves the same on macOS, Linux and Git Bash.
#
# WHAT GETS WRITTEN, AND WHY YOUR WI-FI SURVIVES
#
# An ordinary flash writes three images at three offsets:
#
#     0x0000    bootloader.bin
#     0x8000    partitions.bin
#     0x10000   firmware.bin       <- the app slot, ota_0
#
# The NVS partition lives at 0x9000 and is 0x5000 long. Nothing above touches
# it, which is why the saved Wi-Fi credentials, the TigerTag session and the
# imported printers are all still there after you reflash. That is not luck; it
# is the partition layout doing its job.
#
# --erase removes that guarantee on purpose: it wipes the entire chip, NVS
# included, so the device comes up as if it had never been configured. Use it to
# reproduce a first-boot experience, and expect to provision the board again.
#
# The same arithmetic is the reason CLAUDE.md forbids writing a merged factory
# image at 0x0000 on a provisioned device: a merged image spans from zero and
# therefore covers 0x9000, wiping the user's setup with no warning and no undo.

set -euo pipefail
cd "$(dirname "$0")/.."

ENV=tigerspool
PORT=""
ANY=0
MONITOR=0
FS=0
ERASE=0

while [ $# -gt 0 ]; do
  case "$1" in
    --port)    PORT="${2:?--port needs a device path}"; shift 2 ;;
    --any)     ANY=1; shift ;;
    --monitor) MONITOR=1; shift ;;
    --fs)      FS=1; shift ;;
    --erase)   ERASE=1; shift ;;
    -h|--help) sed -n '2,10p' "$0"; exit 0 ;;
    *) echo "unknown option: $1"; sed -n '2,10p' "$0"; exit 2 ;;
  esac
done

command -v pio >/dev/null 2>&1 || {
  echo "error: pio not on PATH. Install PlatformIO, or use its own shim:"
  echo "       ~/.platformio/penv/bin/pio"
  exit 2
}

# ---------------------------------------------------------------------------
#  Which board is on the other end of that cable?
#
#  The incident this exists for: a TigerScale and a TigerSpool were plugged
#  into the same Mac. The TigerSpool's port disappeared and reappeared under a
#  different name, the remaining port was assumed to be it, and TigerSpool
#  firmware was written to the TigerScale four times before anyone noticed.
#
#  A serial port name is not an identity - it is whatever the OS handed out
#  this time. The MAC is the identity, and esptool can read it without writing
#  anything. So: read first, match, and only then upload.
#
#  The expected MAC lives in .bench-mac, which is gitignored - it is a fact
#  about one desk, not about the project.
# ---------------------------------------------------------------------------
ESPTOOL="$HOME/.platformio/penv/bin/python -m esptool"
EXPECT="${TIGERSPOOL_MAC:-$(cat .bench-mac 2>/dev/null || true)}"

# A port that does not answer is not an error: it is a port that is not an
# ESP32, or one another program is holding. It has to come back EMPTY and let
# the search carry on to the next port.
#
# Without the `|| true` this function's failure took the whole script with it -
# `set -e` plus `pipefail`, and the read is a pipeline. A second USB serial
# device appeared on this bench, esptool could not talk to it, and flash.sh
# exited before writing anything while printing nothing at all. Three flashes
# in a row were believed to have landed and had not; the board kept running
# older firmware and every measurement taken from it was about that older
# firmware. A tool that fails silently is worse than one that fails.
mac_of() { { $ESPTOOL --port "$1" --no-stub read_mac 2>/dev/null \
             | awk '/^MAC:/ { print tolower($2); exit }'; } || true; }

if [ "$ANY" = 1 ]; then
  echo "== --any: skipping the board check"
elif [ -z "$EXPECT" ]; then
  ports=(/dev/cu.usbmodem* /dev/ttyUSB* /dev/ttyACM*)
  found=(); for p in "${ports[@]}"; do [ -e "$p" ] && found+=("$p"); done
  if [ "${#found[@]}" -eq 1 ]; then
    m="$(mac_of "${found[0]}")"
    echo "== no .bench-mac yet; one board present: ${found[0]} ($m)"
    echo "   record it so this can never go to the wrong board:"
    echo "       echo $m > .bench-mac"
    PORT="${found[0]}"
  else
    echo "error: more than one board is plugged in and .bench-mac is not set."
    for p in "${found[@]}"; do echo "       $p  $(mac_of "$p")"; done
    echo "       Record the TigerSpool's MAC:  echo <mac> > .bench-mac"
    exit 2
  fi
else
  match=""
  for p in /dev/cu.usbmodem* /dev/ttyUSB* /dev/ttyACM*; do
    [ -e "$p" ] || continue
    m="$(mac_of "$p")"
    echo "== $p is $m"
    [ "$m" = "$EXPECT" ] && match="$p"
  done
  if [ -z "$match" ]; then
    echo "error: no board with MAC $EXPECT is plugged in."
    echo "       Nothing was written. Use --any to override deliberately."
    exit 2
  fi
  if [ -n "$PORT" ] && [ "$PORT" != "$match" ]; then
    echo "note: --port said $PORT, but $EXPECT is on $match - using $match"
  fi
  PORT="$match"
  echo "== target confirmed: $PORT ($EXPECT)"
fi

cd firmware
PORT_ARG=()
[ -n "$PORT" ] && PORT_ARG=(--upload-port "$PORT")
# Expanding an empty array under `set -u` is an error in bash 3.2, which is
# what /bin/bash on macOS still is. This guard is the portable spelling; the
# script worked only when a port was passed without it.

if [ "$ERASE" = 1 ]; then
  echo "== erasing the whole chip - NVS included, so Wi-Fi and the account go too"
  pio run -e "$ENV" -t erase ${PORT_ARG[@]+"${PORT_ARG[@]}"}
fi

echo "== building and flashing $ENV"
pio run -e "$ENV" -t upload ${PORT_ARG[@]+"${PORT_ARG[@]}"}

if [ "$FS" = 1 ]; then
  echo "== uploading the LittleFS image"
  pio run -e "$ENV" -t uploadfs ${PORT_ARG[@]+"${PORT_ARG[@]}"}
fi

if [ "$MONITOR" = 1 ]; then
  echo "== serial console - ctrl-c twice to leave"
  pio device monitor -e "$ENV" "${PORT_ARG[@]/--upload-port/--port}"
fi
