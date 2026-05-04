# Mofei Simulator

iPhone-Simulator-style development environment for the Xteink Mofei e-reader
hardware (ESP32-S3 + GDEQ0426T82 4.26" e-ink + FT6336U capacitive touch).

Runs the **unmodified production `firmware.bin`** under QEMU full emulation,
with virtual e-ink + touch peripherals bridged to a Tauri webview UI for
display, mouse/keyboard input, and SD-card-as-local-directory.

> **Status (2026-05-04): PR1 / M1 only.** Today this project boots an
> ESP-IDF `hello_world` ELF and surfaces its serial UART in the Tauri UI.
> Display, touch, real firmware boot, and SD/WiFi land in PR2–PR4. See
> [`../.trellis/tasks/05-04-mofei-simulator-bringup/prd.md`](../.trellis/tasks/05-04-mofei-simulator-bringup/prd.md)
> for the full milestone plan.

## One-time setup

### 1. System dependencies

```bash
# macOS (Apple Silicon and Intel both work)
brew install ninja glib pixman libgcrypt pkg-config

# Linux (Debian/Ubuntu)
sudo apt install ninja-build libglib2.0-dev libpixman-1-dev libgcrypt20-dev pkg-config
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

A window opens. Click **Launch QEMU**. The serial console at the bottom
of the window streams the firmware's UART output. **Stop** kills the QEMU
child process.

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

## Known limitations (PR1)

- No display rendering yet (PR2).
- No touch or button input yet (PR3).
- No SD card / WiFi / OTA yet (PR4).
- The `start_sim` QEMU invocation flags are committed unverified — they
  rely on training-knowledge of the Espressif fork. See
  [`../.trellis/tasks/05-04-mofei-simulator-bringup/research/verification-pending.md`](../.trellis/tasks/05-04-mofei-simulator-bringup/research/verification-pending.md)
  for the post-recovery verification checklist.

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
