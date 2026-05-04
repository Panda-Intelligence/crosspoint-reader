#!/usr/bin/env bash
# build-qemu.sh — Build the Espressif QEMU fork for ESP32-S3 simulation.
#
# Idempotent: skips the build if a recent qemu-system-xtensa already exists
# in the cache. Re-clones only if the cache is missing.
#
# Required dependencies:
#   macOS:  brew install ninja glib pixman libgcrypt pkg-config gnutls
#   Linux:  apt install ninja-build libglib2.0-dev libpixman-1-dev libgcrypt20-dev pkg-config libgnutls28-dev
#
# Usage:
#   simulator/scripts/build-qemu.sh           # build if missing
#   simulator/scripts/build-qemu.sh --force   # always rebuild
#
# Output:
#   simulator/.qemu-cache/build/qemu-system-xtensa  (the binary used by start_sim)
#
# Notes:
# - This is committed unverified during the API outage of 2026-05-04. The
#   exact configure flags and target name (xtensa-softmmu) are based on
#   training-knowledge of the espressif/qemu README; see
#   .trellis/tasks/05-04-mofei-simulator-bringup/research/verification-pending.md
#   for what needs cross-checking against the live README.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIM_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CACHE_DIR="${SIM_ROOT}/.qemu-cache"
QEMU_SRC_DIR="${CACHE_DIR}/qemu"
QEMU_BUILD_DIR="${QEMU_SRC_DIR}/build"
QEMU_BIN="${QEMU_BUILD_DIR}/qemu-system-xtensa"
QEMU_REPO_URL="https://github.com/espressif/qemu"

FORCE=0
if [[ "${1:-}" == "--force" ]]; then
  FORCE=1
fi

if [[ ${FORCE} -eq 0 && -x "${QEMU_BIN}" ]]; then
  echo "[build-qemu] cache hit: ${QEMU_BIN}"
  echo "[build-qemu] (use --force to rebuild)"
  exit 0
fi

mkdir -p "${CACHE_DIR}"

if [[ ! -d "${QEMU_SRC_DIR}/.git" ]]; then
  echo "[build-qemu] cloning ${QEMU_REPO_URL} (depth 1) into ${QEMU_SRC_DIR}"
  git clone --depth 1 "${QEMU_REPO_URL}" "${QEMU_SRC_DIR}"
else
  echo "[build-qemu] reusing existing checkout at ${QEMU_SRC_DIR}"
fi

cd "${QEMU_SRC_DIR}"

if [[ ! -f "${QEMU_BUILD_DIR}/build.ninja" || ${FORCE} -eq 1 ]]; then
  echo "[build-qemu] configuring (target: xtensa-softmmu)"
  rm -rf "${QEMU_BUILD_DIR}"
  mkdir -p "${QEMU_BUILD_DIR}"
  cd "${QEMU_BUILD_DIR}"
  ../configure \
    --target-list=xtensa-softmmu \
    --enable-gcrypt \
    --enable-slirp \
    --disable-gnutls \
    --disable-docs \
    --disable-werror
else
  cd "${QEMU_BUILD_DIR}"
fi

JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
echo "[build-qemu] make -j${JOBS}"
make "-j${JOBS}"

# macOS post-build: the espressif fork emits qemu-system-xtensa-unsigned and
# expects a downstream codesign step. Apply an ad-hoc signature with JIT
# entitlements so QEMU's TCG can map executable pages on hardened runtime.
if [[ "$(uname -s)" == "Darwin" && -f "${QEMU_BUILD_DIR}/qemu-system-xtensa-unsigned" ]]; then
  ENTITLEMENTS="${QEMU_BUILD_DIR}/qemu-jit.entitlements"
  cat > "${ENTITLEMENTS}" <<'PLIST_EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.cs.allow-jit</key>
    <true/>
    <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
    <true/>
    <key>com.apple.security.cs.disable-executable-page-protection</key>
    <true/>
</dict>
</plist>
PLIST_EOF
  echo "[build-qemu] codesigning with JIT entitlements"
  cp -f "${QEMU_BUILD_DIR}/qemu-system-xtensa-unsigned" "${QEMU_BIN}"
  codesign --entitlements "${ENTITLEMENTS}" --force -s - "${QEMU_BIN}"
fi

if [[ ! -x "${QEMU_BIN}" ]]; then
  echo "[build-qemu] ERROR: build completed but ${QEMU_BIN} not found" >&2
  echo "[build-qemu] check the actual output binary name in ${QEMU_BUILD_DIR}" >&2
  exit 1
fi

echo "[build-qemu] success: ${QEMU_BIN}"
"${QEMU_BIN}" --version || true
echo "[build-qemu] supported ESP32-class machines:"
"${QEMU_BIN}" -machine help 2>/dev/null | grep -iE "esp32" || true
