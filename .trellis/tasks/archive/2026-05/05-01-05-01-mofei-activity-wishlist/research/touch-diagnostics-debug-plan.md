# Mofei Touch Diagnostics Debug Plan

Date: 2026-05-04
Owner: Codex

## Goal

Use a low-noise, opt-in trace to compare Mofei raw touch events against renderer-oriented coordinates and the final
activity gesture mapping. This should isolate whether a bad tap/swipe is caused by the FT6336U frame, HAL conversion,
orientation transform, button-hint fallback, or activity hit-testing.

## Runtime Hook

`MappedInputManager::consumeTouchEvent(outEvent, renderer)` contains the gated `TOUCHDBG` serial log. It is disabled by
default and only compiles when `MOFEI_TOUCH_DEBUG=1` is set.

Build a diagnostic image with a temporary project option:

```bash
rtk pio run -e mofei --project-option "build_flags=${env:mofei.build_flags} -DMOFEI_TOUCH_DEBUG=1"
```

If shell expansion makes the `${env:mofei.build_flags}` text hard to pass safely, add
`-DMOFEI_TOUCH_DEBUG=1` temporarily to a local diagnostic environment and do not commit that environment unless it is
intentionally reused.

## Serial Evidence To Capture

For each repro gesture, capture:

- `TOUCHDBG raw_type=... oriented_type=... raw=(x,y) oriented=(x,y) screen=... orient=...`
- Existing `TOUCH event=... button=...` lines from `HalGPIO`
- The current activity name and expected action

## Repro Matrix

1. Portrait dashboard: tap each corner cell, center cell, customize row, and footer button hints.
2. Portrait reader: tap left/center/right thirds and swipe left/right/up/down.
3. Rotated reader: repeat reader tests for each renderer orientation.
4. Settings/list activity: tap first visible row, last visible row, blank list space, and footer hints.

## Decision Table

| Symptom | Likely Layer | Next Check |
| --- | --- | --- |
| Raw coordinates are wrong before orientation | FT6336U/HAL | Inspect `MofeiTouch` frame validation and normalization |
| Raw is correct, oriented is mirrored/flipped | Orientation transform | Fix `TouchHitTest::eventForRendererOrientation` only |
| Oriented is correct, footer hint button wrong | HAL hint map | Fix `MOFEI_TOUCH_BUTTON_HINT_RECTS` only |
| Oriented is correct, activity action wrong | Activity hit-test | Fix that activity's `TouchHitTest` usage |

Do not compensate in activity code for a lower-layer orientation bug. Keep one owner per mapping stage.
