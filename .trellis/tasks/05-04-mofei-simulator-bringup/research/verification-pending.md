# Verification Pending — Mofei Simulator Bringup

Date: 2026-05-04

## Why this file exists

PR1 was implemented during an upstream API/proxy outage that prevented:
- `WebFetch` calls (returned API 500 panics from the new-api/Calcium-Ion proxy)
- `Agent` subagent dispatch (`trellis-research`, `general-purpose`,
  `trellis-implement` all failed at ~180 s with the same panic signature)

Therefore PR1 was implemented in the main session by the orchestrating
assistant, using training-knowledge for QEMU + IPC API specifics, and direct
on-disk inspection (PDF datasheets, vendor code, our own `MofeiTouch.cpp`)
for the e-ink + FT6336U specifics.

This file lists every claim that needs to be verified once the network /
subagent path recovers, **before PR1 lands on `feat/murphy`** or PR2 starts.

## Items to Verify (post-recovery)

### From `research/qemu-esp32s3-options.md`

- [x] **VERIFIED 2026-05-04**: Espressif `qemu` fork supports `esp32s3`
  target (`qemu-system-xtensa -machine help` lists both `esp32` and
  `esp32s3` after a clean build). Built version: QEMU 9.2.2.
- [x] **VERIFIED 2026-05-04**: Build dependencies on macOS Apple Silicon
  are: `brew install ninja glib pixman libgcrypt pkg-config gnutls`.
  (`gnutls` was added during PR2 — needed even with `--disable-gnutls`
  was actually optional; we now pass `--disable-gnutls` for a leaner
  build, but the system header is detected by configure on some hosts.)
- [x] **VERIFIED 2026-05-04**: macOS Apple Silicon build works. Post-
  build codesign with JIT entitlements is required to map TCG exec
  pages — `simulator/scripts/build-qemu.sh` now does this automatically.
- [x] **VERIFIED 2026-05-04**: `-machine esp32s3` is the correct
  invocation flag (not `-cpu esp32s3` — that is rejected; the fork uses
  the ESP32-S3-specific CPU implicitly).
- [ ] Octal PSRAM (`R8`) and `qio_opi` flash mode emulation status — to
  verify in PR2 by attempting to boot a hello_world ESP32-S3 ELF.
- [ ] Wokwi licensing for OSS / non-cloud usage (only relevant if we ever
  fall back to Approach 2).

### From `research/qemu-peripheral-ipc.md`

- [ ] QEMU `chardev socket,id=…,path=…,server=on,wait=off` flag form is
  current. Older QEMU versions used different flag names.
- [ ] QEMU `memory_region_init_ram_from_fd` exists in the Espressif fork
  (relevant only if we later upgrade IPC to shared memory).
- [ ] Wokwi custom-chips API flow — only relevant if we ever cross-port.
- [ ] Renode peripheral pattern reference.

### From `research/eink-spi-modeling.md`

- [ ] Cross-check the SSD1677 command vocabulary I extracted from the
  GDEQ0426T82 datasheet (page 10) against the SSD1677 master datasheet
  (Solomon Systech). The 0x46/0x47 Auto-Write-RAM mappings sometimes swap
  between datasheet revisions. Verify by reading the exact command bytes
  the firmware actually issues during a normal boot — capture in PR2
  during the SPI byte-tap phase.
- [ ] Wokwi epaper component SSD16xx implementation reference.
- [ ] Renode display peripheral base class reference.
- [ ] QEMU `hw/display/ssd0303.c` reference.

### From `research/ft6336u-i2c-modeling.md`

- [ ] Register map and frame layout fully match `lib/hal/MofeiTouch.cpp`
  (already cross-checked against on-disk source — high confidence). No
  network verification needed for this file.
- [ ] QEMU `I2CSlave` API skeleton example (`hw/misc/tmp105.c`) is current.

### PR1-specific

- [x] **VERIFIED 2026-05-05**: `qemu-system-xtensa` build completes on
  macOS Apple Silicon with the dependency list in `build-qemu.sh`. Two
  rebuild iterations needed: first added `gnutls` dep, then refactored
  configure to pass `--disable-gnutls --disable-docs` (faster, fewer
  optional deps).
- [ ] An ESP-IDF `hello_world` `.elf` for ESP32-S3 is the right smoke-test
  fixture. Currently `start_sim` errors out with a clear message when the
  fixture is missing — the user is expected to provide `simulator/firmware-fixtures/hello_world.elf`
  out-of-band for now.

### PR2/PR3 integration (added 2026-05-05)

- [x] **VERIFIED 2026-05-05**: SSD1677 + FT6336U C source compile against
  fork's QEMU 9.2.2 headers. `ninja` produces a clean
  `qemu-system-xtensa-unsigned`. Required two source fixups against fork's
  QOM API: (1) class_init signature is `void(ObjectClass*, void*)` — drop
  `const`; (2) VMSTATE_UINT8 cannot store an `enum`, so `ParserMode mode`
  field changed to `uint8_t mode` (cast at use sites).
- [x] **VERIFIED 2026-05-05**: Kconfig graph is cycle-free with the
  in-tree edits committed back to `qemu-peripherals/*.fragment`:
  activation goes via `select` from `XTENSA_ESP32S3`, NOT `default y if
  XTENSA_ESP32S3` from device side (which cycles via
  `FT6336U_MOFEI depends on I2C` ← `BITBANG_I2C select I2C` ←
  `FT6336U_MOFEI select BITBANG_I2C`).
- [ ] SoC machine patch (`esp32s3-soc-machine.patch`) does NOT apply
  cleanly with `git apply` — line numbers were drafted; needs hand-merge
  to instantiate SSD1677 on a SPI2 bus + FT6336U on a bit-bang I²C bus.
  Until merged, the firmware cannot reach either peripheral despite both
  being linked into the binary.

## Resolution Protocol

When the API path recovers:

1. Re-run the original 4 `trellis-research` agents (or `general-purpose`
   with the same prompts). They may now find authoritative answers.
2. For each `[needs verification]` marker in the 4 research files, replace
   with verified content + citation, OR escalate as an open question to
   the user.
3. Update this `verification-pending.md` checklist as items resolve.
4. Once all PR1-blocking items above are checked, proceed to PR2.

PR2 should NOT start until the QEMU runtime invocation flags are verified
(item 4 of the qemu-esp32s3-options checklist) — an incorrect machine flag
would silently waste days of debugging.
