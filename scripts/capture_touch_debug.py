#!/usr/bin/env python3
"""
Capture [TOUCHDBG] serial log from the Mofei device while the user runs the
repro matrix from research/touch-diagnostics-debug-plan.md.

Usage:
  python3 scripts/capture_touch_debug.py \
      --port /dev/cu.usbmodem1101 \
      --out .trellis/tasks/05-04-05-04-touch-coord-fix/research/touch-debug-capture.md \
      --duration 180

Defaults:
  - port: /dev/cu.usbmodem1101
  - duration: 180 s
  - out: prints to stdout if --out is not given

Stops cleanly on SIGINT (Ctrl-C). Each captured TOUCHDBG line is written with
a millisecond-since-start prefix so the user can correlate the log with
their tap sequence.
"""
from __future__ import annotations

import argparse
import datetime as dt
import os
import signal
import sys
import time

try:
    import serial  # type: ignore
except ImportError:
    print("Missing pyserial. Install with: pip install pyserial", file=sys.stderr)
    sys.exit(1)


_running = True


def _on_sigint(_sig, _frm):
    global _running
    _running = False


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbmodem1101")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--duration", type=int, default=180, help="seconds")
    ap.add_argument("--out", default=None)
    ap.add_argument(
        "--filter", default="TOUCHDBG", help="substring filter for kept lines"
    )
    ap.add_argument(
        "--no-reset",
        action="store_true",
        help="skip the DTR/RTS reset toggle (use when device is mid-session)",
    )
    args = ap.parse_args()

    signal.signal(signal.SIGINT, _on_sigint)

    out_path = None
    if args.out:
        out_path = os.path.abspath(args.out)
        os.makedirs(os.path.dirname(out_path), exist_ok=True)

    try:
        s = serial.Serial(args.port, args.baud, timeout=0.3)
    except Exception as exc:
        print(f"OPEN_FAIL: {exc}", file=sys.stderr)
        return 1

    if not args.no_reset:
        s.dtr = False
        s.rts = True
        time.sleep(0.1)
        s.rts = False
        time.sleep(0.1)

    started_wall = dt.datetime.now()
    started_ms = time.monotonic()
    deadline = started_ms + args.duration

    header = (
        f"# Touch debug capture\n\n"
        f"Captured: {started_wall.isoformat(timespec='seconds')}\n"
        f"Port: {args.port}\n"
        f"Duration: {args.duration}s\n"
        f"Filter: {args.filter}\n\n"
        f"## Repro matrix (run in this order)\n\n"
        f"- **A** Tap each of the 4 screen corners\n"
        f"- **B** Tap each of the 4 button-hint slots in the bottom bar\n"
        f"- **C** Tap the center of every grid cell on Dashboard\n"
        f"- **D** Tap exactly on a 6 px gap between two grid cells\n"
        f"- **E** Repeat A-D after rotating the device (settings -> orientation)\n\n"
        f"## Captured lines (`[{args.filter}]` only)\n\n"
        f"```text\n"
    )

    sink = open(out_path, "w") if out_path else sys.stdout
    sink.write(header)
    sink.flush()

    print(
        f"=== capturing {args.filter} from {args.port} for {args.duration}s "
        "(Ctrl-C to stop early) ===",
        file=sys.stderr,
    )

    kept = 0
    while _running and time.monotonic() < deadline:
        line = s.readline()
        if not line:
            continue
        try:
            text = line.decode("utf-8", errors="replace").rstrip()
        except Exception:
            text = repr(line)
        if args.filter in text:
            elapsed_ms = int((time.monotonic() - started_ms) * 1000)
            sink.write(f"+{elapsed_ms:>7d}ms  {text}\n")
            sink.flush()
            kept += 1
            print(f"+{elapsed_ms:>7d}ms  {text}", file=sys.stderr)

    footer = f"```\n\n## Summary\n\nKept {kept} `[{args.filter}]` lines.\n"
    sink.write(footer)
    if sink is not sys.stdout:
        sink.close()
    s.close()

    print(f"=== done. kept {kept} lines. ===", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
