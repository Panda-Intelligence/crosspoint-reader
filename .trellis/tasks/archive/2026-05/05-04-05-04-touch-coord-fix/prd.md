# Touch coordinate misalignment fix

## Goal

Validate (or refute) the user's reported feeling that on-screen UI touch
zones are inaccurate, and if real, fix it at the correct pipeline stage.
Avoid activity-level compensation per the spec rule
"Keep One Owner Per Touch Coordinate Stage" added in
`.trellis/spec/open-x4-sdk/frontend/quality-guidelines.md`.

## What I already know

- `MOFEI_TOUCH_DEBUG=1` build env (`mofei_touch_debug` in `platformio.ini`)
  emits `[TOUCHDBG]` log for every tap with raw + oriented + screen + orient
  + hint mapping fields.
- Research runbook (with repro matrix A–E and symptom-to-stage decision
  table) lives at
  `.trellis/tasks/archive/2026-05/05-01-05-01-mofei-activity-wishlist/research/touch-diagnostics-debug-plan.md`.
- Five-stage pipeline (FT6336U raw → HAL conversion → orientation transform
  → activity hit-test → optional button-hint fallback). Each stage has one
  canonical owner.
- Mofei panel physical 800×480 (landscape native). Logical-portrait 480×800
  is the HAL output frame. Orientation transform rotates to active renderer
  orientation.
- Spec rule: NEVER compensate at a higher stage for a lower-stage bug.

## Assumptions (temporary)

- Some real misalignment exists (user reports "似乎还是有不准确和失真的情况").
- The misalignment is reproducible by a deterministic tap sequence.
- One stage owns it; the fix lives in that stage's owner file only.

## Open Questions

- (Blocking) Is the device currently attached at `/dev/cu.usbmodem*` and
  ready to flash the debug build for data capture?
- (Preference) What is the minimum-acceptable verification gate after a
  fix? Options: (a) re-run repro matrix A–E in landscape + portrait,
  (b) full A–E plus dashboard customize navigation, (c) only verify the
  specific failing case that motivated the fix.

## Requirements (evolving)

- Capture `[TOUCHDBG]` logs covering at least: portrait dashboard 4 corners
  + center, button hint bar, customize footer (if visible), reader
  left/center/right thirds, list activity rows.
- For each tap, attribute the symptom to one of five stages using the
  decision table.
- If a stage is responsible: produce a fix limited to that stage's owner
  file.
- If no real misalignment is observed: close the task with a "no defect
  found" note and link the captured log as evidence.
- All commits build clean (`pio run -e mofei`) and `git diff --check`.

## Acceptance Criteria (evolving)

- [ ] Debug build flashed and ≥1 full repro pass captured to research/
- [ ] Symptom (if any) attributed to a single stage with evidence
- [ ] Fix (if needed) lives in only the responsible stage's owner file
- [ ] Re-flash with production env (`mofei`) and verify the failing case
      now hits the expected element

## Definition of Done

- Tests added/updated where applicable (no unit tests exist for touch
  pipeline today; capture log is the test artifact)
- Lint / typecheck / build green
- Spec rule followed: no activity-level orientation compensation
- Mofei flash verification per
  `.trellis/spec/open-x4-sdk/backend/quality-guidelines.md`

## Out of Scope (explicit)

- Adding new touch gestures (long-press, multi-touch, drag-and-drop)
- Changing FT6336U I2C / GPIO configuration
- Adding new orientations beyond the four already supported
- Touch performance / latency tuning

## Technical Notes

- Owner file by stage:
  - Stage 1 (FT6336U raw frame): `lib/hal/MofeiTouch.cpp`
  - Stage 2 (HAL conversion + axis macros): `MOFEI_TOUCH_*` in
    `lib/hal/MofeiTouch.h`, `toInputTouchEvent` in `lib/hal/HalGPIO.cpp`
  - Stage 3 (orientation transform):
    `TouchHitTest::eventForRendererOrientation` in `src/util/TouchHitTest.h`
  - Stage 4 (activity hit-test): the activity's own `gridCellAt` /
    `pointInRect` / `readerActionForTouch` callsite
  - Stage 5 (button-hint fallback): `MOFEI_TOUCH_BUTTON_HINT_RECTS` in
    `lib/hal/HalGPIO.cpp`
- Build commands:
  - `pio run -e mofei_touch_debug -t upload --upload-port=/dev/cu.usbmodem*`
  - `pio device monitor --port=/dev/cu.usbmodem* --baud=115200`
- Boot serial filter for capture: lines starting with `[TOUCHDBG]`

## Research References

- [`research/touch-debug-capture.md`](research/touch-debug-capture.md) — to
  be created after the debug build is flashed and tapped.
