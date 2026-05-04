// IPC frame protocol — see
// .trellis/tasks/05-04-mofei-simulator-bringup/research/qemu-peripheral-ipc.md
//
// Wire format (8-byte header, big-endian fixed at parser, but we treat
// numeric fields as little-endian to match QEMU/Tauri x86_64+ARM64 hosts):
//
//     u8  channel
//     u8  flags
//     u16 reserved
//     u32 payload_len  (little-endian)
//     u8  payload[payload_len]
//
// Channels:
//     0x01 framebuffer       (PR2)
//     0x02 serial-log        (PR1) — UTF-8 bytes from the firmware UART
//     0x03 gpio-state        (PR2/3)
//     0x04 touch-event       (PR3, inbound)
//     0x05 button-event      (PR3, inbound)
//     0x06 control           (bidi)
//     0xFF heartbeat / error
//
// PR1 only wires channel 0x02 → Tauri event "serial-log".

use std::path::Path;
use base64::{engine::general_purpose::STANDARD as BASE64_STANDARD, Engine};
use tauri::{AppHandle, Emitter};
use tokio::io::AsyncReadExt;
use tokio::net::UnixStream;

const HEADER_LEN: usize = 8;
const MAX_PAYLOAD_LEN: u32 = 1 << 20; // 1 MiB safety cap

pub const CHANNEL_FRAMEBUFFER: u8 = 0x01;
pub const CHANNEL_SERIAL_LOG: u8 = 0x02;
pub const CHANNEL_GPIO_STATE: u8 = 0x03;
pub const CHANNEL_TOUCH_EVENT: u8 = 0x04;
pub const CHANNEL_BUTTON_EVENT: u8 = 0x05;
pub const CHANNEL_CONTROL: u8 = 0x06;
pub const CHANNEL_HEARTBEAT: u8 = 0xFF;

#[derive(Debug)]
pub struct Frame {
    pub channel: u8,
    pub flags: u8,
    pub payload: Vec<u8>,
}

/// Spawn a dedicated OS thread hosting a current-thread Tokio runtime that
/// reads frames from `socket_path` and emits them as Tauri events. Returns
/// immediately. The thread (and its runtime) live until the connection
/// closes or an unrecoverable parse error occurs.
///
/// We build our own runtime rather than relying on `tauri::async_runtime`
/// because PR1 wants tight control over the IPC reader's lifecycle, and a
/// dedicated thread lets us cleanly cancel by dropping the runtime later
/// (PR3 will add a Stop hook).
pub fn spawn_reader(app: AppHandle, socket_path: std::path::PathBuf) {
    std::thread::Builder::new()
        .name("mofei-sim-ipc".to_string())
        .spawn(move || {
            let rt = match tokio::runtime::Builder::new_current_thread()
                .enable_all()
                .build()
            {
                Ok(rt) => rt,
                Err(e) => {
                    let _ = app.emit("simulator-error", format!("ipc tokio runtime failed: {e}"));
                    return;
                }
            };
            rt.block_on(async move {
                // QEMU may take a moment to bind the socket after launch.
                // Retry briefly before giving up.
                let stream = match connect_with_retry(&socket_path, 20, 100).await {
                    Ok(s) => s,
                    Err(e) => {
                        let _ = app.emit("simulator-error", format!("ipc connect failed: {e}"));
                        return;
                    }
                };
                if let Err(e) = read_loop(app.clone(), stream).await {
                    let _ = app.emit("simulator-error", format!("ipc read loop ended: {e}"));
                }
            });
        })
        .expect("failed to spawn mofei-sim-ipc thread");
}

async fn connect_with_retry(
    path: &Path,
    attempts: u32,
    delay_ms: u64,
) -> std::io::Result<UnixStream> {
    let mut last_err: Option<std::io::Error> = None;
    for _ in 0..attempts {
        match UnixStream::connect(path).await {
            Ok(s) => return Ok(s),
            Err(e) => last_err = Some(e),
        }
        tokio::time::sleep(std::time::Duration::from_millis(delay_ms)).await;
    }
    Err(last_err.unwrap_or_else(|| {
        std::io::Error::new(std::io::ErrorKind::TimedOut, "no attempts performed")
    }))
}

async fn read_loop(app: AppHandle, mut stream: UnixStream) -> std::io::Result<()> {
    let mut header = [0u8; HEADER_LEN];
    loop {
        stream.read_exact(&mut header).await?;
        let channel = header[0];
        let flags = header[1];
        // header[2..4] = reserved (ignored)
        let payload_len = u32::from_le_bytes([header[4], header[5], header[6], header[7]]);
        if payload_len > MAX_PAYLOAD_LEN {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                format!("frame payload_len {payload_len} exceeds cap {MAX_PAYLOAD_LEN}"),
            ));
        }
        let mut payload = vec![0u8; payload_len as usize];
        if payload_len > 0 {
            stream.read_exact(&mut payload).await?;
        }
        dispatch(&app, Frame { channel, flags, payload });
    }
}

fn dispatch(app: &AppHandle, frame: Frame) {
    match frame.channel {
        CHANNEL_SERIAL_LOG => {
            // Decode lossy — firmware UART output is UTF-8 with the occasional
            // boot-rom byte that won't decode cleanly.
            let text = String::from_utf8_lossy(&frame.payload).into_owned();
            let _ = app.emit("serial-log", text);
        }
        CHANNEL_FRAMEBUFFER => {
            // 1bpp 800×480 = 48000 bytes; row-major, MSB-first within byte.
            // Pre-compose a host-side RGBA buffer here and emit it as a
            // base64 string so the React Canvas2D side can blit without
            // rebuilding the bit-twiddling loop.
            //
            // Layout-aware: bit==1 means white (0xFF), bit==0 means black
            // (0x00). Alpha always 0xFF.
            let rgba = framebuffer_1bpp_to_rgba(&frame.payload);
            let encoded = BASE64_STANDARD.encode(&rgba);
            let payload = serde_json::json!({
                "kind": if frame.flags & FB_FLAG_PARTIAL != 0 { "partial" } else { "full" },
                "width": EPD_WIDTH,
                "height": EPD_HEIGHT,
                "rgba_b64": encoded,
            });
            let _ = app.emit("framebuffer", payload);
        }
        // PR3 channels not wired yet.
        CHANNEL_GPIO_STATE
        | CHANNEL_TOUCH_EVENT
        | CHANNEL_BUTTON_EVENT
        | CHANNEL_CONTROL
        | CHANNEL_HEARTBEAT => {}
        unknown => {
            let _ = app.emit(
                "simulator-error",
                format!("ipc: unknown channel 0x{unknown:02X}, flags=0x{:02X}, len={}", frame.flags, frame.payload.len()),
            );
        }
    }
}

const EPD_WIDTH: u32 = 800;
const EPD_HEIGHT: u32 = 480;
const EPD_FB_BYTES: usize = (EPD_WIDTH as usize * EPD_HEIGHT as usize) / 8;
const FB_FLAG_PARTIAL: u8 = 0x01;

/// Decode an 800×480 1bpp framebuffer (row-major, MSB-first) to RGBA8888.
/// White pixel (bit=1) → (255,255,255,255); black pixel (bit=0) → (0,0,0,255).
fn framebuffer_1bpp_to_rgba(fb: &[u8]) -> Vec<u8> {
    let mut out = vec![0u8; (EPD_WIDTH as usize) * (EPD_HEIGHT as usize) * 4];
    let n = fb.len().min(EPD_FB_BYTES);
    for byte_idx in 0..n {
        let b = fb[byte_idx];
        let base_pixel = byte_idx * 8;
        for bit in 0..8 {
            let pixel_idx = base_pixel + bit;
            let value = if (b >> (7 - bit)) & 1 != 0 { 0xFFu8 } else { 0x00u8 };
            let off = pixel_idx * 4;
            out[off] = value;
            out[off + 1] = value;
            out[off + 2] = value;
            out[off + 3] = 0xFF;
        }
    }
    out
}
