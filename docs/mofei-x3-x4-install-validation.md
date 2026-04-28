# Mofei ESP32-S3 Install and Validation

Date: 2026-04-25
Executor: Codex

This guide verifies the Mofei firmware on the Mofei ESP32-S3 target device.
XTEINK X3 and XTEINK X4 are separate hardware targets and are not the ESP32-S3 Mofei device.
Do not choose X3/X4 when flashing this Mofei build.

## Build Target

| Device | PlatformIO env | Firmware artifact | Hardware path |
| --- | --- | --- | --- |
| Mofei ESP32-S3 | `mofei` | `.pio/build/mofei/firmware.bin` | `MOFEI_APP`, `MOFEI_DEVICE`, `DeviceType::MOFEI` |

## 1. Detect Serial Port

Connect one Mofei ESP32-S3 device at a time, then run:

```bash
pio device list
ls -1 /dev/cu.* /dev/tty.* 2>/dev/null
```

Expected result: a USB serial port such as `/dev/cu.usbmodem*`, `/dev/cu.usbserial*`, or `/dev/cu.wchusbserial*`.
Do not use `/dev/cu.Bluetooth-Incoming-Port` or `/dev/cu.debug-console`.

If no serial port appears, put the ESP32-S3 into ROM download mode:

1. Hold `BOOT`.
2. Press and release `RESET`.
3. Release `BOOT`.
4. Run the serial port detection commands again.

Set the confirmed port:

```bash
export MOFEI_PORT=/dev/cu.usbmodemXXXX
```

## 2. Quick Script Flow

Use the guarded helper script when a device is connected. It always targets the Mofei ESP32-S3 `mofei` environment and refuses to upload without a confirmed serial port.

Build only:

```bash
scripts/mofei_flash_validate.sh --build-only
```

Build and flash:

```bash
scripts/mofei_flash_validate.sh --port "$MOFEI_PORT"
```

Build, flash, then open serial monitor:

```bash
scripts/mofei_flash_validate.sh --port "$MOFEI_PORT" --monitor
```

The script exits before upload if no port is provided, and it rejects Bluetooth/debug-console ports.

## 3. Manual PlatformIO Commands

Build firmware:

```bash
pio run -e mofei
```

Flash after confirming `MOFEI_PORT` points to the connected Mofei ESP32-S3 target:

```bash
pio run -e mofei --target upload --upload-port "$MOFEI_PORT"
```

Monitor boot logs:

```bash
pio device monitor --port "$MOFEI_PORT" --baud 115200
```

## 4. Expected Boot Checkpoints

Expected serial checkpoints:

- `Starting CrossPoint version 1.2.0-mofei`
- Hardware detection selects the Mofei path.
- `Mofei Dashboard` appears after boot.

Expected screen checkpoints:

- First boot clears the screen and renders the dashboard.
- Up/Down moves the dashboard selection.
- Confirm opens the selected Mofei package.
- Back returns to the Mofei dashboard.

## 5. Study Deck Smoke Test

Optional SD card content:

```text
/.mofei/study/sample.json
/.mofei/fonts/notosans_tc_12.epf
/.mofei/fonts/notosans_tc_14.epf
/.mofei/fonts/notosans_tc_16.epf
/.mofei/fonts/notosans_tc_18.epf
```

Validate after the dashboard renders:

1. Open Study Package.
2. Open Study Cards.
3. Verify deck count loads from `/.mofei/study/*.json` when SD content exists.
4. Mark one card as Know, Again, Later, or Save.
5. Return to the dashboard without a crash or reboot.

## 6. Troubleshooting

If no serial port appears:

- Use a data-capable USB-C cable, not a charge-only cable.
- Connect directly to the Mac before using a hub.
- Enter ESP32-S3 ROM download mode with `BOOT` + `RESET`.
- Re-run `pio device list` after every reconnect.

If upload fails:

- Disconnect other serial devices.
- Confirm `MOFEI_PORT` is not Bluetooth/debug-console.
- Lower upload speed in `platformio.ini` only if the port is correct but transfer is unstable.
