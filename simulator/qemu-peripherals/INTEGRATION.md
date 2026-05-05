# Mofei simulator — QEMU integration patches

This directory contains stub files showing **how** the custom Mofei
peripherals are intended to be wired into the espressif/qemu fork. The
actual integration (copying into the cloned `.qemu-cache/qemu/` tree and
patching `meson.build` / `Kconfig` / SoC machine source) is performed by
`simulator/scripts/integrate-peripherals.sh` (committed in this PR but
**not yet executed end-to-end** — see PR2/PR3 status notes).

## What each file represents

| File | Purpose |
|---|---|
| `display-meson.fragment` | Meson `system_ss.add(...)` line for `hw/display/meson.build` registering the SSD1677 source |
| `display-Kconfig.fragment` | `config SSD1677_GDEQ0426` block for `hw/display/Kconfig` |
| `i2c-meson.fragment` | Meson registration for the FT6336U source under `hw/sensor/` |
| `sensor-Kconfig.fragment` | `config FT6336U_MOFEI` block for `hw/sensor/Kconfig` |
| `xtensa-Kconfig.fragment` | `select` lines added under `config XTENSA_ESP32S3` to pull our two devices into the build |
| `esp32s3-soc-machine.diff` | Conceptual diff against `hw/xtensa/esp32s3.c` showing where to instantiate `ssd1677-gdeq0426` on a SPI2/HSPI bus and `ft6336u-mofei` on a bit-bang I²C bus connected to GPIO12/GPIO13 |

## Why a SoC-machine patch is non-trivial

The espressif fork's `hw/xtensa/esp32s3.c` (as of QEMU 9.2.2 build, May 2026)
explicitly models only **`spi1`** — the SPI flash controller. **SPI2/SPI3
(user peripherals) are NOT yet instantiated.** The Mofei firmware uses
hardware SPI on what Arduino-ESP32 calls SPI2 (HSPI) for the e-ink panel.

Wiring our SSD1677 to the SoC therefore requires *first* adding SPI2/SPI3
to the fork's machine model — a significantly larger change than just
attaching a slave to an existing bus. Two options to revisit at PR2 closeout:

**Option I (preferred)**: Submit a minimal SPI2 controller stub upstream
to espressif/qemu that exposes a child SSI bus. We then attach
`ssd1677-gdeq0426` to that bus.

**Option II**: Run the firmware in a "bit-bang SPI" build flag (analogous
to `MOFEI_TOUCH_SOFT_I2C=1` for I²C) and attach SSD1677 to a bit-bang SSI
bus on raw GPIO. Adds runtime overhead but avoids the controller stub.

The FT6336U is simpler: the firmware already uses **bit-bang software I²C**
(`MOFEI_TOUCH_SOFT_I2C=1`). We attach `ft6336u-mofei` to a `bitbang_i2c`
bus tied to GPIO12 (SCL) / GPIO13 (SDA) via the GPIO matrix. The fork's
`hw/i2c/Kconfig` already has `BITBANG_I2C` available.

## Reproducing the integration locally

```bash
# After running scripts/build-qemu.sh once:
simulator/scripts/integrate-peripherals.sh    # NOT YET WRITTEN
```

The integration script will:
1. Copy `simulator/qemu-peripherals/ssd1677_gdeq0426t82.c` →
   `simulator/.qemu-cache/qemu/hw/display/`
2. Copy `simulator/qemu-peripherals/ft6336u.c` →
   `simulator/.qemu-cache/qemu/hw/sensor/`
3. Append fragments above to the corresponding `meson.build` / `Kconfig`
4. Apply `esp32s3-soc-machine.diff` (once written) to
   `hw/xtensa/esp32s3.c`
5. Re-run `ninja` inside the build dir — incremental rebuild

This is intentionally **out of band** rather than a permanent fork patch
so we can pull upstream changes without conflicts.
