# Mofei Simulator

iPhone-Simulator-style development environment for the Xteink Mofei e-reader
hardware (ESP32-S3 + GDEQ0426T82 4.26" e-ink + FT6336U capacitive touch).

Runs the **unmodified production `firmware.bin`** under QEMU full emulation,
with virtual e-ink + touch peripherals bridged to a Tauri webview UI for
display, mouse/keyboard input, and SD-card-as-local-directory.

> **Status (2026-05-05): PR3 / M3 partial.** PR1+PR2+PR3 host-side
> plumbing is complete (QEMU launcher, IPC reader+writer, e-ink canvas,
> mouse/keyboard input injection). The QEMU SoC integration patch that
> wires the SSD1677 + FT6336U virtual peripherals into the Espressif
> fork's machine model is **partial** — see
> [`qemu-peripherals/INTEGRATION.md`](qemu-peripherals/INTEGRATION.md)
> for the SPI2/SPI3 controller stub still to land. Full milestone plan in
> [`../.trellis/tasks/05-04-mofei-simulator-bringup/prd.md`](../.trellis/tasks/05-04-mofei-simulator-bringup/prd.md).

## One-time setup

### 1. System dependencies

```bash
# macOS (Apple Silicon and Intel both work)
brew install ninja glib pixman libgcrypt pkg-config gnutls

# Linux (Debian/Ubuntu)
sudo apt install ninja-build libglib2.0-dev libpixman-1-dev libgcrypt20-dev pkg-config libgnutls28-dev
```

### 2. Build the QEMU runtime

This clones and builds the Espressif QEMU fork into `simulator/.qemu-cache/`.
The first run takes ~15 minutes; subsequent runs are instant (cached).

```bash
cd simulator
./scripts/build-qemu.sh
```

To force a rebuild:
```bash
./scripts/build-qemu.sh --force
```

### 3. Provide a firmware fixture (PR1 only)

PR1's smoke test boots an ESP-IDF `hello_world` ELF for ESP32-S3. Build it
with ESP-IDF and copy the resulting ELF to:

```
simulator/firmware-fixtures/hello_world.elf
```

> PR4 will switch to booting the production `firmware.bin` from
> `pio run -e mofei` and this manual fixture step will go away.

### 4. Install JS dependencies

```bash
cd simulator
npm install
```

## Daily development

```bash
cd simulator
npm run tauri dev
```

A window opens. Click **Launch QEMU**. The 800×480 e-ink canvas reflects
the firmware's framebuffer; the serial console at the bottom streams UART.
**Stop** kills the QEMU child process.

### Keyboard / mouse input injection

| Input | Effect |
|---|---|
| Click / drag on canvas | Touch tap / swipe at canvas coordinates |
| ←→↑↓ | Simulated swipe from canvas center |
| Space / Enter | Tap at canvas center |
| PageUp / PageDown | Side buttons (Up / Down) |
| Esc | Home button |
| R | Hardware refresh button |

Input events are routed through the Tauri backend → IPC writer → QEMU
`chardev` → FT6336U virtual peripheral (channel 0x04) or button virtual
device (channel 0x05).

## Architecture (PR1)

```
┌───────────────────────┐
│  Tauri webview        │
│  (React, App.tsx)     │
│  • serial console     │
└──────────┬────────────┘
           │ Tauri events ("serial-log")
┌──────────▼────────────┐
│  Tauri Rust backend   │
│  • start_sim / stop_sim
│  • ipc::spawn_reader  │
└──────────┬────────────┘
           │ Unix-domain-socket chardev (binary frame protocol)
           │ /tmp/mofei-sim.sock or $XDG_RUNTIME_DIR/mofei-sim.sock
┌──────────▼────────────┐
│  qemu-system-xtensa   │
│  -machine esp32s3     │
│  -kernel hello.elf    │
│  -chardev socket,…    │
└───────────────────────┘
```

The IPC frame format is a fixed 8-byte header
`(channel:u8, flags:u8, reserved:u16, payload_len:u32)` followed by
`payload_len` bytes. PR1 wires only the `0x02 serial-log` channel; PR2–PR3
will add framebuffer (`0x01`), touch (`0x04`), and button (`0x05`) channels.

See
[`../.trellis/tasks/05-04-mofei-simulator-bringup/research/qemu-peripheral-ipc.md`](../.trellis/tasks/05-04-mofei-simulator-bringup/research/qemu-peripheral-ipc.md)
for the full protocol design.

## CI / headless screenshot

The `simulator/headless/` crate builds a separate binary that runs QEMU
without the Tauri webview and writes a PNG of the last framebuffer
received during a fixed observation window. Designed for boot-into-known-
pixels regression in CI.

```bash
cd simulator/headless
cargo run --release -- \
    --firmware ../firmware-fixtures/hello_world.elf \
    --duration 10 \
    --out boot.png
```

Exits non-zero if no framebuffer was captured (useful as a CI gate).

## Known limitations (PR3)

- The SSD1677 + FT6336U virtual peripherals are committed in
  `qemu-peripherals/` but the QEMU build integration patch (Meson
  fragments, Kconfig, and ESP32-S3 SoC machine wiring) is partial —
  pending a SPI2/SPI3 controller stub upstream. See
  [`qemu-peripherals/INTEGRATION.md`](qemu-peripherals/INTEGRATION.md).
- No SD card / WiFi / OTA yet (PR4).
- PSRAM (`R8`) and `qio_opi` flash mode emulation status for the
  production `firmware.bin` is unverified — see
  [`../.trellis/tasks/05-04-mofei-simulator-bringup/research/verification-pending.md`](../.trellis/tasks/05-04-mofei-simulator-bringup/research/verification-pending.md).

## Project layout

```
simulator/
├── README.md                      ← you are here
├── package.json                   ← Tauri + React frontend
├── vite.config.ts
├── tsconfig.json
├── src/                           ← React app
│   ├── App.tsx                    ← serial console UI
│   └── main.tsx
├── src-tauri/                     ← Rust backend
│   ├── Cargo.toml
│   ├── tauri.conf.json
│   └── src/
│       ├── main.rs
│       ├── lib.rs                 ← start_sim / stop_sim Tauri commands
│       └── ipc.rs                 ← chardev frame parser
├── scripts/
│   └── build-qemu.sh              ← Espressif QEMU fork builder
├── firmware-fixtures/             ← ELFs / .bins fed to QEMU (gitignored)
└── .qemu-cache/                   ← build artifacts (gitignored)
```
