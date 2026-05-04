// Mofei Simulator — Tauri backend.
//
// Spawns the Espressif QEMU fork as a child process and bridges its custom
// peripherals to the Tauri webview through a Unix-domain-socket chardev.
//
// PR1 scope (M1 milestone): smoke-test the QEMU launch path and surface
// firmware UART output as a "serial-log" Tauri event. No display, no touch,
// no SD/WiFi yet — those land in PR2–PR4.

mod ipc;

use std::path::PathBuf;
use std::process::{Child, Command};
use std::sync::Mutex;
use tauri::{AppHandle, State};

struct SimState {
    process: Mutex<Option<Child>>,
}

fn project_root() -> PathBuf {
    // src-tauri/ is two levels under simulator/, so two pops to reach simulator/
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
    // Use $XDG_RUNTIME_DIR on Linux; on macOS that's typically unset, fall
    // back to /tmp. The simulator is a dev tool, single-user, so /tmp is fine.
    let base = std::env::var("XDG_RUNTIME_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|_| PathBuf::from("/tmp"));
    base.join("mofei-sim.sock")
}

fn cleanup_stale_socket(path: &std::path::Path) {
    if path.exists() {
        // Best-effort: remove a leftover socket file from a prior run.
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
             Required deps (macOS): brew install ninja glib pixman libgcrypt pkg-config",
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

    // Args based on training-knowledge of espressif/qemu fork; see
    // .trellis/tasks/05-04-mofei-simulator-bringup/research/verification-pending.md
    // for what to verify against the live README before PR1 merges.
    let mut cmd = Command::new(&qemu);
    cmd.arg("-machine").arg("esp32s3")
        .arg("-cpu").arg("esp32s3")
        .arg("-kernel").arg(&firmware)
        .arg("-nographic")
        // serial UART → host stdout (separate from our binary IPC)
        .arg("-serial").arg("stdio")
        // binary IPC chardev — virtual peripherals (PR2+) write framebuffer
        // / GPIO frames here; PR1 just opens it but no peripheral writes yet.
        .arg("-chardev").arg(format!(
            "socket,id=mofei,path={},server=on,wait=off",
            socket.display()
        ));

    match cmd.spawn() {
        Ok(child) => {
            *process_guard = Some(child);
            // Start the IPC reader. It tolerates the socket not being ready
            // immediately (retries internally).
            ipc::spawn_reader(app, socket);
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
        Ok("stopped".to_string())
    } else {
        Ok("not_running".to_string())
    }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(SimState { process: Mutex::new(None) })
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![start_sim, stop_sim])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
