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

### Preserve Native Touch Event Ownership

Mofei touch input must cross the HAL boundary as the generic
`InputTouchEvent` contract, not as `MofeiTouchDriver::Event` or FT6336U
register data.

#### 1. Scope / Trigger

Use this rule whenever a screen handles taps, swipes, hit-testing, or button
hint taps from touch input. It prevents the same physical touch from being
processed twice: once as a native coordinate event and again as the legacy
touch-to-button fallback.

#### 2. Signatures

```cpp
struct InputTouchEvent {
  enum class Type : uint8_t { None, Tap, SwipeLeft, SwipeRight, SwipeUp, SwipeDown };
  Type type;
  uint16_t x;
  uint16_t y;
  bool isTap() const;
};

bool MappedInputManager::consumeTouchEvent(InputTouchEvent* outEvent) const;
void MappedInputManager::suppressTouchButtonFallback() const;
bool MappedInputManager::isTouchButtonHintTap(const InputTouchEvent& event) const;
```

#### 3. Contracts

- HAL/input code converts chip-specific events into `InputTouchEvent`.
- Activity code may branch on generic touch types and shared hit-test helpers.
- Activity code must not include `MofeiTouch.h` or inspect FT6336U registers.
- A native consumer owns a touch after `consumeTouchEvent()` returns `true`.
- If the screen handles that touch natively, it must call
  `suppressTouchButtonFallback()` before any later `wasPressed()` or
  `wasReleased()` checks in the same loop.
- Button-hint/footer taps may intentionally be left to fallback by checking
  `isTouchButtonHintTap()` and not suppressing them.

#### 4. Validation & Error Matrix

| Condition | Required behavior |
| --- | --- |
| Native screen handles tap or swipe | Suppress fallback and return or avoid button checks for that event |
| Touch is a button-hint tap | Let fallback run unless the screen handles the hint itself |
| Screen only supports buttons | Do not consume the touch; HAL fallback keeps buttons working |
| Reader orientation differs from touch orientation | Transform through shared helpers before hit-testing |

#### 5. Good/Base/Bad Cases

- Good: `EpubReaderActivity` consumes a touch, transforms it with
  `TouchHitTest::eventForRendererOrientation()`, then suppresses fallback.
- Base: a button-only screen ignores `consumeTouchEvent()` and relies on the
  existing virtual button adapter.
- Bad: an activity consumes a tap, opens a menu, then lets the same tap also
  reach `wasReleased(Button::Confirm)`.

#### 6. Tests Required

- Run `pio run -e mofei` for compile coverage.
- Run `pio check -e mofei --fail-on-defect low --fail-on-defect medium --fail-on-defect high`
  after broad input changes.
- On device, flash `env:mofei` and confirm serial logs show
  `FT6336U status=ready ... addr=0x2E`.

#### 7. Wrong vs Correct

Wrong:

```cpp
MofeiTouchDriver::Event event;
if (touch.update(&event) && event.type == MofeiTouchDriver::EventType::Tap) {
  openItem(event.x, event.y);
}
if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
  openSelectedItem();
}
```

Correct:

```cpp
InputTouchEvent event;
if (mappedInput.consumeTouchEvent(&event)) {
  const bool buttonHintTap = mappedInput.isTouchButtonHintTap(event);
  if (!buttonHintTap && event.isTap()) {
    mappedInput.suppressTouchButtonFallback();
    openItem(event.x, event.y);
    return;
  }
}
if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
  openSelectedItem();
}
```

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

When a Mofei device is attached, build-only is not enough. Always follow with
a flash test on the actual USB port before calling the slice verified:

```bash
scripts/mofei_flash_validate.sh --port /dev/cu.usbmodemXXXX
```

For touch, display, input, boot, power, font, i18n, or UI rendering changes,
also inspect serial logs or perform the relevant manual screen interaction
after flashing. If flashing is impossible, record the exact upload/esptool
error and the physical recovery step needed before claiming verification.

---

## Code Review Checklist

<!-- What reviewers should check -->

(To be filled by the team)
