# Mofei X3 / X4 Install and Validation

Date: 2026-04-24
Executor: Codex

This guide verifies the Mofei firmware on XTEINK X3 and XTEINK X4 devices.
Do not flash until the target device appears in `pio device list` and the upload port is confirmed.

## Build Targets

| Device | PlatformIO env | Firmware artifact | Hardware path |
| --- | --- | --- | --- |
| XTEINK X4 | `mofei_x4` | `.pio/build/mofei_x4/firmware.bin` | `MOFEI_FORCE_X4`, `DeviceType::X4` |
| XTEINK X4 compatible default | `mofei` | `.pio/build/mofei/firmware.bin` | `MOFEI_FORCE_X4`, `DeviceType::X4` |
| XTEINK X3 | `mofei_x3` | `.pio/build/mofei_x3/firmware.bin` | `MOFEI_FORCE_X3`, `DeviceType::X3`, X3 display mode |

## 1. Detect Device

Connect one device at a time, then run:

```bash
pio device list
ls -1 /dev/cu.* /dev/tty.* 2>/dev/null
```

Expected result: a USB serial port such as `/dev/cu.usbmodem*`, `/dev/cu.usbserial*`, or `/dev/cu.wchusbserial*`.
Do not use `/dev/cu.Bluetooth-Incoming-Port`.

Set the confirmed port:

```bash
export XTEINK_PORT=/dev/cu.usbmodemXXXX
```

## 1.1 Quick Script Flow

Use the guarded helper script when a device is connected. It requires an explicit device type and refuses to upload
without a confirmed serial port.

Build only:

```bash
scripts/mofei_flash_validate.sh --device x4 --build-only
scripts/mofei_flash_validate.sh --device x3 --build-only
```

Build and flash:

```bash
scripts/mofei_flash_validate.sh --device x4 --port "$XTEINK_PORT"
scripts/mofei_flash_validate.sh --device x3 --port "$XTEINK_PORT"
```

Build, flash, then open serial monitor:

```bash
scripts/mofei_flash_validate.sh --device x4 --port "$XTEINK_PORT" --monitor
scripts/mofei_flash_validate.sh --device x3 --port "$XTEINK_PORT" --monitor
```

The script exits before upload if no port is provided, and it rejects Bluetooth/debug-console ports.

## 2. Build Firmware

For X4:

```bash
pio run -e mofei_x4
```

For X3:

```bash
pio run -e mofei_x3
```

The compatibility X4 build is:

```bash
pio run -e mofei
```

Success criteria:

- Build ends with `SUCCESS`.
- Flash usage remains below the `0x600000` app slot from `partitions_mofei.csv`.
- Firmware artifact exists under `.pio/build/<env>/firmware.bin`.

## 3. Flash Device

Only flash after confirming `XTEINK_PORT` points to the connected target.

For X4:

```bash
pio run -e mofei_x4 --target upload --upload-port "$XTEINK_PORT"
```

For X3:

```bash
pio run -e mofei_x3 --target upload --upload-port "$XTEINK_PORT"
```

For the compatibility X4 build:

```bash
pio run -e mofei --target upload --upload-port "$XTEINK_PORT"
```

If upload fails, disconnect other serial devices and rerun `pio device list` before retrying.

## 4. Monitor Boot Logs

Run after flashing:

```bash
pio device monitor --port "$XTEINK_PORT" --baud 115200
```

Expected X4 log checkpoints:

- `Hardware detect: X4`
- `Starting CrossPoint version 1.2.0-mofei-x4` for `mofei_x4`
- `Mofei Dashboard` appears after boot

Expected X3 log checkpoints:

- `Hardware detect: X3`
- `Starting CrossPoint version 1.2.0-mofei-x3`
- Display initializes with the X3 hardware path, which calls `setDisplayX3()` through `DeviceType::X3`
- `Mofei Dashboard` appears after boot

## 5. Screen Validation

Check these items on both devices:

- First boot clears the full screen and renders the dashboard without shifted or clipped layout.
- Header text is readable and not rotated incorrectly.
- Dashboard cards fit inside screen bounds.
- Opening Study, Reading, Desktop, and Arcade packages refreshes cleanly.
- Returning with Back does not leave stale high-contrast artifacts.

X3-specific screen checks:

- No 800x480 clipping on the 792x528 panel.
- Button hints remain visible at the bottom.
- Long text cards in Study Cards wrap inside the visible area.

## 6. Button Validation

From the dashboard:

- Up/Down changes selection.
- Confirm opens the selected package.
- Back returns to the previous screen.
- Left/Right work in queue/deck views where labels show `Deck -` and `Deck +`.
- Power button enters sleep or wakes according to current settings.

If buttons are swapped or missing, capture the serial log and ADC values before changing mappings.

## 7. Storage and Study Data Validation

Prepare an SD card with:

```text
/.mofei/study/sample.json
/.mofei/fonts/notosans_tc_12.epf
/.mofei/fonts/notosans_tc_14.epf
/.mofei/fonts/notosans_tc_16.epf
/.mofei/fonts/notosans_tc_18.epf
```

Minimal study deck example:

```json
{
  "name": "Smoke Test",
  "cards": [
    {"front": "apple", "back": "苹果", "example": "An apple a day."},
    {"front": "river", "back": "河流", "example": "The river is wide."}
  ]
}
```

Validate:

- Study Hub shows imported deck/card counts.
- Study Cards can flip and record Know, Again, Later, Save.
- Recovery opens after Again cards exist.
- Later Cards and Saved Cards show `Deck X/Y Card A/B`.
- Learning Report shows queue counts and weak area guidance.

## 8. Sleep and Wake Validation

Validate after the dashboard renders:

- Enter sleep from the device UI or power button.
- Wake with the power button.
- Confirm the screen refreshes from sleep without corruption.
- Confirm the device returns to Mofei Dashboard instead of a reader resume loop.

For X3, also watch for fuel gauge or I2C warnings in the serial monitor.

## 9. Failure Handling

If the device boots but the screen is blank:

- Confirm the correct env was flashed.
- X3 must use `mofei_x3`.
- X4 should use `mofei_x4` or `mofei`.
- Reflash while holding the device in bootloader mode if required by the board.

If SD card initialization fails:

- Reformat the card as FAT32 or exFAT.
- Confirm the study directory is exactly `/.mofei/study`.
- Reinsert the card and reboot.

If serial monitor shows the wrong hardware label:

- Stop testing the device.
- Rebuild and reflash the correct env.
- Capture the first 100 boot log lines for diagnosis.

## 10. Current Local Check

On 2026-04-24, Codex detected no connected XTEINK USB serial device on this Mac. The only listed ports were `/dev/cu.debug-console` and Bluetooth. Upload was intentionally not executed to avoid flashing the wrong target.

The firmware build artifacts for `mofei`, `mofei_x4`, and `mofei_x3` were generated successfully before this document was written.
