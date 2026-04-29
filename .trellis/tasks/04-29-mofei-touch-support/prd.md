# Mofei Touch Support

Date: 2026-04-29
Owner: Codex

## Problem

Mofei firmware previously initialized the FT6336U touch controller on GPIO13/GPIO12 at I2C address `0x38`, with guessed power and interrupt pins. Device observations showed reads from address `0x38` returning stable bytes like `98 38 80 76 41`. That payload matches an AHT20-style sensor frame and decodes as an invalid FT6336U touch point count. The result was that taps and swipes remained ineffective, and the firmware could mark the wrong I2C device as a ready touch controller.

User-provided working-system evidence now identifies the Mofei FT6336U touch address as 7-bit `0x2E`. Production firmware must default `MOFEI_TOUCH_ADDR` to `0x2E`; the vendor/sample `0x38` address is diagnostic-only unless later Mofei-specific evidence overrides the working-system report.

User correction: a previously installed system on this same device supported touch. Therefore the hardware should be treated as touch-capable. The current firmware evidence only proves that this firmware path is reading the wrong `0x38` device or missing required board-specific touch initialization; it does not prove the board lacks touch hardware.

## Evidence

- `docs/mofei/ic.png` maps ESP32-S3R8 nets:
  - `SDA` -> GPIO13
  - `SCL` -> GPIO12
  - `TOUCH_PWR` -> GPIO45
  - `TOUCH_INT` -> GPIO44
- `docs/mofei/ic.png` labels GPIO13/GPIO12 as generic `SDA`/`SCL` nets. It does not, by itself, prove those nets are the FT6336U touch FPC lines.
- `docs/mofei/GDEQ0426T82-T01C_Arduino/i2c.h` maps the vendor FT6336U sample to Arduino pins:
  - `A4` as touch SDA
  - `A5` as touch SCL
  - `A3` as touch reset
- `docs/mofei/GDEQ0426T82-T01C_Arduino/GDEQ0426T82-T01C_Arduino.ino` maps `A0` as the touch IRQ input.
- Under the current PlatformIO `esp32s3` Arduino variant, `A0/A3/A4/A5` map to GPIO1/GPIO4/GPIO5/GPIO6. On this Mofei schematic those GPIOs are already used by keys and EPD SPI control nets, so the vendor sample labels cannot be directly used as Mofei GPIO mapping.
- The vendor sample bit-bangs FT6336U I2C and resets the controller before configuring `DEVICE_MODE`, `THGROUP`, and `PERIODACTIVE`.
- The current implementation in `lib/hal/MofeiTouch.*` already has a dedicated Mofei touch driver and uses hardware `Wire`.
- The vendor Arduino sample uses 8-bit `FT_CMD_WR 0x70` / `FT_CMD_RD 0x71`, equivalent to 7-bit `0x38`, but observed Mofei hardware returned AHT20-like frames at `0x38`; this is not the production address default.

## Requirements

1. Keep the documented Mofei touch bus defaults aligned with `ic.png`: SDA GPIO13, SCL GPIO12, power GPIO45, interrupt GPIO44.
2. Add the documented reset GPIO if it can be derived from the Arduino sample and existing ESP32-S3 pin mapping. Do not guess if the mapping cannot be proven.
3. The driver must use `MOFEI_TOUCH_ADDR` as the active FT6336U address source and default production builds to 7-bit `0x2E`.
4. Reject clearly invalid FT6336U frames, especially point counts above 2 and coordinate values outside the documented touch geometry.
5. Detect and log the likely AHT20 collision/signature when the active address returns AHT20-like status/data instead of FT6336U registers, without implying `0x38` is the production FT6336U address.
6. Preserve existing tap/swipe-to-button mapping in `HalGPIO.cpp`; fix the touch controller path, not the UI/button policy.
7. Keep changes narrowly scoped to Mofei touch support and the task context/docs needed to explain it.
8. Do not conclude that the hardware lacks touch support. Since the original system supported touch, unresolved evidence must be framed as missing firmware GPIO/init path.

## Non-Goals

- Do not rewrite reader UI navigation.
- Do not add a new touch library unless the existing driver cannot support the documented FT6336U protocol.
- Do not change display orientation or Mofei display write order.
- Do not invent unverified GPIO mappings.
- Do not run broad arbitrary GPIO I2C scans in production firmware.

## Acceptance Criteria

- `MofeiTouchDriver::begin()` leaves `available()` false when the active address responds with AHT20-like data or invalid FT6336U status.
- Production `env:mofei` defaults to `MOFEI_TOUCH_ADDR=0x2E`; any `0x38` build is clearly diagnostic-only.
- A valid FT6336U frame with 1 or 2 points is still accepted and translated into tap/swipe events.
- Invalid point counts do not produce touch events and do not keep the driver in a false-ready state.
- Build verification passes for the Mofei environment.
- Static analysis or a narrow compile gate is run locally; if full `pio check` is blocked by existing repository issues, record the exact boundary.
- If the device cannot be flashed, record the exact esptool error and the physical recovery step required before claiming any on-device touch result.

## Suggested Verification

- `scripts/mofei_flash_validate.sh --build-only`
- If build-only is green and a device port is available, flash with `scripts/mofei_flash_validate.sh --port <port>` and inspect serial logs for touch detection.
