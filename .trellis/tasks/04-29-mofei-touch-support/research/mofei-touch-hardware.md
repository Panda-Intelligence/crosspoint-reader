# Mofei Touch Hardware Evidence

Date: 2026-04-29
Owner: Codex

## Documented GPIO / Net Mapping

Source: `docs/mofei/ic.png`

| Net | ESP32-S3 pin |
| --- | --- |
| `SDA` | GPIO13 |
| `SCL` | GPIO12 |
| `TOUCH_PWR` | GPIO45 |
| `TOUCH_INT` | GPIO44 |

The schematic image confirms the current default SDA/SCL/PWR/INT values are not arbitrary: they match the Mofei ESP32-S3R8 net labels.

Important limit: the image labels GPIO13/GPIO12 as generic `SDA`/`SCL`; it does not show the FT6336U FPC endpoint. Those nets may be a shared board I2C bus, and the observed `0x38` payload proves that at least one non-touch device can answer on that address.

## ESP32-S3 GPIO45 Capability

Source: `docs/mofei/esp32-s3_datasheet_en.pdf`

GPIO45 is not input-only in the local ESP32-S3 datasheet:

- Page 17 pin table lists `GPIO45` as `IO`, powered by `VDD3P3_CPU`, with weak pull-down and input enable.
- Page 22 IO MUX table lists `GPIO45` as `I/O/T`.
- Page 26 warns that `GPIO0`, `GPIO3`, `GPIO45`, and `GPIO46` are strapping pins and must be used with caution.
- Page 32 identifies GPIO45 as the `VDD_SPI voltage` strapping pin. Its default strapping configuration is weak pull-down, bit value `0`.
- Page 32 also says strapping pins are latched at chip reset and are then freed for regular IO after reset.
- Page 34 shows GPIO45 strap value `0` selects the default 3.3 V `VDD_SPI` source, while strap value `1` can select the 1.8 V flash voltage regulator when eFuse does not force a voltage.

Conclusion: keep the production `TOUCH_PWR` default on GPIO45 because `docs/mofei/ic.png` documents that net and the ESP32-S3 datasheet permits GPIO45 as regular IO after reset. The risk is not that GPIO45 cannot be driven; the risk is that externally driving or pulling it high at reset can change the latched `VDD_SPI` voltage strap. Diagnostics that avoid GPIO45 power control must be explicit build environments, not a production default change and not broad GPIO scanning.

## User Hardware Constraint

The user confirmed that a previously installed system on this same device supported touch. This changes the conclusion boundary:

- Treat the hardware as touch-capable.
- Do not conclude that Mofei lacks a touch controller.
- Current firmware evidence only proves that the current GPIO/init path reaches an AHT20-like `0x38` response instead of a valid FT6336U frame.
- The remaining task is to recover or infer the original system's touch GPIO and initialization sequence.

## Working-System Touch Address Evidence

Source: user-provided report from another developer on 2026-04-29.

The working Mofei touch system communicates with the FT6336U touch controller at I2C address `0x2E`. Treat this as a
7-bit I2C address unless later codebase or hardware evidence proves otherwise.

This supersedes the earlier production assumption that Mofei FT6336U must be at `0x38`. The vendor Arduino sample still
documents `FT_CMD_WR 0x70` and `FT_CMD_RD 0x71`, which are equivalent to 7-bit `0x38`, but that sample is module/protocol
context rather than confirmed Mofei board production evidence.

Implementation consequence:

- Production Mofei firmware should default `MOFEI_TOUCH_ADDR` to `0x2E`.
- `0x38` remains useful as a narrow legacy/vendor diagnostic override.
- AHT20-like frame rejection should remain in place, but logs should describe wrong-device/AHT20-like frames at the active
  address instead of implying `0x38` is necessarily the production FT6336U address.

## Vendor Arduino Sample

Sources:

- `docs/mofei/GDEQ0426T82-T01C_Arduino/i2c.h`
- `docs/mofei/GDEQ0426T82-T01C_Arduino/i2c.cpp`
- `docs/mofei/GDEQ0426T82-T01C_Arduino/GDEQ0426T82-T01C_Arduino.ino`

The vendor FT6336U sample uses:

| Function | Arduino label |
| --- | --- |
| Touch SDA | `A4` |
| Touch SCL | `A5` |
| Touch RST | `A3` |
| Touch IRQ | `A0` |

The sample resets FT6336U, then writes:

- `DEVICE_MODE` (`0x00`) = `0`
- `THGROUP` (`0x80`) = `22`
- `PERIODACTIVE` (`0x88`) = `14`

Before this task's driver hardening, the project driver already applied those FT6336U config values, but accepted
any readable `0x38` device during detection.

PlatformIO's current `esp32s3` Arduino variant maps these labels as:

| Arduino label | ESP32-S3 GPIO in current variant |
| --- | --- |
| `A0` | GPIO1 |
| `A3` | GPIO4 |
| `A4` | GPIO5 |
| `A5` | GPIO6 |

That mapping conflicts with the Mofei schematic usage for key and EPD control nets. Therefore the vendor sample is useful for FT6336U protocol and reset timing, but not sufficient as a Mofei board-level GPIO map.

## AHT20 Collision Observation

Observed `0x38` bytes such as `98 38 80 76 41` decode as an AHT20-style frame, not as a valid FT6336U status and point payload:

- FT6336U supports at most 2 touch points.
- `0x98 & 0x0F` yields 8 points, which is invalid for FT6336U.
- Stable AHT20-like frames on the same address explain why the current implementation can report a detected controller while touch remains unusable.

## Implementation Implication

The firmware should keep the documented bus pins from `ic.png`, but production detection should use the working-system
7-bit address `0x2E` through configurable `MOFEI_TOUCH_ADDR`. Detection must still prove the responding device behaves
like FT6336U. A plain ACK/read from any address is insufficient because the panel or board may expose a different device,
as shown by the earlier AHT20-like `0x38` frames on the same bus.

The legacy/vendor `0x38` address should be diagnostic-only (`MOFEI_TOUCH_ADDR=0x38`) unless later Mofei-specific evidence
overrides the working-system `0x2E` report.

## Device Validation: 2026-04-29

Device port:

- `/dev/cu.usbmodem1101`
- USB VID:PID `303A:1001`
- Serial `E0:72:A1:DB:0D:08`
- Description: `USB JTAG/serial debug unit`

Commands executed:

- `scripts/mofei_flash_validate.sh --port /dev/cu.usbmodem1101`
- `pio device monitor --port /dev/cu.usbmodem1101 --baud 115200 --no-reconnect`

Observed upload evidence:

- `Hash of data verified.`
- `Hard resetting via RTS pin...`

Observed runtime evidence with documented `MOFEI_TOUCH_PWR_ENABLE_LEVEL HIGH`:

```text
[58034] [ERR] [TOUCH] detect rejected AHT20-like 0x38 frame: 98 38 80 76 41
[61185] [INF] [TOUCH] FT6336U status=not detected SDA=13 SCL=12 addr=0x38
[228101] [ERR] [TOUCH] detect rejected AHT20-like 0x38 frame: 98 38 80 76 41
[231252] [INF] [TOUCH] FT6336U status=not detected SDA=13 SCL=12 addr=0x38
```

Observed runtime evidence after a temporary active-low power validation build (`MOFEI_TOUCH_PWR_ENABLE_LEVEL LOW`):

```text
[11165] [INF] [TOUCH] FT6336U status=not detected SDA=13 SCL=12 addr=0x38
[13016] [ERR] [TOUCH] detect rejected AHT20-like 0x38 frame: 98 38 80 76 41
```

The temporary active-low build was reverted and the documented HIGH default was flashed back successfully. The power polarity check did not make FT6336U visible on GPIO13/GPIO12.

Conclusion: the firmware-side AHT20 rejection works on real hardware and prevents false FT6336U readiness. Touch interaction remains unavailable in this firmware because the only observed device at `0x38` on GPIO13/GPIO12 returns stable AHT20-like frames. Because the original installed system did support touch, this is evidence of an incomplete or wrong firmware touch path, not evidence of absent touch hardware.

## Recovery / Continued Validation: 2026-04-29

After a diagnostic firmware attempt, the board still enumerates as:

- `/dev/cu.usbmodem1101`
- USB VID:PID `303A:1001`
- Serial `E0:72:A1:DB:0D:08`
- Description: `USB JTAG/serial debug unit`

Local checks:

- `pio run -e mofei` passed.
- `./bin/clang-format-fix -g` passed.
- `git diff --check` passed.
- `pio check -e mofei --fail-on-defect low --fail-on-defect medium --fail-on-defect high` failed on 26 existing low-style findings under `src/` and `src/activities/*`; no `lib/hal/MofeiTouch.*` finding appeared.

Device recovery checks:

- `scripts/mofei_flash_validate.sh --port /dev/cu.usbmodem1101` rebuilt `env:mofei`, then failed during upload with:

```text
Failed to connect to ESP32-S3: No serial data received.
```

- Direct esptool probe also failed:

```bash
/Users/isaac/.platformio/penv/bin/esptool --chip esp32s3 --port /dev/cu.usbmodem1101 --baud 115200 --before default-reset --after no-reset chip-id
```

```text
Failed to connect to ESP32-S3: No serial data received.
```

- `lsof /dev/cu.usbmodem1101 /dev/tty.usbmodem1101` showed no occupying process.
- `pio device list` and `ioreg` still showed the Espressif USB JTAG/serial device.

Recovery update: the board later re-enumerated as `/dev/cu.usbmodem101` with the same USB VID:PID and serial number. Production
`env:mofei` was flashed back successfully on that port, so the older `/dev/cu.usbmodem1101` upload blocker is no longer active.

## Latest Validation: 2026-04-29

Current device port:

- `/dev/cu.usbmodem101`
- USB VID:PID `303A:1001`
- Serial `E0:72:A1:DB:0D:08`
- Description: `USB JTAG/serial debug unit`

Local build checks passed:

- `git diff --check`
- `pio run -e mofei`
- `pio run -e mofei_touch_no_power_diag`
- `pio run -e mofei_touch_soft_i2c`
- `pio run -e mofei_touch_soft_i2c_power_low_diag`
- `pio run -e mofei_touch_soft_i2c_rst7_diag`

Production restore command:

```bash
scripts/mofei_flash_validate.sh --port /dev/cu.usbmodem101
```

Observed upload evidence:

```text
Connected to ESP32-S3 on /dev/cu.usbmodem101
Hash of data verified.
Hard resetting via RTS pin...
```

Short production serial sample after restore:

```text
[41178] [INF] [TOUCH] FT6336U status=not detected transport=wire SDA=13 SCL=12 addr=0x38
[43029] [ERR] [TOUCH] detect rejected AHT20-like 0x38 frame: 98 38 80 76 41
[51182] [INF] [TOUCH] FT6336U status=not detected transport=wire SDA=13 SCL=12 addr=0x38
[53033] [ERR] [TOUCH] detect rejected AHT20-like 0x38 frame: 98 38 80 76 41
```

Diagnostic envs from the same task ruled out these single-cause hypotheses:

- `mofei_touch_soft_i2c`: software I2C still read AHT20-like `0x38` data.
- `mofei_touch_no_power_diag`: builds an explicit GPIO45-disabled diagnostic by setting `MOFEI_TOUCH_PWR=-1`; this is for testing whether firmware power driving affects detection without changing the production default.
- `mofei_touch_soft_i2c_power_low_diag`: active-low `TOUCH_PWR` did not expose FT6336U.
- `mofei_touch_soft_i2c_rst7_diag`: using GPIO7 as a diagnostic reset did not expose FT6336U.

Current conclusion: the production driver no longer falsely marks the AHT20-like `0x38` responder as FT6336U, and production firmware has been restored to the device. Full touch support is still blocked on recovering the real FT6336U GPIO/init path from the original touch-capable system or a more complete Mofei board schematic. This is still not evidence that the device lacks touch hardware.

## Address Update: 2026-04-29

New user-provided working-system evidence identifies the Mofei FT6336U touch address as 7-bit `0x2E`. The firmware now
defaults production `MOFEI_TOUCH_ADDR` to `0x2E` and keeps the vendor/sample `0x38` path as `mofei_touch_addr38_diag`.

Local verification for this address update:

- `git diff --check` passed.
- `pio run -e mofei` passed after rerunning with approved PlatformIO access to `/Users/isaac/.platformio/platforms.lock`.
- `pio run -e mofei_touch_addr38_diag` passed.

This update changes the production address assumption only. It does not add production GPIO broad scanning and does not
change `HalGPIO.cpp` tap/swipe mapping.
