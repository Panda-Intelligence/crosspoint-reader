# QEMU ESP32-S3 Emulation Options

Status: Web research blocked (upstream API outage 2026-05-04). Findings are
**training-knowledge with `[needs verification]` markers**. To be re-checked
once `WebFetch` is reachable, before committing the ADR.
Date: 2026-05-04

## Executive Summary

`[needs verification]` Espressif maintains an actively-developed QEMU fork
(`github.com/espressif/qemu`) that is the de-facto only viable path for full-
system ESP32-S3 emulation today. The fork has grown S3 support across 2023–
2024: dual-core Xtensa LX7, FreeRTOS-friendly timer/interrupt model, GPIO
matrix, UART, basic SPI, GDMA. **Octal PSRAM (`R8`) and `qio_opi` flash mode
are the historically weak spot** and may require either a stub MMU mode or
patching the firmware build to use a compatible flash mode for simulator runs.
Wokwi's emulator core is closed-source and gated behind their cloud/CI
products. Upstream QEMU's Xtensa target supports older ESP32 (mostly via
community patches) but lacks ESP32-S3 peripherals. Renode has Xtensa support
but practically no ESP32-S3 model.

**Recommendation: Approach 1 (espressif/qemu fork)**, with an explicit
contingency plan to either (a) accept downgraded flash mode in `[env:mofei]`
for simulator builds, or (b) extend the fork's PSRAM model. Effort to first
SPI capture: ~3–5 days assuming the fork's S3 SPI works, ~7–10 if we have to
patch SPI bus introspection ourselves.

## Comparison Table

`[All cells need verification — figures based on training-time knowledge]`

| Capability | Espressif `qemu` fork | Wokwi core | Upstream QEMU | Renode |
|---|---|---|---|---|
| S3 dual-core Xtensa LX7 | ✓ | ✓ | ✗ | ✗ |
| GPIO matrix | ✓ | ✓ | partial | ✗ |
| SPI peripherals | ✓ (SPI2/SPI3) | ✓ | partial | ? |
| I²C peripheral | partial | ✓ | partial | ✗ |
| Octal PSRAM (`R8`) | ⚠️ partial | ✓ | ✗ | ✗ |
| `qio_opi` flash mode | ⚠️ may need stub | ✓ | ✗ | ✗ |
| Custom virtual peripherals | ✓ via QOM/sysbus C API | ✓ via JS/C custom-chips | ✓ via QOM | ✓ via C# |
| Bus introspection | chardev / QMP / sysbus probe | webhook / event API | chardev / QMP | event hooks |
| Boot from raw `.bin` | ✓ | ✓ | ⚠️ | ⚠️ |
| Boot from `.elf` | ✓ | ✓ | ✓ | ✓ |
| macOS Apple Silicon build | ✓ | ✓ (cloud) | ✓ | ⚠️ Mono runtime |
| License | GPLv2 | proprietary | GPLv2 | MIT/Apache |
| Open source | ✓ | ✗ | ✓ | ✓ |
| Process model | spawn child | spawn child or cloud | spawn child | spawn child |

## Approaches

### Approach 1: Espressif `qemu` fork (Recommended)

**How it works:** Build `qemu-system-xtensa` from `espressif/qemu`, target
`esp32s3`. Write our virtual e-ink and FT6336U as in-tree QOM device classes
(or out-of-tree via `-device …` plugin if supported on this fork — verify).
Connect to Tauri-side UI via QEMU `chardev` Unix socket. Boot the
`firmware.bin` produced by `[env:mofei]` directly via `-drive`+`-bios` or
the fork's `-merge_args` binary boot helper.

**Pros**:
- Upstream-maintained S3 support, `make` from source on macOS works.
- Dual-core, FreeRTOS-friendly timer model.
- Familiar QEMU device model (sysbus, qdev) — anyone with QEMU experience can
  contribute.
- GPL but process boundary preserves Tauri app's license freedom.
- Free, no cloud dependency, runs offline.

**Cons**:
- `qio_opi` octal-flash and N16R8 octal-PSRAM emulation may be incomplete.
- Build dependencies are non-trivial on macOS (Ninja, GLib, pixman).
- Some S3 peripherals (CAM, LCD_CAM, RMT) are missing — but we don't use
  those.

**Effort to first SPI capture**: ~3–5 days (1 day fork build, 1 day boot the
firmware, 1 day stub e-ink as I²C/SPI capture, 1 day chardev to Tauri).
Possibly +5 days if PSRAM/OPI flash needs patching.

### Approach 2: Wokwi-bridge (Fallback)

**How it works:** Use Wokwi's CLI / VS Code extension as the simulator
runtime; bridge their event API to our Tauri UI through a thin Node/Rust
adapter. Wokwi already has e-paper and FT6336-class touch components.

**Pros**:
- Out-of-the-box ESP32-S3 + octal PSRAM + octal flash.
- Pre-built e-paper and capacitive-touch components — possibly minimal
  custom code.

**Cons**:
- Closed source. We are at vendor mercy for any model fix.
- Licensing for non-cloud / commercial / OSS-redistribution is unclear and
  may block our ship goals.
- Custom-peripheral API is JS/WebAssembly-flavored — fits browser, awkward
  to bridge to a native Tauri Rust process.
- Their ESP32-S3 Wokwi-CLI behavior may differ from cloud and from real
  hardware.

**Effort to first SPI capture**: ~1 day if licensing allows.

### Approach 3: Upstream QEMU + community Xtensa patches (Not Recommended)

Lacks S3 peripherals; we'd be writing the whole thing. ~2–3× the work of
Approach 1 with no benefit.

## Boot from `.bin` vs `.elf`

`[needs verification]`

PlatformIO `[env:mofei]` produces `firmware.bin` (raw flash image with
bootloader + app at correct partitions, or just app — depends on build) plus
`firmware.elf` (debugger-friendly).

- Espressif fork supports both: `.bin` via `-drive file=firmware.bin,if=mtd`
  with the `efuse.bin`/`bootloader.bin` partition (matches `partitions_mofei.csv`),
  and `.elf` via `-kernel firmware.elf` for fast iteration without bootloader
  involvement.
- For PR1 (M1 milestone), boot via `-kernel app.elf` is simpler — skip the
  bootloader entirely, reach `app_main()` directly. Switch to `.bin` boot
  for M4 ("real production image").

## macOS Apple Silicon

Espressif fork builds natively on Apple Silicon. Known recipe:
```
brew install ninja glib pixman libgcrypt
git clone https://github.com/espressif/qemu
cd qemu
./configure --target-list=xtensa-softmmu --enable-gcrypt --enable-slirp
make -j
```
`[needs verification]` Output binary: `build/qemu-system-xtensa`.

## License Implication

QEMU is GPLv2. We **spawn it as a separate child process** from our Tauri
Rust backend; we do not link QEMU code into the Tauri binary. That's a
process boundary, not a derivative work. The Tauri app's MIT/Apache-2.0
license is preserved. Distribution of a bundled QEMU binary inside the
simulator package would be GPL but that's allowed alongside non-GPL Tauri
code as long as we honor source-availability. **In practice, distribute
Tauri app + a postinstall step that builds or downloads QEMU separately**;
this avoids any bundling questions entirely.

## Recommendation

**Approach 1: Espressif `qemu` fork**. Open-source, maintained, native build
on our dev hosts, clean process boundary. Risk concentrates in PSRAM/OPI
flash — manage with a contingency plan to fall back to `qio_dio` flash mode
for `[env:mofei_sim]` if needed. Wokwi is a strong fallback if licensing or
build issues block us, but starting there cedes control of an architecturally
central component.

## Citations / To Verify Post-Outage

- https://github.com/espressif/qemu — README, supported targets, S3 status
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/qemu.html — official ESP-IDF guide
- https://docs.wokwi.com/guides/esp32 — Wokwi S3 support
- https://docs.wokwi.com/parts/wokwi-epaper-1.54-mono — e-paper part docs
- https://docs.wokwi.com/api/custom-chips-api — custom chips API
- Renode docs page on Xtensa — verify S3 peripheral coverage

All `[needs verification]` markers above should be re-checked when WebFetch
is reachable, *before* committing this PRD's ADR.
