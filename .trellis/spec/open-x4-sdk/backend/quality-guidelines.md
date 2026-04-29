# Quality Guidelines

> Code quality standards for backend development.

---

## Overview

<!--
Document your project's quality standards here.

Questions to answer:
- What patterns are forbidden?
- What linting rules do you enforce?
- What are your testing requirements?
- What code review standards apply?
-->

(To be filled by the team)

---

## Forbidden Patterns

<!-- Patterns that should never be used and why -->

### Do Not Trust I2C Address ACKs As Device Identity

An I2C ACK or successful register read only proves that something answered on
the bus. It is not enough to identify a chip. Mofei touch support is the
current example: the working-system evidence points production Mofei FT6336U
traffic at 7-bit `0x2E`, while the vendor/sample `0x38` path can return
AHT20-style frames on the same board bus.

Wrong:

```cpp
uint8_t status = 0;
if (readFt6336(FT6336_REG_TD_STATUS, &status, 1)) {
  ready = true;
}
```

Correct:

```cpp
uint8_t data[FT6336_FRAME_LEN] = {};
const bool readOk = readRegister(FT6336_REG_TD_STATUS, data, sizeof(data));
ready = readOk && validateFt6336Frame(data, sizeof(data), "detect") == Ft6336FrameStatus::Valid;
```

For FT6336U specifically, validate at least these contract points before
marking the controller ready:

- `TD_STATUS` high nibble must be zero.
- point count must be `0..2`.
- touch point event code must not be reserved.
- raw coordinates must fit the configured touch geometry.
- AHT20-like frames such as `98 38 80 76 41` must be rejected and logged as
  the wrong device at the active `MOFEI_TOUCH_ADDR`, not parsed as touch data.

For Mofei production builds, the active FT6336U address must come from
`MOFEI_TOUCH_ADDR` and default to the working-system 7-bit address `0x2E`.
The vendor/sample `0x38` address is allowed only through explicit diagnostic
build flags or environments.

---

## Required Patterns

<!-- Patterns that must always be used -->

### Treat Mofei GPIO45 As A Sensitive Strap-Capable Output

For Mofei touch power, GPIO45 is documented by `docs/mofei/ic.png` as
`TOUCH_PWR`. The local ESP32-S3 datasheet (`docs/mofei/esp32-s3_datasheet_en.pdf`)
shows GPIO45 is `I/O/T` and can be used as regular IO after reset, but it is
also the `VDD_SPI` voltage strapping pin sampled at chip reset.

Do not mark GPIO45 unusable as a normal output based on an input-only
assumption. Keep production behavior aligned with the schematic unless direct
hardware evidence disproves it. Any experiment that disables or changes GPIO45
power driving must use an explicit diagnostic build flag or environment, and
must not become a broad GPIO scan in production firmware.

---

## Testing Requirements

<!-- What level of testing is expected -->

For Mofei firmware driver changes, run the narrow build gate:

```bash
scripts/mofei_flash_validate.sh --build-only
```

When a device is attached, follow with a flash and serial-log check on the
actual USB port.

---

## Code Review Checklist

<!-- What reviewers should check -->

(To be filled by the team)
