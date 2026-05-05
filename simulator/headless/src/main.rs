// Mofei simulator — headless screenshot harness.
//
// Spawns QEMU with the same args as the Tauri lib::start_sim path, reads
// framebuffer frames over the chardev Unix socket, and writes a PNG of the
// last frame received during a fixed observation window. Designed for CI
// regression: boot-into-known-pixels checks.
//
// Usage:
//   mofei-sim-headless --firmware path/to/firmware.elf \
//                      --duration 10 \
//                      --out boot.png
//
// Implementation notes:
//   * No Tauri / no webview. The whole pipeline is the QEMU process plus
//     a single Tokio task that reads the chardev socket.
//   * IPC frame format mirrors simulator/src-tauri/src/ipc.rs (8-byte
//     header, channel byte, length-prefixed payload). Only channel 0x01
//     framebuffer is consumed; other channels are drained and discarded.
//   * On exit (after `--duration` seconds), QEMU is killed and the most
//     recent framebuffer is written to `--out`. If no framebuffer was ever
//     received, exits with code 2 and writes nothing.

use std::path::{Path, PathBuf};
use std::process::Stdio;
use std::time::Duration;

use anyhow::{anyhow, Context, Result};
use clap::Parser;
use image::{GrayImage, ImageBuffer, Luma};
use tokio::io::AsyncReadExt;
use tokio::net::UnixStream;
use tokio::process::Command;
use tokio::sync::mpsc;
use tokio::time;

const PANEL_WIDTH: u32 = 800;
const PANEL_HEIGHT: u32 = 480;
const FB_BYTES: usize = (PANEL_WIDTH as usize * PANEL_HEIGHT as usize) / 8;

const HEADER_LEN: usize = 8;
const MAX_PAYLOAD: u32 = 1 << 20;

const CHANNEL_FRAMEBUFFER: u8 = 0x01;
const CHANNEL_SERIAL_LOG: u8 = 0x02;

#[derive(Parser, Debug)]
#[command(version, about = "Mofei simulator headless screenshot harness")]
struct Args {
    /// Path to the firmware ELF or BIN to boot in QEMU.
    #[arg(long)]
    firmware: PathBuf,

    /// Path to the espressif/qemu fork's `qemu-system-xtensa` binary.
    #[arg(long, default_value = "simulator/.qemu-cache/qemu/build/qemu-system-xtensa")]
    qemu: PathBuf,

    /// IPC chardev socket path. Will be removed on start if it exists.
    #[arg(long, default_value = "/tmp/mofei-sim-headless.sock")]
    socket: PathBuf,

    /// Number of seconds to run before screenshotting and exiting.
    #[arg(long, default_value_t = 10)]
    duration: u64,

    /// Output PNG path.
    #[arg(long)]
    out: PathBuf,

    /// Echo firmware UART output to stderr (off by default — keep CI quiet).
    #[arg(long)]
    serial: bool,
}

#[tokio::main]
async fn main() -> Result<()> {
    let args = Args::parse();

    if !args.qemu.exists() {
        return Err(anyhow!(
            "QEMU binary not found at {}. Run scripts/build-qemu.sh first.",
            args.qemu.display()
        ));
    }
    if !args.firmware.exists() {
        return Err(anyhow!(
            "Firmware not found at {}",
            args.firmware.display()
        ));
    }

    if args.socket.exists() {
        std::fs::remove_file(&args.socket)
            .with_context(|| format!("removing stale socket {}", args.socket.display()))?;
    }

    let socket_arg = format!(
        "socket,id=mofei,path={},server=on,wait=off",
        args.socket.display()
    );

    let mut child = Command::new(&args.qemu)
        .arg("-machine").arg("esp32s3")
        .arg("-kernel").arg(&args.firmware)
        .arg("-nographic")
        .arg("-serial").arg(if args.serial { "stdio" } else { "null" })
        .arg("-chardev").arg(&socket_arg)
        .stdin(Stdio::null())
        .stdout(if args.serial { Stdio::inherit() } else { Stdio::null() })
        .stderr(Stdio::inherit())
        .spawn()
        .with_context(|| format!("spawning {}", args.qemu.display()))?;

    let (fb_tx, mut fb_rx) = mpsc::unbounded_channel::<Vec<u8>>();
    let socket_path = args.socket.clone();
    let reader_handle = tokio::spawn(async move {
        // Allow QEMU a moment to bind the socket.
        for _ in 0..40u32 {
            match UnixStream::connect(&socket_path).await {
                Ok(stream) => return read_loop(stream, fb_tx).await,
                Err(_) => time::sleep(Duration::from_millis(100)).await,
            }
        }
        Err(anyhow!("ipc socket never appeared at {}", socket_path.display()))
    });

    // Capture framebuffers until the duration elapses or QEMU exits.
    let deadline = time::Instant::now() + Duration::from_secs(args.duration);
    let mut last_fb: Option<Vec<u8>> = None;
    let mut frames_seen: u64 = 0;
    loop {
        tokio::select! {
            _ = time::sleep_until(deadline) => break,
            maybe_fb = fb_rx.recv() => {
                match maybe_fb {
                    Some(fb) => { last_fb = Some(fb); frames_seen += 1; }
                    None => break,
                }
            }
            status = child.wait() => {
                eprintln!("[headless] QEMU exited with {:?}", status);
                break;
            }
        }
    }

    // Stop QEMU and reader.
    let _ = child.kill().await;
    let _ = child.wait().await;
    let _ = std::fs::remove_file(&args.socket);
    let _ = reader_handle.abort();

    eprintln!("[headless] frames received: {frames_seen}");
    let fb = last_fb.ok_or_else(|| anyhow!("no framebuffer received before deadline"))?;
    write_png(&args.out, &fb).with_context(|| format!("writing {}", args.out.display()))?;
    eprintln!("[headless] screenshot written: {}", args.out.display());
    Ok(())
}

async fn read_loop(
    mut stream: UnixStream,
    fb_tx: mpsc::UnboundedSender<Vec<u8>>,
) -> Result<()> {
    let mut header = [0u8; HEADER_LEN];
    loop {
        if let Err(e) = stream.read_exact(&mut header).await {
            // Treat clean EOF as not-an-error for CI loops.
            if e.kind() == std::io::ErrorKind::UnexpectedEof
                || e.kind() == std::io::ErrorKind::BrokenPipe
            {
                return Ok(());
            }
            return Err(e.into());
        }
        let channel = header[0];
        let _flags = header[1];
        let payload_len =
            u32::from_le_bytes([header[4], header[5], header[6], header[7]]);
        if payload_len > MAX_PAYLOAD {
            return Err(anyhow!("payload_len {payload_len} exceeds cap"));
        }
        let mut payload = vec![0u8; payload_len as usize];
        if payload_len > 0 {
            stream.read_exact(&mut payload).await?;
        }
        match channel {
            CHANNEL_FRAMEBUFFER => {
                if payload.len() == FB_BYTES {
                    if fb_tx.send(payload).is_err() {
                        return Ok(());
                    }
                } else {
                    eprintln!(
                        "[headless] unexpected framebuffer length {} (expected {})",
                        payload.len(),
                        FB_BYTES
                    );
                }
            }
            CHANNEL_SERIAL_LOG => { /* ignored unless --serial; QEMU's stdio handles that */ }
            _ => { /* drain other channels */ }
        }
    }
}

/// Write an 800×480 1bpp framebuffer as a PNG. Bit=1 → white (255), bit=0
/// → black (0). MSB-first within each byte.
fn write_png(path: &Path, fb: &[u8]) -> Result<()> {
    let mut img: GrayImage = ImageBuffer::new(PANEL_WIDTH, PANEL_HEIGHT);
    for byte_idx in 0..fb.len() {
        let b = fb[byte_idx];
        let base = (byte_idx * 8) as u32;
        for bit in 0..8u32 {
            let pixel = base + bit;
            let x = pixel % PANEL_WIDTH;
            let y = pixel / PANEL_WIDTH;
            if y >= PANEL_HEIGHT {
                break;
            }
            let v: u8 = if (b >> (7 - bit)) & 1 != 0 { 255 } else { 0 };
            img.put_pixel(x, y, Luma([v]));
        }
    }
    img.save(path)?;
    Ok(())
}
