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

## Requirements

### Functional

* R1. The simulator runs an unmodified `firmware.bin` produced by `pio run -e mofei`
  (until M3) and `pio run -e mofei` or a new `[env:mofei_sim]` (M4+, if needed).
* R2. The simulator binds the FT6336U virtual peripheral to bit-bang I²C on
  GPIO13 (SDA) / GPIO12 (SCL), responds at 7-bit address `0x2E`, drives INT
  on GPIO44 active-low, and respects the PWR pin on GPIO45 + RST on GPIO7.
* R3. The simulator binds the GDEQ0426T82 virtual peripheral (SSD1677 driver)
  to the SoC's SPI master used by the firmware, with D/C#, CS#, RES#, BUSY,
  BS1 wired through the GPIO matrix as the firmware drives them.
* R4. Mouse clicks and arrow-key presses in the Tauri UI inject FT6336U
  touch frames into the virtual peripheral, indistinguishable to the
  firmware from a physical FT6336U event.
* R5. Side-button keypresses (PgUp/PgDn) and Esc inject GPIO level changes
  through the simulator into the firmware's button input pins.
* R6. The host directory `simulator/sim_data/` mounts as the SD card; SD
  reads/writes go to that directory.
* R7. WiFi traffic from the firmware is bridged through the host's network
  via QEMU SLIRP / user-net.
* R8. Battery, OTA, and power-down code paths are stubbed (return canned
  values; do not actually reset).

### Non-functional

* R9. End-to-end touch latency (mouse-down in UI → firmware receives
  FT6336U frame) ≤ 32 ms p95 on a 2024-era Apple Silicon Mac.
* R10. Framebuffer commit (firmware Master Activation → pixel visible on
  Tauri canvas) ≤ 200 ms p95 for a full update.
* R11. The simulator runs entirely offline — no cloud dependency.
* R12. License compatibility: QEMU (GPLv2) is spawned as a child process
  only; not statically linked into Tauri binary. Bundled QEMU binary, if
  shipped, is documented with source-availability link.
* R13. Cross-platform: builds and runs on macOS Apple Silicon and Linux x86_64.

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

## Decision (ADR-lite, provisional pending user confirmation + post-outage web verification)

**Context**: Four high-leverage technical choices were identified during
brainstorm (QEMU selection, e-ink modeling depth, FT6336U modeling depth,
QEMU↔Tauri IPC). Web research was unavailable at brainstorm time due to an
upstream API outage; recommendations rely on training-knowledge for the
QEMU and IPC topics, and on direct datasheet + on-disk source code reading
for e-ink and FT6336U topics.

**Decision** (each summarized; full reasoning in `research/*.md`):

1. **QEMU runtime: Espressif `qemu` fork** (target `xtensa-softmmu`,
   `esp32s3`). Open-source, maintained, native macOS Apple-Silicon build,
   spawn-as-child-process license posture.
   *Risk*: octal PSRAM + `qio_opi` flash mode may need a stub or downgraded
   build. Contingency: `[env:mofei_sim]` with `qio_dio` flash mode if the
   fork's octal model is incomplete.
   *See*: [`research/qemu-esp32s3-options.md`](research/qemu-esp32s3-options.md)

2. **GDEQ0426T82 e-ink: Hybrid FSM** (Approach C). Faithful command-and-
   address-window state machine; skip-and-consume LUT bytes, softstart,
   border, temperature; one 800×480 1bpp framebuffer; BUSY-high for
   simulated refresh duration (default 0.5 s full / 0.2 s fast / 0.05 s
   partial; `MOFEI_SIM_FAST_REFRESH=1` skips the delay).
   *See*: [`research/eink-spi-modeling.md`](research/eink-spi-modeling.md)

3. **FT6336U touch: Minimal Register Machine** (Option A). Implement only
   the registers `MofeiTouchDriver` actually reads (`0x00`, `0x01`,
   `0x02..0x0E`, `0x80`, `0x88`, `0xA6`, `0xA8`); other reads return `0x00`,
   other writes are no-op. Bit-bang I²C parser on raw GPIO13/12 (no need
   for ESP32 hardware-Wire emulation, since `MOFEI_TOUCH_SOFT_I2C=1`).
   INT held low until firmware reads `TD_STATUS`.
   *See*: [`research/ft6336u-i2c-modeling.md`](research/ft6336u-i2c-modeling.md)

4. **IPC: Single Chardev Unix Socket, Length-Prefixed Binary Multiplex**
   (Approach A). 8-byte header `(channel:u8, flags:u8, reserved:u16,
   payload_len:u32) + payload`. Channels: framebuffer, serial-log,
   gpio-state, touch-event, button-event, control. Tauri reads through
   `tokio::net::UnixStream`. Stdio reserved for QEMU's own diagnostics.
   *See*: [`research/qemu-peripheral-ipc.md`](research/qemu-peripheral-ipc.md)

**Consequences**:
- 70%+ of effort goes into Approach 1 (QEMU fork build + S3 boot) and
  Approach 3 (FT6336U bit-bang protocol parser on GPIO). Both have well-
  defined contracts, low semantic risk.
- E-ink FSM is mid-effort; LUT skip-and-consume keeps it tractable.
- IPC layer is intentionally simple — moves to shm if profiling demands it.
- Web-research-deferred items (`[needs verification]` markers in research
  files) must be verified before PR1 merges; stop-gap acceptable for PR1
  scaffolding.

## Technical Notes

- Existing `simulator/src-tauri/src/lib.rs` `start_sim` already spawns a native child
  process — that slot will be repurposed to launch QEMU with our peripheral plugins.
  The Tauri stop/start lifecycle is already wired.
- `sim_data/` is the established convention for the virtual SD root.
- `[env:mofei]` already produces the firmware artifact. PR1 should not need to
  modify `platformio.ini`.
- Pin map for the FT6336U virtual peripheral is locked by the touch-support PRD.

## Implementation Plan (small PRs — provisional, pending user confirmation)

### PR1 — QEMU bringup + scaffold (M1 acceptance)

Goal: `pnpm tauri dev` in `simulator/`, click "Launch", QEMU starts, ESP-IDF
`hello_world` runs, serial output appears in Tauri UI.

Subtasks:
1. **Verify research caveats** (post-API-recovery): re-run `WebFetch` on the
   QEMU and IPC `[needs verification]` items; commit any corrections to
   `research/*.md` before code lands.
2. **Build script**: `simulator/scripts/build-qemu.sh` clones
   `espressif/qemu`, configures, builds `qemu-system-xtensa`. Idempotent.
   Cached binary in `simulator/.qemu-cache/`.
3. **Replace `start_sim` body** in `simulator/src-tauri/src/lib.rs`: instead
   of spawning `bin/mofei-sim`, spawn `qemu-system-xtensa` with the right
   flags (`-machine esp32s3`, `-cpu esp32s3`, `-kernel <hello_world.elf>`,
   `-chardev socket,id=mofei,path=…`, `-serial stdio`).
4. **IPC scaffold**: open the chardev Unix socket on the Tauri side
   (`tokio::net::UnixStream`), parse the 8-byte header framing, dispatch
   to per-channel handlers (PR1: only `0x02 serial-log` is wired).
5. **UI**: replace the keyboard-controls help text in `App.tsx` with a live
   serial console pane. Keep the existing `start_sim`/`stop_sim` buttons.
6. **Smoke test**: ESP-IDF `examples/get-started/hello_world` boots,
   `Hello world!` appears in the Tauri console pane.

### PR2 — GDEQ0426T82 virtual peripheral + canvas (M2)

Subtasks:
1. **C peripheral**: `simulator/qemu-peripherals/ssd1677_gdeq0426t82.c`
   implementing the hybrid FSM from `research/eink-spi-modeling.md`.
   Out-of-tree QOM device class; loaded via `-device ssd1677-gdeq0426`.
2. **Wire up**: SPI master (SPI2 on ESP32-S3) → device. GPIO matrix lines
   for D/C#, CS#, RES#, BUSY, BS1 mapped per Mofei pin assignment (extract
   from `lib/hal/HalDisplay*` and `open-x4-sdk/src/EInkDisplay/`).
3. **IPC channel `0x01 framebuffer`**: on Master Activation, push 48 KB
   1bpp buffer over the chardev to Tauri. Drive BUSY high for the
   configured refresh duration.
4. **Frontend**: React component that decodes the 1bpp buffer to Canvas2D
   `ImageData` and blits at 1:1 scale (800×480 panel → fits 800×600 window).
5. **Smoke test**: a hand-crafted firmware that draws a checkerboard
   (`memset` framebuffer, command 0x24 with pattern, 0x22 0x20) renders
   correctly in the Tauri canvas.
6. **Acceptance gate**: `pio run -e mofei` firmware on the simulator boots
   and reaches the splash/loading screen visibly.

### PR3 — FT6336U virtual peripheral + input injection (M3)

Subtasks:
1. **C peripheral**: `simulator/qemu-peripherals/ft6336u.c` implementing
   the minimal register machine from `research/ft6336u-i2c-modeling.md`.
2. **Bit-bang I²C parser**: subscribe to GPIO12/13 lines, decode
   START/STOP/ADDR/DATA, dispatch to register file. Verify protocol-level
   conformance against `MofeiTouchDriver` traffic captured in PR2 logs.
3. **INT pin**: drive GPIO44 low on touch, release on `TD_STATUS` read.
4. **IPC channels**:
   - `0x04 touch-event` inbound (Tauri → QEMU): `(action, x, y, finger_id)`.
     Translate to FT6336U frame buffer + assert INT.
   - `0x05 button-event` inbound: GPIO level changes for side buttons
     (PgUp/PgDn) and Esc.
5. **Frontend**: mouse-down/move/up handlers on the canvas → `0x04` frames.
   Keyboard handlers: arrow keys → simulated swipe gestures (down at center,
   move to direction over 200 ms, up); Space/Enter → tap center;
   PgUp/PgDn/Esc → button events.
6. **Smoke test**: firmware's serial log shows `FT6336U detected` and
   touch events flow through to Dashboard taps.
7. **Acceptance gate**: a tap and a swipe round-trip end-to-end.

### PR4 — Production firmware to Dashboard (M4)

Subtasks:
1. Boot path: confirm production `pio run -e mofei` `firmware.bin` (with
   real bootloader, real partitions) boots in our simulator. If `qio_opi`
   blocks: add `[env:mofei_sim]` with `qio_dio` and document the deviation.
2. SD card: bind `simulator/sim_data/` as the SD root (`-drive file=…,if=sd`
   or virtual SD device).
3. WiFi: enable QEMU SLIRP user-net. Confirm DHCP works.
4. Stub OTA, battery: ensure firmware paths that touch these don't crash.
5. **Acceptance gate**: full Dashboard activity reachable. Mouse a couple
   of icons. Verify rendered icons match physical device.

### PR5 (optional) — Headless screenshot CI hook

Subtasks:
1. CLI flag `--headless --screenshot path.png --duration N`: run for N
   seconds, capture canvas pixel buffer, write PNG, exit.
2. GitHub Actions job that boots an `[env:mofei]` build, takes 5 anchor
   screenshots (boot, dashboard, reader open), diffs against committed
   reference PNGs, fails on > N% delta.

## Research References

* [`research/qemu-esp32s3-options.md`](research/qemu-esp32s3-options.md) — Espressif `qemu` fork is the recommended runtime (with PSRAM/OPI-flash contingency); web sources marked `[needs verification]`.
* [`research/eink-spi-modeling.md`](research/eink-spi-modeling.md) — SSD1677 hybrid FSM; full command set extracted from on-disk PDF page 10.
* [`research/ft6336u-i2c-modeling.md`](research/ft6336u-i2c-modeling.md) — Minimal register machine; full register map sourced from `lib/hal/MofeiTouch.cpp`.
* [`research/qemu-peripheral-ipc.md`](research/qemu-peripheral-ipc.md) — Single-chardev UDS multiplex with 8-byte header; QEMU API specifics marked `[needs verification]`.
