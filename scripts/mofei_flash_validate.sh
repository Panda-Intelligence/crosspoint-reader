#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  scripts/mofei_flash_validate.sh [--port PORT] [--build-only] [--monitor]

Examples:
  scripts/mofei_flash_validate.sh --build-only
  scripts/mofei_flash_validate.sh --port /dev/cu.usbmodemXXXX --monitor
  MOFEI_PORT=/dev/cu.usbmodemXXXX scripts/mofei_flash_validate.sh

Safety rules:
  - This script targets the Mofei ESP32-S3 PlatformIO env: mofei.
  - Upload/monitor require --port or MOFEI_PORT.
  - Without a port, the script lists PlatformIO devices and exits before upload.
  - Bluetooth/debug-console ports are rejected.
USAGE
}

port="${MOFEI_PORT:-${XTEINK_PORT:-}}"
build_only=0
monitor_after=0
env_name="mofei"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port)
      if [[ $# -lt 2 ]]; then
        echo "error: --port requires a serial device path" >&2
        exit 2
      fi
      port="$2"
      shift 2
      ;;
    --build-only)
      build_only=1
      shift
      ;;
    --monitor)
      monitor_after=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

echo "[mofei] target device: Mofei ESP32-S3"
echo "[mofei] platformio env: $env_name"

pio run -e "$env_name"

artifact=".pio/build/${env_name}/firmware.bin"
if [[ ! -f "$artifact" ]]; then
  echo "error: expected firmware artifact not found: $artifact" >&2
  exit 1
fi

echo "[mofei] artifact ready: $artifact"

if [[ "$build_only" -eq 1 ]]; then
  echo "[mofei] build-only mode complete; upload skipped"
  exit 0
fi

if [[ -z "$port" ]]; then
  echo "error: upload requires --port or MOFEI_PORT" >&2
  echo "[mofei] connected devices:" >&2
  pio device list >&2 || true
  exit 3
fi

case "$port" in
  *Bluetooth*|*debug-console*)
    echo "error: refusing unsafe/non-device port: $port" >&2
    exit 3
    ;;
esac

echo "[mofei] uploading to $port"
pio run -e "$env_name" --target upload --upload-port "$port"

if [[ "$monitor_after" -eq 1 ]]; then
  echo "[mofei] opening serial monitor on $port"
  pio device monitor --port "$port" --baud 115200
else
  echo "[mofei] upload complete; run monitor with:"
  echo "  pio device monitor --port \"$port\" --baud 115200"
fi
