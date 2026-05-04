use std::process::{Child, Command};
use std::sync::Mutex;
use tauri::State;

struct SimState {
    process: Mutex<Option<Child>>,
}

#[tauri::command]
fn start_sim(state: State<'_, SimState>) -> Result<String, String> {
    let mut process_guard = state.process.lock().unwrap();
    if process_guard.is_some() {
        return Ok("already_running".to_string());
    }

    // Determine path to bin/mofei-sim
    let mut path = std::env::current_dir().unwrap();
    path.pop(); // Go up to crosspoint-reader
    path.push("bin");
    path.push("mofei-sim");

    match Command::new(path).spawn() {
        Ok(child) => {
            *process_guard = Some(child);
            Ok("started".to_string())
        }
        Err(e) => Err(format!("Failed to start simulator: {}", e)),
    }
}

#[tauri::command]
fn stop_sim(state: State<'_, SimState>) -> Result<String, String> {
    let mut process_guard = state.process.lock().unwrap();
    if let Some(mut child) = process_guard.take() {
        let _ = child.kill();
        let _ = child.wait();
        Ok("stopped".to_string())
    } else {
        Ok("not_running".to_string())
    }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(SimState {
            process: Mutex::new(None),
        })
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![start_sim, stop_sim])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
