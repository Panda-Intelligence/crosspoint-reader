# GDEQ0426T82 (SSD1677) Virtual E-ink Panel Modeling

Status: Complete for command set + init sequence (sourced from datasheet PDF
pages 1–10). QEMU SPI-bus integration recommendation marked
`[needs verification]` and should be confirmed once web research is reachable
again.
Date: 2026-05-04

## Executive Summary

The GDEQ0426T82 panel is driven by an **SSD1677** controller (datasheet
`docs/mofei/GDEQ0426T82-T01C.pdf`, page 4). Datasheet page 10 specifies a
6-stage Power On / Render / Power Off sequence using a small, well-known
SSD16xx command vocabulary. For our simulator, we do **not** need a faithful
LUT-aware refresh waveform model — we only need to:
1. Track CS#, D/C#, BS1, RES#, BUSY pins.
2. Decode commands `0x12, 0x46, 0x47, 0x01, 0x11, 0x44, 0x45, 0x3C, 0x18, 0x22, 0x20, 0x4E, 0x4F, 0x24, 0x26, 0x10, 0x0C` plus a small handful of others.
3. Buffer writes to RAM 0x24 (B/W) into a 48000-byte framebuffer.
4. On `0x22 + 0x20` (Display Update Sequence), commit the buffer to host UI.

**Recommendation: Option C (Hybrid State Machine)** — implement the address
window + RAM counter logic faithfully, but skip LUT byte parsing and grayscale
waveform timing. Effort: ~2 days to first framebuffer rendered in canvas.

## Confirmed Hardware Facts (from datasheet pages 4–10)

| Property | Value |
|---|---|
| Driver IC | **SSD1677** |
| Resolution | 800(H) × 480(V), 1-bit B/W |
| Interface | 4-wire SPI (BS1=L) or 3-wire SPI (BS1=H) |
| SPI clock | Write: max 20 MHz; Read: max 2.5 MHz |
| Pinout | CS#, SCL, SDA, D/C#, RES#, BUSY, BS1, VCI, VDDIO, VCOM, VSH1/2, VSL, VGH, VGL, GDR, RESE, VPP |
| Logic level | VDDIO (3.3 V typical), VIH = 0.8·VDDIO |
| Update times | Full: 3.5 s; Fast: 1.5 s; Partial: 0.42 s |
| Power | VCI 2.2–3.6 V; sleep 40 µA, deep sleep 2 µA |
| Touch IC | FT6336U (separate I²C bus — see `ft6336u-i2c-modeling.md`) |

The pin description (datasheet page 6) confirms BUSY is an **output** to the
host: HIGH while the panel is busy (executing a display update or programming
OTP); the host polls BUSY low before sending more commands. The virtual panel
must drive BUSY high during simulated update intervals.

## SSD1677 Initialization Sequence (datasheet page 10)

The datasheet Stage Diagram dictates exactly:

```
1. Power On                Supply VCI, wait 10 ms
2. Set Initial Config      HW Reset (RES# LOW pulse), SW Reset by Cmd 0x12, wait 10 ms
3. Send Initialization     Cmd 0x46, Data 0xF7      (clear/fill RAM 0x24, B/W)
                           Cmd 0x47, Data 0xF7      (clear/fill RAM 0x26, Red/Old)
                           Cmd 0x01                 (set gate driver output)
                           Cmd 0x11                 (set data entry mode — RAM increment direction)
                           Cmd 0x44                 (set RAM X start/end)
                           Cmd 0x45                 (set RAM Y start/end)
                           Cmd 0x3C                 (border waveform)
4. Load Waveform LUT       Cmd 0x18                 (sense temperature int/ext)
                           Cmd 0x22                 (display update control 2)
                           Cmd 0x20                 (master activation — load LUT from OTP)
                           Wait BUSY low
5. Write Image / Drive     Cmd 0x4E                 (set RAM X address counter)
                           Cmd 0x4F                 (set RAM Y address counter)
                           Cmd 0x24                 (write RAM B/W)
                           Cmd 0x26                 (write RAM Red/Old) — optional for B/W panels
                           Cmd 0x0C                 (set softstart)
                           Cmd 0x22                 (display update control 2 — pattern selects which phases)
                           Cmd 0x20                 (master activation — execute display update)
                           Wait BUSY low
6. Power Off               Cmd 0x10                 (deep sleep)
```

## Command Vocabulary — MUST/SHOULD/MAY for Virtual Panel

| Cmd | Name | Class | Virtual panel behavior |
|---|---|---|---|
| `0x12` | SW_RESET | MUST | Clear RAM, reset address counters, drive BUSY high briefly. |
| `0x10` | DEEP_SLEEP | MUST | Stop accepting commands until next HW reset. |
| `0x11` | DATA_ENTRY_MODE | MUST | One byte: bits[2:0] selects X/Y increment/decrement and address-update direction. Affects how subsequent `0x24` writes increment counters. |
| `0x44` | RAM_X_START_END | MUST | Two bytes: X start (in 8-pixel units), X end. |
| `0x45` | RAM_Y_START_END | MUST | Four bytes: Y start lo, Y start hi, Y end lo, Y end hi. |
| `0x4E` | RAM_X_COUNTER | MUST | Set X write pointer. |
| `0x4F` | RAM_Y_COUNTER | MUST | Set Y write pointer (2 bytes). |
| `0x24` | WRITE_RAM_BW | MUST | All subsequent SPI bytes (until next command) are RAM data, written at the current X/Y counter following the data-entry-mode rules. |
| `0x22` | DISPLAY_UPDATE_CONTROL_2 | MUST | One byte selects which sub-phases run on next `0x20`. We can treat as opaque. |
| `0x20` | MASTER_ACTIVATION | MUST | **Trigger framebuffer commit to host UI**. Drive BUSY high for a simulated refresh duration (full=3.5 s, fast=1.5 s, partial=0.42 s, or scaled-down for dev speed), then release BUSY. |
| `0x01` | DRIVER_OUTPUT_CTL | SHOULD | Three bytes: MUX (lines), gate scan order. Affects effective rendered height. |
| `0x3C` | BORDER_WAVEFORM | SHOULD | One byte border setting. Cosmetic only, can ignore. |
| `0x46` | AUTO_WRITE_RED_RAM | SHOULD | Bulk-fill 0x26 RAM with one pattern byte. |
| `0x47` | AUTO_WRITE_BW_RAM | SHOULD | Bulk-fill 0x24 RAM with one pattern byte. (Note: vendor docs sometimes swap 0x46/0x47 mappings — confirm against firmware traffic during M2.) |
| `0x18` | TEMPERATURE_SENSOR_CTL | SHOULD | One byte: `0x80`=internal, `0x48`=external I²C. We can return canned 25 °C. |
| `0x0C` | SOFT_START_CTL | MAY | Four-byte SS pattern. Ignore. |
| `0x21` | DISPLAY_UPDATE_CONTROL_1 | MAY | RAM source select. Optional. |
| `0x26` | WRITE_RAM_RED | MAY | Same as 0x24 but for the second buffer. For pure B/W we can route to a parallel buffer or ignore. |
| `0x32` | WRITE_LUT_REGISTER | MAY | LUT bytes. **Skip parsing** — just consume the 153 bytes following. |
| `0x37` | OTP_READ_OPTIONS | MAY | Skip. |
| `0x3F` | UNKNOWN_BLE | MAY | Vendor demo issues this — empty handler. |

Anything not on this list: log + ignore in development build, fail in CI build
(to surface untested commands).

## D/C# Pin Handling in QEMU

`[needs web verification]` Conventional pattern (from training-knowledge):

The D/C# pin is a host-driven GPIO. Virtual peripheral must:
1. Subscribe to the GPIO state of D/C# via a `qemu_irq` line bound to a slot
   in our peripheral struct.
2. On each SPI byte arrival (via `SSISlave::transfer` callback), branch on
   the *current* D/C# level: HIGH = data, LOW = command.
3. Track CS# similarly — when CS# rises, finalize any in-progress multi-byte
   command parameter sequence.

Existing emulators that use this pattern (training-knowledge, verify):
- Wokwi's `wokwi-ssd1306` and `wokwi-epaper` components — exposed via custom-
  chips C API (`__attribute__((weak))` callbacks for SPI/GPIO/timer events).
- Renode's `Antmicro.Renode.Peripherals.SPI.GoodDisplay_GDE...` model class.
- QEMU upstream `hw/display/ssd0303.c` — small OLED, illustrates the
  command/data mode switch via a member variable `s->mode`.

## Output Buffer Format

Recommended layout for direct Canvas2D `ImageData` blit:

```
Layout:    row-major, MSB-first within byte
Stride:    100 bytes per row (800 / 8)
Total:     48000 bytes (800 × 480 / 8)
Sense:     bit=1 → white pixel, bit=0 → black pixel  (SSD16xx convention)

To Canvas2D ImageData (RGBA8):
  for each byte b at offset (row*100 + col_byte):
    for bit i in 7..0:
      pixel = (b >> i) & 1
      rgba = pixel ? 0xFFFFFFFF : 0xFF000000
      write to imageData.data[(row*800 + col_byte*8 + (7-i))*4 .. +3]
```

Memory cost: 48 KB raw + 1.5 MB RGBA temp on JS side. Trivial on a desktop.

## Refresh Timing Model (Optional, Recommended for Visual Fidelity)

Simulating BUSY-high during refresh has two values:
1. The firmware blocks on `wait_busy_low()` — if we never raise BUSY, we
   risk the firmware racing into the next frame faster than reality, masking
   real-device timing bugs.
2. The flicker / refresh visual is part of the e-ink character.

Suggested defaults (configurable via env var):

| Update type | Real | Simulator default | Toggle |
|---|---|---|---|
| Full | 3.5 s | 0.5 s | `MOFEI_SIM_FAST_REFRESH=1` → 0 s |
| Fast | 1.5 s | 0.2 s | |
| Partial | 0.42 s | 0.05 s | |

Determine type from the `0x22` parameter byte: full = `0xF7`, fast = `0xC7`,
partial = `0xFF`/`0xCF` (varies; trace the firmware to confirm during M2).

## Modeling Depth Options

### Option A: SPI Byte Tap

- Just buffer all bytes between CS# falling and CS# rising into a list.
- On each Master Activation (`0x22 0x20`), scan the buffer for the most
  recent `0x24` command and assume the bytes after it form the framebuffer.

**Pros**: Implementation is ~200 lines.

**Cons**: Brittle to command ordering; no support for partial updates,
windowed writes, or grayscale red buffer; cannot detect address-counter
direction issues.

**Effort**: 0.5 day.

### Option B: Faithful FSM

- Full SSD1677 state machine: address window, RAM pointer, data-entry mode,
  LUT bytes parsed and stored, all 25+ commands modeled.

**Pros**: Will accept any future SSD1677-class panel firmware.

**Cons**: ~3× the work; LUT byte semantics are mostly waveform-related and
add no value to a 1bpp B/W simulator.

**Effort**: 4–5 days.

### Option C: Hybrid (Recommended)

- Full FSM for: command/data dispatch, address window, RAM X/Y counter, data
  entry mode, BUSY high/low timing on `0x20`.
- Skip-and-consume for: LUT bytes (`0x32`), softstart (`0x0C`), border
  (`0x3C`), temperature sensor (`0x18` returns canned 25 °C).
- Single 800×480 1bpp framebuffer; treat `0x26` writes as "red" buffer
  ignored or composited with `0x24` (depending on what firmware actually
  writes there — for B/W panels typically nothing).

**Pros**: Correct enough for any 1bpp render pipeline; rejects malformed
command sequences (catches firmware bugs); leaves a clean upgrade path to
LUT modeling later.

**Cons**: Needs a small state machine. Effort ~2 days.

**Effort**: 2 days for first frame, +1 day for partial-update support.

## Recommendation

**Option C (Hybrid)**. The firmware is doing real RAM-window writes (page 10
of datasheet shows `0x44`/`0x45` at line 7; firmware sets a window before
every render). Tapping bytes blindly will silently corrupt partial updates
and any future region-based UI. The FSM cost is moderate and amortizes well.

## Citations

- On-disk: `docs/mofei/GDEQ0426T82-T01C.pdf` (pages 1–10 read; 11–23 not yet
  inspected, may contain command parameter detail)
- On-disk: `docs/mofei/GDEQ0426T82-T01C_Arduino/Ap_29demo.h` (vendor bitmap
  fixture)
- On-disk: `docs/mofei/GDEQ0426T82-T01C_Arduino/GDEQ0426T82-T01C_Arduino.ino`
  (vendor init code — not yet inspected)
- `[needs verification]` Wokwi epaper component, Renode display peripheral
  base class, QEMU `hw/display/ssd0303.c`.
