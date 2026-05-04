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

- [ ] Espressif `qemu` fork (https://github.com/espressif/qemu) actually
  supports `esp32s3` target as of 2026-05-04. Confirm branch name (was
  `xtensa-softmmu` target the right naming?).
- [ ] Build dependencies on macOS Apple Silicon: `brew install ninja glib
  pixman libgcrypt` is sufficient. Confirm against current README.
- [ ] Octal PSRAM (`R8`) and `qio_opi` flash mode emulation status. The
  research file flags this as a risk; need to read open issues to confirm
  whether our `[env:mofei]` `firmware.bin` actually boots, or whether we
  need `[env:mofei_sim]` with `qio_dio` flash mode as fallback.
- [ ] `-machine esp32s3 -cpu esp32s3` is the correct invocation. The fork
  may use different flag names (`-machine esp32s3 -m 8M` or similar).
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

- [ ] `qemu-system-xtensa` build actually completes on macOS Apple Silicon
  with the dependency list documented in `simulator/scripts/build-qemu.sh`.
  This was NOT executed during PR1 implementation. The script is committed
  as scaffolded but unverified.
- [ ] An ESP-IDF `hello_world` `.elf` for ESP32-S3 is the right smoke-test
  fixture. Currently `start_sim` errors out with a clear message when the
  fixture is missing — the user is expected to provide `simulator/firmware-fixtures/hello_world.elf`
  out-of-band for now.

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
