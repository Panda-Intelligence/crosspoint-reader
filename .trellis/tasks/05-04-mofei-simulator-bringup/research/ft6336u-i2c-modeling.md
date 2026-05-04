# FT6336U I²C Virtual Peripheral Modeling

Status: Complete (sourced from on-disk vendor code + our `lib/hal/MofeiTouch.cpp`).
Date: 2026-05-04

## Executive Summary

A minimum-viable virtual FT6336U for our simulator only needs to satisfy
`MofeiTouchDriver::detectOnPins()` + `configureController()` + `readPoint()`. The
driver reads exactly 11 bytes starting from `TD_STATUS` (0x02) per touch poll,
plus 3 register writes during init and ~4 single-byte reads for the diagnostic
path. Everything else can return zeros. **Recommendation: implement Option A
(Minimal Register Machine)**, deferring the full FT6336U register file until
we ever ship to a tester who cares about firmware-ID display strings. Effort
estimate: ~1 day to first valid frame round-trip.

## Locked Hardware Contract

From `.trellis/tasks/04-29-mofei-touch-support/prd.md` and `lib/hal/MofeiTouch.h`:

| Property | Value |
|---|---|
| 7-bit I²C address | `0x2E` (production) — virtual peripheral MUST respond at this address |
| Bus pins | SDA=GPIO13, SCL=GPIO12 |
| Power pin | GPIO45, enable level = LOW (`MOFEI_TOUCH_PWR_ENABLE_LEVEL`) |
| Reset pin | GPIO7, active LOW, hold 50 ms, settle 100 ms |
| INT pin | GPIO44, active LOW, INPUT_PULLUP on host |
| Bus speed | 400 kHz (`MOFEI_TOUCH_I2C_FREQ`) |
| Default transport | Software-bitbanged I²C (`MOFEI_TOUCH_SOFT_I2C=1`) — virtual GPIO must support bit-bang timing |
| Logical/raw geometry | Width 480, Height 800 (portrait) |
| Max points firmware accepts | 2 (`FT6336_MAX_POINTS`) |

## Register Map — What the Firmware Actually Touches

Reverse-engineered from `lib/hal/MofeiTouch.cpp` constants and access sites:

| Address | Symbol | RW | Role | Firmware behavior |
|---|---|---|---|---|
| `0x00` | `DEVICE_MODE` | RW | Mode select (0=working, 1=factory test) | Init: write `0x00`. Diagnostic: read back. Config validated to equal `0x00` to be considered ready. |
| `0x01` | `GESTURE_ID` | R | Hardware gesture decode | Diagnostic-only read (`logDiagnosticSample`). Not used to drive events. Can return `0x00`. |
| `0x02` | `TD_STATUS` | R | Touch detect status. Low nibble = point count, high nibble = reserved (must be 0) | **CRITICAL**: read every poll as the start of an 11-byte burst (`FT6336_FRAME_LEN=11`). Frame validity: `(data[0] & 0xF0) == 0` AND `points <= 2`. |
| `0x03` | `P1_XH` (event[7:6] \| reserved \| X[11:8]) | R | Touch1 event flag + X high nibble | bits 7:6 = event (0=put-down, 1=put-up, 2=contact, 3=reserved/invalid). Firmware rejects if event==3 (`FT6336_EVENT_RESERVED`). |
| `0x04` | `P1_XL` | R | Touch1 X low byte | combined: rawX = ((P1_XH & 0x0F) << 8) \| P1_XL |
| `0x05` | `P1_YH` (touch_id[7:4] \| Y[11:8]) | R | Touch1 ID + Y high nibble | rawY = ((P1_YH & 0x0F) << 8) \| P1_YL |
| `0x06` | `P1_YL` | R | Touch1 Y low byte | |
| `0x07` | `P1_WEIGHT` | R | Touch1 pressure | Read in burst, ignored. Return `0x00`. |
| `0x08` | `P1_MISC` (area[7:4] \| reserved) | R | Touch1 area | Read in burst, ignored. |
| `0x09–0x0E` | `P2_*` | R | Touch2 mirror of `0x03..0x08` | Only consumed when `points == 2`. |
| `0x80` | `THGROUP` | RW | Touch threshold | Init: write `22` (`FT6336_THGROUP_DEFAULT`). Read back, must equal `22`. |
| `0x88` | `PERIODACTIVE` | RW | Active-state report period | Init: write `14` (`FT6336_PERIODACTIVE_DEFAULT`). Read back, must equal `14`. |
| `0xA6` | `FIRMWARE_ID` | R | Firmware version | Diagnostic-only single-byte read. Suggested response: `0x01` (any non-`0xFF`). |
| `0xA8` | `VENDOR_ID` | R | Chip vendor ID (FocalTech `0x11`) | Diagnostic-only. Suggested response: `0x11`. |

**Frame layout for the 11-byte burst from `0x02`** (this is the hot path):

```
offset  reg    name
  0    0x02   TD_STATUS        (low nibble = point count, high nibble = 0)
  1    0x03   P1_XH/EVT
  2    0x04   P1_XL
  3    0x05   P1_YH/ID
  4    0x06   P1_YL
  5    0x07   P1_WEIGHT
  6    0x08   P1_MISC
  7    0x09   P2_XH/EVT
  8    0x0A   P2_XL
  9    0x0B   P2_YH/ID
  10   0x0C   P2_YL    (firmware reads only 11 bytes, so P2_WEIGHT/MISC at 0x0D/0x0E are NOT read)
```

(Note: `MofeiTouch.cpp` decodes touch1 from `data[1..4]`, touch2 from `data[7..10]`. The
"`FT6336_TOUCH2_OFFSET=7`" constant confirms the 11-byte stride.)

## Init Sequence the Firmware Issues

From `MofeiTouchDriver::begin()` + `configureController()` + `detectOnPins()`:

```
Host actions (during begin)
  1. Drive PWR (GPIO45) LOW                        ← 1 second settle
  2. INT (GPIO44) configured INPUT_PULLUP
  3. RST (GPIO7) LOW for 50 ms, then HIGH, then 100 ms settle
  4. (optional) MOFEI_TOUCH_SCAN diagnostic — probes 0x2E and 0x38 with bit-bang ACK

Detect phase (`detectOnPins`)
  5. Bit-bang I²C bus init at SDA=13, SCL=12
  6. Read 11 bytes from reg 0x02
     → must satisfy validateFt6336Frame() (first byte high nibble == 0, points ≤ 2)

Configure phase (`configureController`)
  7. Write reg 0x00 (DEVICE_MODE) = 0x00
  8. Write reg 0x80 (THGROUP) = 22 (0x16)
  9. Write reg 0x88 (PERIODACTIVE) = 14 (0x0E)
 10. Read back 0x00, 0x80, 0x88, 0xA6, AND 11 bytes from 0x02
 11. Validate readbacks equal what was written. If not, the driver marks itself unavailable.
```

Once `ready=true`, the runtime path is just:

```
Steady state (`update`/`readPoint`, ~100 Hz when active, ~10 Hz when idle)
   - Poll only when (touchDown OR INT==LOW OR elapsed ≥ MOFEI_TOUCH_POLL_INTERVAL_MS=10ms)
   - Burst-read 11 bytes from reg 0x02
   - Decode TD_STATUS, then P1 (and optionally P2)
```

## INT Pin Behavior Contract

Firmware (`MofeiTouchDriver::update`):
- Treats INT as **active-low level** (`digitalRead(MOFEI_TOUCH_INT) == LOW` ⇒ data ready).
- Falls back to time-based polling (10 ms cadence) if INT is not asserted.
- No edge detection, no ISR — pure polled.
- `INPUT_PULLUP` on host means the virtual peripheral must drive **strong LOW**
  during touch-active and **release (high-Z, host pull-up wins)** when idle.

Implication for the virtual peripheral: when a touch event arrives from the
host UI, **assert INT low** and **populate the 11-byte frame buffer**, then
hold INT low until the firmware reads `TD_STATUS` (i.e., until SCL/SDA traffic
shows the read transaction completing). Release INT after the read or after
~50 ms timeout (whichever first).

For PUT-UP transitions: send one frame with `TD_STATUS=0x01` and event=put-up
in `P1_XH`, then on the *next* poll send `TD_STATUS=0x00`. The firmware
distinguishes "no points" (`points==0` ⇒ `released=true` if `touchDown`) from
the explicit put-up event flag.

## Edge Cases the Firmware Checks

(From `MofeiTouch.cpp::validateFt6336Frame` and `validateFt6336Point`.)

1. **High nibble of TD_STATUS must be zero**. Any bit set in `0xF0` is rejected.
2. **Point count > 2 is rejected** as invalid.
3. **AHT20 collision detection** at addr 0x38: if `(data[0] & 0x18) == 0x18` OR
   `(data[0] & 0x80) != 0`, AND the frame is otherwise invalid, firmware logs
   "wrong-device/AHT20-like frame" and refuses to mark the driver ready. Our
   virtual peripheral at `0x2E` MUST NOT emit this signature, but if we want to
   model the production `0x38` recovery path we can intentionally emit
   `98 38 80 76 41` as a "fake AHT20" feature flag.
4. **Per-point event code 3 is rejected** (reserved value).
5. **Coordinates out of `MOFEI_TOUCH_WIDTH × MOFEI_TOUCH_HEIGHT` (or the swapped
   alt geometry) are rejected**.

## QEMU I²C Slave Device API (Reference)

`[needs web verification — written from training knowledge]`

QEMU upstream models I²C devices as `I2CSlave` subclasses living on an
`I2CBus`. Key surface:

```c
struct I2CSlaveClass {
    DeviceClass parent_class;
    int (*event)(I2CSlave *s, enum i2c_event event); // START/STOP/NACK
    uint8_t (*recv)(I2CSlave *s);   // master reading from us
    int (*send)(I2CSlave *s, uint8_t data); // master writing to us
};
```

Existing register-machine examples in upstream QEMU to crib from:
- `hw/misc/tmp105.c` — TMP105 thermal sensor, single-register pointer + RW.
- `hw/i2c/imx_i2c.c` — controller side, but illustrates address matching.
- `hw/sensor/lsm303dlhc_mag.c` — multi-register sensor.

For us, the implementation skeleton is:

```c
typedef struct {
    I2CSlave parent;
    uint8_t reg_ptr;        // last register address written
    bool writing_reg_ptr;   // true if next byte is the address
    uint8_t reg[256];       // sparse register file
    uint16_t pending_x[2], pending_y[2];
    uint8_t pending_evt[2], pending_count;
    qemu_irq int_pin;       // GPIO line to drive INT
    CharBackend chr;        // chardev for IPC with host UI
} FT6336State;
```

The bit-bang transport in `MofeiTouchDriver` (`MOFEI_TOUCH_SOFT_I2C=1`) means
**we do NOT need ESP32 I²C peripheral emulation in QEMU** — we just need the
GPIO matrix to faithfully toggle SDA/SCL pins, and we wire a **virtual I²C
slave on raw GPIO** rather than on the SoC's I²C controller. That's a much
simpler integration: an I²C-protocol parser sitting on two `qemu_irq` lines.

If the firmware later switches to hardware Wire (`MOFEI_TOUCH_SOFT_I2C=0`),
we'd need actual ESP32-S3 `I2C0` controller emulation, which Espressif's QEMU
fork has been adding as of late 2024. Defer until needed.

## Modeling Depth Options

### Option A: Minimal Register Machine (Recommended)

- Register file: only `0x00`, `0x01`, `0x02..0x0E`, `0x80`, `0x88`, `0xA6`, `0xA8`.
- All other addresses return `0x00` on read, no-op on write.
- Frame buffer (`pending_*`) populated by host-injected touch events via chardev.
- INT pin asserted low when frame buffer non-empty.

**Pros**: Trivial to implement. Maps 1:1 to firmware's actual register reads.
Anything we got wrong would surface as a logged frame validation error, easy
to debug.

**Cons**: If a future firmware change reads other diagnostic registers, returns
of `0x00` may confuse it.

**Effort**: ~0.5 day register machine, ~0.5 day INT/IRQ wiring, ~0.5 day
chardev bridge for events. **Total: 1–2 days**.

### Option B: Faithful Register File

- Implement the full FT6336U register set (DEVICE_MODE, RAW_X/Y, FACTORY,
  CIPHER, etc.) as documented in `D-FT6336U-DataSheet-V1.0.pdf`.
- Implement gesture-engine state transitions.

**Pros**: Drop-in replacement for any FT6336U firmware, including future ones.

**Cons**: ~5× the work for ~0× current value.

**Effort**: ~5–7 days.

## Recommendation

**Option A**. The firmware is the contract; nothing else matters. Total of
~9 well-known register addresses, plus an 11-byte burst buffer that's just a
2-touch state struct serialized.

## Citations

- On-disk: `lib/hal/MofeiTouch.cpp`, `lib/hal/MofeiTouch.h`
- On-disk: `docs/mofei/GDEQ0426T82-T01C_Arduino/FT6336.cpp`, `FT6336.h`
- On-disk: `docs/mofei/D-FT6336U-DataSheet-V1.0.pdf`
- On-disk: `.trellis/tasks/04-29-mofei-touch-support/prd.md`
- QEMU upstream `hw/misc/tmp105.c` (training-knowledge reference, verify after
  reconnect)
