# Mofei Simulator Bringup — ESP32-S3 QEMU Full Emulation

Date: 2026-05-04
Owner: isaac.liu
Base branch: `feat/murphy`

## Goal

Build an iPhone-Simulator-grade development environment that runs **the actual unmodified
Mofei `firmware.bin`** (produced by `[env:mofei]`) under QEMU full emulation, with virtual
peripherals modeling the GDEQ0426T82 800×480 e-ink panel and FT6336U capacitive touch
controller. The existing `simulator/` Tauri 2 + React + TS shell renders the live e-ink
framebuffer and injects touch / GPIO button events. Goal is to slash the
edit→flash→observe loop from minutes to seconds and enable headless CI screenshot
regression on every PR.

## What We Already Know (locked, do not revisit)

### User-locked scope
- **Approach**: QEMU full emulation (NOT host build / NOT HAL mock). The simulator must
  execute the same `firmware.bin` byte-for-byte that flashes to a physical Mofei board.
- **Initial target hardware**: ESP32-S3 + GDEQ0426T82 4.26" e-ink (800×480, SPI) +
  FT6336U capacitive touch (I²C). X4 (ESP32-C3) is a future extension, not in scope.
- **Must simulate**: e-ink rendering (with refresh delay / grayscale visual), touch
  injection (click/swipe → FT6336U I²C registers), physical key injection (GPIO),
  SD card → local directory mapping.
- **Stubs allowed**: battery, OTA.
- **Host network passthrough**: WiFi via QEMU user-net / SLIRP (host's NIC).
- **UI shell**: existing `simulator/` Tauri 2 + React + TS project.

### Repo facts (verified at brainstorm time)
- `[env:mofei]` already exists in `platformio.ini` (`esp32-s3-devkitc1-n16r8`,
  `qio_opi`, `partitions_mofei.csv`) — firmware build path is ready, no env work needed
  for PR1.
- `simulator/src-tauri/src/lib.rs` already implements a `start_sim` Tauri command that
  spawns an external `bin/mofei-sim` native process. This validates the "Tauri = UI
  shell, simulator core = separate native process" architecture. QEMU fits this slot.
- Mofei pin map is locked by `.trellis/tasks/04-29-mofei-touch-support/prd.md`:
  - I²C touch: SDA=GPIO13, SCL=GPIO12, PWR=GPIO45, INT=GPIO44, FT6336U addr=`0x2E`
  - These are authoritative for the virtual FT6336U peripheral.
- Existing UI in `simulator/src/App.tsx` documents the input contract:
  - Arrow keys → swipe; Space/Enter → tap center; PgUp/PgDn → side buttons;
    Esc → Home; `r` → hardware refresh; mouse click → tap at cursor.
  - Local `sim_data/` dir mounts as the SD card.

### Reference materials on disk
- `docs/mofei/esp32-s3_datasheet_en.pdf` — SoC reference.
- `docs/mofei/GDEQ0426T82-T01C.pdf` — e-ink panel datasheet.
- `docs/mofei/D-FT6336U-DataSheet-V1.0.pdf` — touch controller datasheet.
- `docs/mofei/GDEQ0426T82-T01C_Arduino/{Ap_29demo.h, FT6336.cpp, FT6336.h, i2c.cpp, i2c.h, GDEQ0426T82-T01C_Arduino.ino}` —
  vendor Arduino register-level golden reference.

## Open Questions (for brainstorm Q&A loop)

These are the only items we will ask the user about. Each will be researched first,
then presented as 2–3 concrete options.

1. **QEMU selection** — Espressif `qemu-xtensa` fork vs Wokwi core vs upstream QEMU
   patches. Determines ~70% of downstream effort.
2. **Peripheral modeling depth** —
   - GDEQ0426T82: model grayscale LUT + real refresh timing, OR snapshot SPI
     framebuffer bytes and push to UI directly?
   - FT6336U: full I²C register-machine, OR minimal register set + interrupt-on-event?
3. **firmware ↔ simulator IPC** — Unix socket / shared memory / QEMU chardev / QMP /
   custom QEMU C peripheral plugin.
4. **Frontend rendering** — Canvas 2D 1bpp direct vs WebGL; restore e-ink invert /
   ghost-shadow visual?
5. **Build artifact source** — reuse `[env:mofei]` directly, or add `[env:mofei_sim]`
   (e.g., to disable hardware-only init paths, enable `-DSIMULATOR=1`)?
6. **Headless / CI** — required from M2, or M5+? Influences whether render path must
   decouple from Tauri webview.
7. **Milestones** — proposed slicing:
   - M1: QEMU runs ESP-IDF `hello_world` binary, serial visible in simulator.
   - M2: GDEQ0426T82 SPI capture → framebuffer → Tauri canvas.
   - M3: FT6336U virtual peripheral, mouse/touch round-trip into firmware.
   - M4: Real `[env:mofei]` `firmware.bin` boots into Dashboard inside simulator.

## Requirements (evolving — will be filled by Q&A)

* (TBD — populated as Open Questions resolve)

## Acceptance Criteria (evolving)

- [ ] M1: Simulator launches an ESP-IDF reference binary; serial log streams to UI.
- [ ] M2: SPI bytes written to the e-ink CS line are decoded into an 800×480 1bpp
      framebuffer and rendered in the Tauri webview within ≤ 200 ms of a full update.
- [ ] M3: Mouse-down / mouse-up / arrow keys produce valid FT6336U register reads
      that the unmodified `MofeiTouchDriver` accepts as tap/swipe events.
- [ ] M4: Production `firmware.bin` from `pio run -e mofei` boots in simulator and
      reaches the Dashboard activity end-to-end.
- [ ] (TBD) Headless screenshot regression hook usable from CI.

## Definition of Done

- All M1–M4 criteria green.
- `simulator/README.md` documents: how to build, how to launch, key bindings,
  `sim_data/` SD layout, troubleshooting.
- Lint/format clean for both `simulator/src-tauri/` (cargo fmt/clippy) and
  `simulator/src/` (existing TS toolchain).
- No regression to `[env:mofei]` firmware build.
- Rollout note: simulator is dev-only; never bundled into release artifacts.

## Out of Scope (explicit)

- X4 / ESP32-C3 emulation (future work).
- Cycle-accurate or instruction-cycle-accurate timing simulation.
- Real BLE / WiFi radio simulation (we use host NIC via SLIRP — no RF).
- Battery curve / power state simulation.
- OTA flow (stubbed).
- Pixel-perfect e-ink ghosting / refresh artifact reproduction (best-effort visual,
  not a contract).
- Replacing on-device flash testing for release validation. Simulator is a
  development accelerator, not a substitute for HW QA.

## Research References

Each of these is delegated to a `trellis-research` sub-agent and persisted under
`research/`. The main brainstorm thread does not duplicate raw findings here.

* `research/qemu-esp32s3-options.md` — QEMU fork landscape for ESP32-S3.
* `research/eink-spi-modeling.md` — GDEQ0426T82 SPI command set + how other e-ink
  simulators model the panel.
* `research/ft6336u-i2c-modeling.md` — FT6336U register layout + minimum viable
  virtual device.
* `research/qemu-peripheral-ipc.md` — How custom QEMU peripherals expose data to
  external host processes (chardev, vhost-user, QMP, Unix socket conventions).

## Decision (ADR-lite)

(To be filled at end of brainstorm.)

## Technical Notes

- Existing `simulator/src-tauri/src/lib.rs` `start_sim` already spawns a native child
  process — that slot will be repurposed to launch QEMU with our peripheral plugins.
  The Tauri stop/start lifecycle is already wired.
- `sim_data/` is the established convention for the virtual SD root.
- `[env:mofei]` already produces the firmware artifact. PR1 should not need to
  modify `platformio.ini`.
- Pin map for the FT6336U virtual peripheral is locked by the touch-support PRD.

## Implementation Plan (small PRs — provisional)

* PR1: Research lock-in + ADR + scaffold replacement of `bin/mofei-sim` placeholder
  with QEMU launcher. M1 acceptance.
* PR2: GDEQ0426T82 virtual peripheral + Tauri canvas renderer. M2 acceptance.
* PR3: FT6336U virtual peripheral + input injection contract. M3 acceptance.
* PR4: Boot real Mofei `firmware.bin` to Dashboard. M4 acceptance.
* PR5 (optional): Headless screenshot CI hook.
