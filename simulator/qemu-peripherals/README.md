# QEMU peripherals — Mofei simulator

C source for the custom QEMU virtual peripherals that present Mofei's e-ink
panel and capacitive touch controller to the guest firmware.

## Files

| File | Channel | Status |
|---|---|---|
| `ssd1677_gdeq0426t82.c` | SPI slave (`hw/ssi/ssi.h`) | PR2 — committed, **not yet wired into QEMU build** |
| `ft6336u.c` | I²C slave / GPIO bit-bang | PR3 (not yet started) |

## Integration model

These peripherals attach into the espressif/qemu fork's tree as out-of-tree
sources patched into the build. The exact patch goes into:

- A `meson.build` fragment under `qemu/hw/display/` and `qemu/hw/sensor/`
  registering each `.c` file
- A `Kconfig` entry to allow the device to be selected
- A wiring patch in the ESP32-S3 SoC machine file (`qemu/hw/xtensa/esp32s3.c`
  or similar) instantiating each peripheral on the right SPI/I²C bus

The wiring patch is **not yet written**. Reasons:

1. The exact SoC machine file path inside the espressif fork needs
   verification — see `.trellis/tasks/05-04-mofei-simulator-bringup/research/verification-pending.md`
2. The Mofei firmware's actual SPI controller assignment (SPI2 vs SPI3) and
   GPIO pin mapping for D/C#, CS#, RES#, BUSY needs cross-checking against
   `lib/hal/HalDisplay*` and `open-x4-sdk/src/EInkDisplay/`.

PR2 ends with the C source in place plus the host-side decoder (Tauri canvas)
ready. The integration patch lands as part of the verification clearance pass
once the QEMU build is up.

## How it talks to the host UI

Each peripheral opens its own QEMU `chardev` backend. On the Tauri side, a
single Unix-domain socket is shared (multiplexed by `channel:u8` header
byte). See `simulator/src-tauri/src/ipc.rs` for the parser and
`research/qemu-peripheral-ipc.md` for the protocol design.

`ssd1677_gdeq0426t82.c` writes channel `0x01 framebuffer` (48000 bytes
per full update). `ft6336u.c` (PR3) reads channel `0x04 touch-event`
(inbound from UI) and writes nothing (interrupt is a GPIO line, not a chardev
message).

## Manual test checklist (post-integration)

- [ ] HW reset (RES# low pulse) clears framebuffer to all-white (0xFF).
- [ ] SW reset (cmd 0x12) does the same and briefly drives BUSY high.
- [ ] Cmd 0x11 (data entry mode) → cmd 0x44/0x45 (RAM window) → cmd
      0x4E/0x4F (counter) → cmd 0x24 + N data bytes correctly populates
      `fb[]` at `(row * 100) + col_byte`.
- [ ] Cmd 0x47 (auto-write BW) with arg 0x55 produces a striped framebuffer.
- [ ] Cmd 0x22 + arg 0xF7 followed by cmd 0x20 emits a full framebuffer to
      the chardev and holds BUSY high for 500 ms.
- [ ] Wokwi-style hand-crafted firmware that draws a checkerboard renders
      on the Tauri canvas at 1:1 scale.
