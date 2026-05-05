#!/bin/bash
# Upload Murphy firmware files to Cloudflare R2
# Prerequisites: wrangler installed and authenticated
# Usage: bash scripts/upload-firmware.sh [version]

set -euo pipefail

VERSION="${1:-1.0.0}"
PROJECT_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/.pio/build/mofei"
BUCKET="murphy-firmware"

echo "=== Murphy Firmware Upload v${VERSION} ==="

# Check files exist
for f in bootloader.bin partitions.bin boot_app0.bin firmware.bin; do
  if [ ! -f "$BUILD_DIR/$f" ]; then
    echo "ERROR: $BUILD_DIR/$f not found. Run 'pio run' first."
    exit 1
  fi
  echo "  ✓ $f ($(du -h "$BUILD_DIR/$f" | cut -f1))"
done

echo ""
echo "Uploading to R2 bucket: $BUCKET..."

# Upload individual files
wrangler r2 object put "$BUCKET/bootloader.bin"   --file="$BUILD_DIR/bootloader.bin"   --content-type=application/octet-stream
wrangler r2 object put "$BUCKET/partitions.bin"   --file="$BUILD_DIR/partitions.bin"   --content-type=application/octet-stream
wrangler r2 object put "$BUCKET/boot_app0.bin"    --file="$BUILD_DIR/boot_app0.bin"    --content-type=application/octet-stream
wrangler r2 object put "$BUCKET/firmware.bin"     --file="$BUILD_DIR/firmware.bin"     --content-type=application/octet-stream

# Store version metadata in KV
wrangler kv key put --binding=FIRMWARE_META "latest-version" "$VERSION"
wrangler kv key put --binding=FIRMWARE_META "latest-build-date" "$(date +%Y-%m-%d)"
wrangler kv key put --binding=FIRMWARE_META "firmware-size" "$(stat -f%z "$BUILD_DIR/firmware.bin")"

echo ""
echo "=== Upload complete ==="
echo "  Web flasher: https://murphy.pandacat.ai/"
echo "  OTA endpoint: https://murphy.pandacat.ai/ota/latest"
echo "  Firmware:     https://murphy.pandacat.ai/firmware/firmware.bin"
