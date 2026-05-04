# QEMU Custom Peripheral ↔ Tauri Host IPC

Status: Web research blocked (upstream API outage 2026-05-04). Conclusions
are training-knowledge based, marked `[needs verification]` where they
depend on specific QEMU API surfaces. Architectural recommendations are
sound regardless.
Date: 2026-05-04

## Executive Summary

For our outbound (QEMU → Tauri) traffic, the dominant payload is the e-ink
framebuffer: 48 KB on full update at ≤ 10 Hz peak. For inbound (Tauri →
QEMU), touch and button events at ≤ 60 Hz, < 32 bytes each. **Both
directions fit comfortably inside a single Unix-domain-socket, length-
prefixed binary frame protocol** routed through a QEMU `chardev` socket.
Shared memory adds complexity without performance benefit at these
volumes.

**Recommendation: Approach A — Single Unix Socket, Length-Prefixed Multiplex.**
Effort: ~1 day to wire up.

## Comparison Table

`[QEMU API specifics need verification]`

| Mechanism | Throughput | Latency | Portability | QEMU integration | Tauri integration |
|---|---|---|---|---|---|
| QEMU `chardev` Unix socket | ~1 GB/s practical | < 1 ms | macOS+Linux ✓ | First-class API | Standard `tokio::net::UnixStream` |
| QEMU QMP | ~10 MB/s (JSON) | 5–10 ms | ✓ | Built-in | JSON parser |
| POSIX shm + sem | ~10 GB/s | < 100 µs | ✓ (no Win32 worry) | DIY `memfd_create` | `nix` crate |
| Raw Unix socket (no QEMU framing) | same as chardev | same | ✓ | Need custom backend | same as chardev |
| gRPC over UDS | ~500 MB/s | 1–2 ms | ✓ | Heavy | Heavy |
| WebSocket | ~100 MB/s | 2–5 ms | ✓ | Awkward | Native in Tauri but weird here |
| stdin/stdout chardev | ~1 GB/s | < 1 ms | ✓ | Trivial | Less clean (stderr collision) |
| vhost-user | huge | < 1 ms | ✓ | Heavy | Heavy |

## Architecture Options

### Approach A: Single Chardev Unix Socket, Length-Prefixed Multiplex (Recommended)

**Layout:**
- QEMU launched with: `-chardev socket,id=mofei,path=/tmp/mofei.sock,server=on,wait=off -device …,chardev=mofei`
- Both our virtual e-ink peripheral and our virtual FT6336U attach to the
  same chardev backend, OR (cleaner) we add a tiny multiplexer device that
  fans out by channel tag.
- Tauri Rust backend: `tokio::net::UnixStream::connect("/tmp/mofei.sock")`,
  framed reader.

**Frame format proposal:**

```
struct Frame {
    u8  channel;     // 0x01 = framebuffer, 0x02 = serial-log,
                     // 0x03 = gpio-state, 0x04 = touch-event,
                     // 0x05 = button-event, 0x06 = control,
                     // 0xFF = error/heartbeat
    u8  flags;       // bit 0 = full update vs partial, etc.
    u16 reserved;
    u32 payload_len; // little-endian
    u8  payload[payload_len];
}  // header is 8 bytes; max payload_len 1 MB enforced.
```

Channel-specific payloads:
- `0x01 framebuffer`: full = 48000 bytes 1bpp; partial = `(x0,y0,x1,y1,bytes...)`
- `0x02 serial-log`: UTF-8 bytes from UART chardev
- `0x03 gpio-state`: `(gpio:u16, level:u8)` triplets
- `0x04 touch-event` (inbound): `(action:u8 [DOWN/MOVE/UP], x:u16, y:u16, finger_id:u8)`
- `0x05 button-event` (inbound): `(button:u8, pressed:u8)`
- `0x06 control` (bidi): `(verb:u8, arg:bytes)` — reset, hardware-refresh, etc.

**Pros**:
- One socket, one connection lifecycle.
- Simple framing; ~30 lines of Rust on each side.
- Bidirectional through the same UDS.
- Trivial to log/replay traffic for regression tests.

**Cons**:
- Single-channel head-of-line blocking — but at our volumes (48 KB / 100 ms)
  this is irrelevant.

### Approach B: shm Framebuffer + Unix Socket Events

**Layout:**
- One `memfd`-backed shared mapping for the framebuffer (48 KB or 96 KB
  double-buffered).
- One Unix socket for everything else.

**Pros**:
- Zero-copy framebuffer; theoretical < 100 µs latency.

**Cons**:
- Requires QEMU peripheral to integrate with `memfd_create` and pass an FD
  to the Tauri side via SCM_RIGHTS — extra plumbing.
- macOS lacks `memfd_create`; portable POSIX shm path differs and adds
  cross-platform complexity.
- 48 KB at 10 Hz is 480 KB/s — UDS handles this with two orders of
  magnitude headroom.

**When to consider**: only if profiling later reveals socket throughput is a
real bottleneck. **Not recommended for PR1.**

### Approach C: stdio Chardev (Simple, No Socket Files)

QEMU started with `-serial stdio` or `-chardev stdio,id=mofei` and our Tauri
process spawns QEMU with piped stdin/stdout, reading frames from stdout.

**Pros**:
- No socket cleanup, no `/tmp/` permission issues, no port conflicts.
- Already partially scaffolded in `simulator/src-tauri/src/lib.rs` (which
  uses `std::process::Command::spawn`).

**Cons**:
- stdout collides with QEMU's own monitor / serial console output unless we
  carefully separate.
- Backpressure model is murky.

Hybrid pragmatic plan: **Use stdio for serial-log and stderr for QEMU's own
diagnostics; use a separate chardev Unix socket for our binary protocol.**

## Wokwi / Renode Patterns

`[needs verification]`

- **Wokwi custom-chips API**: peripheral implemented in C compiled to wasm,
  callbacks for `chipInit`, `pinChange`, `spiConnect`, `i2cConnect`, etc.
  Custom peripheral state lives in JS host; data flows via callback.
  Not directly applicable to QEMU but shows the channel taxonomy: SPI,
  I²C, GPIO, timer, attribute (config) — exactly matches what we need to
  multiplex.
- **Renode**: peripheral implemented in C# subclassing
  `Antmicro.Renode.Peripherals.IPeripheral`; cross-process communication via
  the Renode "machine monitor" socket protocol (text-based). Useful for
  understanding the abstraction split, less useful as direct prior art for
  binary IPC.

## QEMU Memory-Region Sharing

`[needs verification]` QEMU does support `memory_region_init_ram_from_fd`
which can map a file or memfd into guest RAM and the host process can
`mmap` the same fd. This is what `vhost-user` builds on. For our framebuffer
(which lives in *host-side* virtual peripheral memory, NOT guest RAM),
we'd use plain POSIX shm or memfd — not QEMU's memory-region machinery.
Not strictly necessary for PR1.

## Single-Channel vs Multi-Channel

**Single channel with a tag prefix is correct** for our scale. Multi-channel
adds:
- More socket cleanup paths
- More open-FD tracking on Tauri side
- More complex shutdown ordering

The only reason to split is if one channel has very different latency
sensitivity than another *and* the volume is high enough that multiplex
HoL blocking matters. Touch (60 Hz × 8 bytes) + framebuffer (10 Hz × 48 KB)
on a UDS does not blocking-conflict.

## Recommendation

**Approach A: Single chardev Unix socket, length-prefixed binary multiplex,
8-byte header + payload.** Reuse the existing `simulator/src-tauri/src/lib.rs`
`start_sim` slot to spawn QEMU with the right `-chardev` flags. Open the
socket on the Tauri side, push frames to the React UI via Tauri events.

## Citations / To Verify Post-Outage

- https://www.qemu.org/docs/master/system/devices/serial.html
- https://www.qemu.org/docs/master/qmp-spec.html
- https://www.qemu.org/docs/master/system/qemu-manpage.html (chardev
  argument forms)
- https://docs.wokwi.com/api/custom-chips-api
- https://renode.readthedocs.io/en/latest/

All `[needs verification]` items to be re-checked when WebFetch returns,
*before* committing this PRD's ADR section.
