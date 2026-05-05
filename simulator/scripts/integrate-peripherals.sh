#!/usr/bin/env bash
# integrate-peripherals.sh — Copy Mofei custom QEMU peripherals into the
# cloned espressif/qemu source tree and apply Meson + Kconfig + machine
# patches. Idempotent. Re-run `scripts/build-qemu.sh` afterwards to compile.
#
# Status (2026-05-05): Committed alongside PR3. The Meson + Kconfig
# fragment application IS exercised; the ESP32-S3 machine-source patch
# (`esp32s3-soc-machine.patch`) is committed but **not yet runtime-tested**.
# Apply with --soc-patch when ready to iterate.
#
# Usage:
#   simulator/scripts/integrate-peripherals.sh           # copy + meson + kconfig
#   simulator/scripts/integrate-peripherals.sh --soc     # also apply esp32s3 patch
#   simulator/scripts/integrate-peripherals.sh --revert  # undo (does best effort)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIM_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SRC_DIR="${SIM_ROOT}/qemu-peripherals"
QEMU_DIR="${SIM_ROOT}/.qemu-cache/qemu"

if [[ ! -d "${QEMU_DIR}" ]]; then
  echo "[integrate] ERROR: ${QEMU_DIR} not found. Run scripts/build-qemu.sh first." >&2
  exit 1
fi

APPLY_SOC=0
REVERT=0
for arg in "$@"; do
  case "${arg}" in
    --soc)    APPLY_SOC=1 ;;
    --revert) REVERT=1 ;;
    *) echo "[integrate] unknown flag: ${arg}" >&2; exit 2 ;;
  esac
done

DISPLAY_DST="${QEMU_DIR}/hw/display/ssd1677_gdeq0426t82.c"
SENSOR_DST="${QEMU_DIR}/hw/sensor/ft6336u.c"
DISPLAY_MESON="${QEMU_DIR}/hw/display/meson.build"
DISPLAY_KCONFIG="${QEMU_DIR}/hw/display/Kconfig"
SENSOR_MESON="${QEMU_DIR}/hw/sensor/meson.build"
SENSOR_KCONFIG="${QEMU_DIR}/hw/sensor/Kconfig"
XTENSA_KCONFIG="${QEMU_DIR}/hw/xtensa/Kconfig"

# Sentinel strings — using these we make the script idempotent: append once,
# detect and skip on re-run.
DISPLAY_MESON_SENTINEL="CONFIG_SSD1677_GDEQ0426"
SENSOR_MESON_SENTINEL="CONFIG_FT6336U_MOFEI"
DISPLAY_KCONFIG_SENTINEL="config SSD1677_GDEQ0426"
SENSOR_KCONFIG_SENTINEL="config FT6336U_MOFEI"
XTENSA_KCONFIG_SENTINEL="select SSD1677_GDEQ0426"

revert_one() {
  local file="$1"
  local sentinel="$2"
  if [[ -f "${file}" ]] && grep -qF "${sentinel}" "${file}"; then
    # Strip our additions: any line containing the sentinel and the lines
    # comprising the same fragment block. Crude but adequate for our
    # short fragments. We keep a backup at file.bak.
    cp -f "${file}" "${file}.revert.bak"
    grep -vF "${sentinel}" "${file}" > "${file}.tmp"
    mv -f "${file}.tmp" "${file}"
    echo "[integrate] reverted: ${file}"
  fi
}

if [[ ${REVERT} -eq 1 ]]; then
  rm -f "${DISPLAY_DST}" "${SENSOR_DST}"
  revert_one "${DISPLAY_MESON}"  "${DISPLAY_MESON_SENTINEL}"
  revert_one "${DISPLAY_KCONFIG}" "${DISPLAY_KCONFIG_SENTINEL}"
  revert_one "${SENSOR_MESON}"   "${SENSOR_MESON_SENTINEL}"
  revert_one "${SENSOR_KCONFIG}" "${SENSOR_KCONFIG_SENTINEL}"
  revert_one "${XTENSA_KCONFIG}" "${XTENSA_KCONFIG_SENTINEL}"
  echo "[integrate] revert complete (rerun build-qemu.sh to recompile)"
  exit 0
fi

# Copy peripheral sources.
echo "[integrate] copying ssd1677_gdeq0426t82.c → hw/display/"
cp -f "${SRC_DIR}/ssd1677_gdeq0426t82.c" "${DISPLAY_DST}"

mkdir -p "${QEMU_DIR}/hw/sensor"
echo "[integrate] copying ft6336u.c → hw/sensor/"
cp -f "${SRC_DIR}/ft6336u.c" "${SENSOR_DST}"

# Apply Meson fragments (idempotent).
append_if_missing() {
  local file="$1"
  local sentinel="$2"
  local fragment="$3"
  if [[ ! -f "${file}" ]]; then
    echo "[integrate] WARN: ${file} not found, skipping" >&2
    return
  fi
  if grep -qF "${sentinel}" "${file}"; then
    echo "[integrate] already present in ${file##*/}: ${sentinel}"
    return
  fi
  echo "" >> "${file}"
  cat "${fragment}" >> "${file}"
  echo "[integrate] appended fragment to ${file##*/}"
}

append_if_missing "${DISPLAY_MESON}"   "${DISPLAY_MESON_SENTINEL}"   "${SRC_DIR}/display-meson.fragment"
append_if_missing "${DISPLAY_KCONFIG}" "${DISPLAY_KCONFIG_SENTINEL}" "${SRC_DIR}/display-Kconfig.fragment"

# hw/sensor/ may not have a meson.build; create one if missing.
if [[ ! -f "${SENSOR_MESON}" ]]; then
  echo "[integrate] creating ${SENSOR_MESON} (fork did not have one)"
  cat > "${SENSOR_MESON}" <<'EOF'
# Auto-managed by simulator/scripts/integrate-peripherals.sh
sensor_ss = ss.source_set()
EOF
fi
if [[ ! -f "${SENSOR_KCONFIG}" ]]; then
  echo "[integrate] creating ${SENSOR_KCONFIG} (fork did not have one)"
  touch "${SENSOR_KCONFIG}"
fi

append_if_missing "${SENSOR_MESON}"   "${SENSOR_MESON_SENTINEL}"   "${SRC_DIR}/sensor-meson.fragment"
append_if_missing "${SENSOR_KCONFIG}" "${SENSOR_KCONFIG_SENTINEL}" "${SRC_DIR}/sensor-Kconfig.fragment"
append_if_missing "${XTENSA_KCONFIG}" "${XTENSA_KCONFIG_SENTINEL}" "${SRC_DIR}/xtensa-Kconfig.fragment"

# SoC-machine patch is opt-in because it has not been runtime-tested.
if [[ ${APPLY_SOC} -eq 1 ]]; then
  PATCH="${SRC_DIR}/esp32s3-soc-machine.patch"
  if [[ ! -f "${PATCH}" ]]; then
    echo "[integrate] ERROR: ${PATCH} not found" >&2
    exit 3
  fi
  echo "[integrate] applying SoC machine patch (UNVERIFIED — see INTEGRATION.md)"
  ( cd "${QEMU_DIR}" && git apply --check "${PATCH}" 2>/dev/null && git apply "${PATCH}" ) || {
    echo "[integrate] WARN: patch did not apply cleanly. Manual integration required."
    echo "[integrate]       Patch lives at: ${PATCH}"
    exit 4
  }
  echo "[integrate] SoC patch applied. Re-run scripts/build-qemu.sh to recompile."
fi

echo ""
echo "[integrate] done."
echo "[integrate] next: simulator/scripts/build-qemu.sh"
[[ ${APPLY_SOC} -eq 0 ]] && echo "[integrate] (re-run with --soc to also apply the ESP32-S3 machine patch)"
