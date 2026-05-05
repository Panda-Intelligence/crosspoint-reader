// Mofei Simulator — Tauri backend.
//
// Spawns the Espressif QEMU fork as a child process and bridges its custom
// peripherals to the Tauri webview through a Unix-domain-socket chardev
// (single socket, length-prefixed binary multiplex — see ipc.rs).
//
// Outbound (QEMU → UI): serial log (PR1), framebuffer (PR2).
// Inbound (UI → QEMU): touch events, button events (PR3).
//
// QEMU integration of the SSD1677 + FT6336U virtual peripherals into the
// Espressif fork's machine model is *partial* — see
// simulator/qemu-peripherals/INTEGRATION.md. Today the IPC plumbing is
// fully exercised end-to-end on the host side; the firmware-facing side
// requires the SoC machine patch to land.

mod ipc;

use std::path::PathBuf;
use std::process::{Child, Command};
use std::sync::{Arc, Mutex};
use tauri::{AppHandle, State};

struct SimState {
    process: Mutex<Option<Child>>,
    writer: Arc<ipc::IpcWriter>,
}

fn project_root() -> PathBuf {
    let mut p = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    if p.ends_with("src-tauri") {
        p.pop();
    }
    p
}

fn qemu_binary_path() -> PathBuf {
    project_root().join(".qemu-cache/qemu/build/qemu-system-xtensa")
}

fn default_firmware_path() -> PathBuf {
    project_root().join("firmware-fixtures/hello_world.elf")
}

fn ipc_socket_path() -> PathBuf {
    let base = std::env::var("XDG_RUNTIME_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|_| PathBuf::from("/tmp"));
    base.join("mofei-sim.sock")
}

fn cleanup_stale_socket(path: &std::path::Path) {
    if path.exists() {
        let _ = std::fs::remove_file(path);
    }
}

#[tauri::command]
fn start_sim(app: AppHandle, state: State<'_, SimState>) -> Result<String, String> {
    let mut process_guard = state.process.lock().map_err(|e| e.to_string())?;
    if process_guard.is_some() {
        return Ok("already_running".to_string());
    }

    let qemu = qemu_binary_path();
    if !qemu.exists() {
        return Err(format!(
            "QEMU binary not found at {}.\n\
             Run `simulator/scripts/build-qemu.sh` first.\n\
             Required deps (macOS): brew install ninja glib pixman libgcrypt pkg-config gnutls",
            qemu.display()
        ));
    }

    let firmware = default_firmware_path();
    if !firmware.exists() {
        return Err(format!(
            "Firmware fixture not found at {}.\n\
             For PR1 (M1 smoke test), build ESP-IDF's hello_world example for esp32s3 \
             and copy the resulting .elf into simulator/firmware-fixtures/hello_world.elf.\n\
             PR4 will switch to booting the production firmware.bin from `pio run -e mofei`.",
            firmware.display()
        ));
    }

    let socket = ipc_socket_path();
    cleanup_stale_socket(&socket);

    // `-machine esp32s3` is the verified flag (`-cpu esp32s3` is rejected by
    // the fork). PSRAM/qio_opi flash mode boot — still unverified for the
    // production firmware.bin; see verification-pending.md.
    let mut cmd = Command::new(&qemu);
    cmd.arg("-machine").arg("esp32s3")
        .arg("-kernel").arg(&firmware)
        .arg("-nographic")
        .arg("-serial").arg("stdio")
        .arg("-chardev").arg(format!(
            "socket,id=mofei,path={},server=on,wait=off",
            socket.display()
        ));

    match cmd.spawn() {
        Ok(child) => {
            *process_guard = Some(child);
            ipc::spawn_io(app, state.writer.clone(), socket);
            Ok("started".to_string())
        }
        Err(e) => Err(format!("failed to spawn qemu-system-xtensa: {}", e)),
    }
}

#[tauri::command]
fn stop_sim(state: State<'_, SimState>) -> Result<String, String> {
    let mut process_guard = state.process.lock().map_err(|e| e.to_string())?;
    if let Some(mut child) = process_guard.take() {
        let _ = child.kill();
        let _ = child.wait();
        cleanup_stale_socket(&ipc_socket_path());
        state.writer.unbind();
        Ok("stopped".to_string())
    } else {
        Ok("not_running".to_string())
    }
}

#[tauri::command]
fn inject_touch(
    state: State<'_, SimState>,
    action: u8,
    x: u16,
    y: u16,
    finger_id: u8,
) -> Result<bool, String> {
    let frame = ipc::encode_touch_event(action, x, y, finger_id);
    Ok(state.writer.try_send(frame))
}

#[tauri::command]
fn inject_button(
    state: State<'_, SimState>,
    button_id: u8,
    pressed: bool,
) -> Result<bool, String> {
    let frame = ipc::encode_button_event(button_id, pressed);
    Ok(state.writer.try_send(frame))
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let writer = Arc::new(ipc::IpcWriter::new());
    tauri::Builder::default()
        .manage(SimState {
            process: Mutex::new(None),
            writer,
        })
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![
            start_sim,
            stop_sim,
            inject_touch,
            inject_button
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
