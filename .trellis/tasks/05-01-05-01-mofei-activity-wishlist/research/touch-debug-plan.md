# Touch hit-area debugging plan

Date: 2026-05-03
Owner: Codex

## Problem

User reports the on-screen UI touch zones still feel "不准确和失真" — taps register
at a position that doesn't match the visual element underneath, or no element
fires at all.

The MofeiTouch driver itself has been confirmed functional (FT6336U detected at
`addr=0x2E SDA=13 SCL=12`, `frame status=0x00 points=N` events flow at boot).
Therefore the bug lies somewhere in the chain:

```
FT6336U raw (x_raw, y_raw)            // captured by MofeiTouch.cpp
        │
        ▼  apply scaling + axis swap/invert via MOFEI_TOUCH_* macros
HalGPIO::touchEvent (x_logical, y_logical) // logical 480×800 portrait coords
        │
        ▼  MappedInputManager::consumeTouchEvent(&e, renderer)
        ▼  TouchHitTest::eventForRendererOrientation(e, renderer)
oriented event (x_oriented, y_oriented)    // matches renderer orientation
        │
        ▼  Activity hit-test: gridCellAt / pointInRect / readerActionForTouch
visual element at (x_oriented, y_oriented)
```

Several known coordinate-system seams exist:

1. **Mofei panel is physical 800×480** (landscape). Logical screen depends on
   `renderer.getOrientation()`.
2. **`MOFEI_TOUCH_LOGICAL_WIDTH/HEIGHT` are 480/800** (portrait native). When
   the renderer is in `Portrait`, the touch logical frame matches and
   `eventForRendererOrientation` is a no-op. In any other orientation, the
   logical coords get rotated/inverted.
3. **Button hint hit rects** (`MOFEI_TOUCH_BUTTON_HINT_RECTS` in
   `lib/hal/HalGPIO.cpp`) live in `event.sourceX/Y` (raw, pre-orientation)
   space. Visual button positions (`x4ButtonPositions[]` in
   `LyraTheme::drawButtonHints`) are computed AFTER `setOrientation(Portrait)`.
   These two coordinate systems can drift if the active orientation differs.
4. **Hit-test functions are mixed**: dashboard uses `gridCellAt` against
   `cellRects` computed from `renderer.getScreenWidth/Height` (oriented), but
   button-hint dispatch uses `event.sourceX/Y` (raw). Activities mixing both
   can mishit when the user is not in `Portrait`.

## Build target

A new env `mofei_touch_debug` was added in `platformio.ini`:

```bash
pio run -e mofei_touch_debug -t upload --upload-port=/dev/cu.usbmodem*
pio device monitor --port /dev/cu.usbmodem* --baud 115200
```

The flag `-DMOFEI_TOUCH_DEBUG=1` activates `LOG_INF("TOUCHDBG", ...)` inside
`MappedInputManager::consumeTouchEvent(InputTouchEvent* e, const GfxRenderer&
renderer)` so every consumed touch event prints a line like:

```
[TOUCHDBG] type=1 raw=(305,612) oriented=(305,612) screen=480x800 orient=0 hintTap=0
```

Where:
- `type` matches `InputTouchEvent::Type` (1=Tap, 2=SwipeLeft, 3=Right, 4=Up,
  5=Down).
- `raw` is the FT6336U logical coordinate (post `MOFEI_TOUCH_*` scaling, but
  pre `eventForRendererOrientation`).
- `oriented` is the value handed to the activity (post orientation transform).
- `screen` is `renderer.getScreenWidth()` × `getScreenHeight()` for cross-check.
- `orient` is the `GfxRenderer::Orientation` enum value.
- `hintTap` is non-zero when the raw coordinate lies inside the button-hint
  zone (`MOFEI_TOUCH_BUTTON_HINT_RECTS`).

## Step-by-step procedure

For each of the following test points, tap the screen at the location, capture
the corresponding `[TOUCHDBG]` log line, and compare to the expected ranges.

### A. Top-left corner

Expected: `raw ≈ (0..30, 0..30)` and `oriented` matches under `orient=0`.
If `oriented` does NOT match (e.g. `oriented=(770, 0)`), the orientation
transform is mis-rotating.

### B. Bottom-right corner of the button hint bar

Tap the visual rightmost button (Right arrow). Expected:
- `raw.y` ∈ `[752, 800)` (touch hint Y zone is `LOGICAL_HEIGHT - 48`).
- `raw.x` ∈ `[351, 460]` (`MOFEI_TOUCH_BUTTON_HINT_RECTS[3] = {x=351, w=109}`).
- `hintTap=1`.

If `hintTap=0` despite a clearly-button tap, either the FT6336U reports
out-of-band raw values or the rect array is stale relative to the visual.

### C. Center of every grid cell on Dashboard

For Dashboard's iOS-grid layout (3 cols × N rows), tap the center of each
cell and record `oriented`. Compare with the per-cell `Rect` printed by
`DashboardActivity::layoutCells` — add a one-shot `LOG_INF("DASH", ...)` of
the array if it isn't already there.

A drift between the tapped center and the cell `Rect` indicates either:
- Cell layout uses different `pageWidth/Height` than the touch transform.
- `eventForRendererOrientation` is using `getScreenWidth/Height` (oriented)
  but the dashboard rect math used a stale value.

### D. Edges and gaps

Tap exactly on a 6 px gap between two grid cells. Expect "no action" — but
if the surrounding cell's selection still fires, `gridCellAt` is rounding
the gap into the closer cell. This is acceptable visually but worth noting.

### E. Orientation rotation

If the device supports rotation, force each orientation
(`Portrait`, `LandscapeClockwise`, `LandscapeCCW`, `PortraitInverted`) and
repeat A–D. Symptom of the orientation transform bug is asymmetric mismatch
between raw and oriented under non-Portrait modes.

## Visual overlay (next-step option)

If the log is not enough, add a build-flag-gated debug overlay that draws
a small crosshair (and the raw/oriented coordinate text) at the tap location
on every render frame. Sketch:

```cpp
#if MOFEI_TOUCH_DEBUG
namespace TouchDebugOverlay {
  static InputTouchEvent lastEvent{};
  static unsigned long lastEventMs = 0;
  inline void record(const InputTouchEvent& e) {
    lastEvent = e;
    lastEventMs = millis();
  }
  inline void draw(GfxRenderer& renderer) {
    if (lastEvent.type == InputTouchEvent::Type::None) return;
    if (millis() - lastEventMs > 4000) return;  // fade after 4s
    renderer.drawLine(lastEvent.x - 8, lastEvent.y, lastEvent.x + 8, lastEvent.y, 1, true);
    renderer.drawLine(lastEvent.x, lastEvent.y - 8, lastEvent.x, lastEvent.y + 8, 1, true);
    char buf[48];
    snprintf(buf, sizeof(buf), "raw=%u,%u or=%u,%u",
             lastEvent.rawX, lastEvent.rawY, lastEvent.x, lastEvent.y);
    renderer.drawText(SMALL_FONT_ID, lastEvent.x + 10, lastEvent.y - 6, buf);
  }
}
#endif
```

`MappedInputManager::consumeTouchEvent` would call `record(...)` after the
orientation transform, and every activity's `render()` would call
`TouchDebugOverlay::draw(renderer)` as the last step before
`displayBuffer()`. The overlay vanishes after 4 s so the screen returns to
normal once you stop tapping.

## Outputs to produce

After running the debug build:
1. Serial log capture covering test points A–E (paste into this file).
2. A short table mapping each visual element → `(x_oriented, y_oriented)
   range observed` → `(rect.x, rect.y, rect.width, rect.height) expected`.
3. Identification of which seam (raw scaling, orientation transform, hint
   rect, activity hit-test) is responsible for the observed drift.

The fix should then target ONE seam per commit so the change is bisectable.
