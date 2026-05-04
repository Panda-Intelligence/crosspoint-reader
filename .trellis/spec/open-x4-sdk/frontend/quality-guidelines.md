# Quality Guidelines

> Code quality standards for frontend development.

---

## Overview

<!--
Document your project's quality standards here.

Questions to answer:
- What patterns are forbidden?
- What linting rules do you enforce?
- What are your testing requirements?
- What code review standards apply?
-->

(To be filled by the team)

---

## Forbidden Patterns

<!-- Patterns that should never be used and why -->

(To be filled by the team)

---

## Required Patterns

<!-- Patterns that must always be used -->

### Preserve Dashboard Shortcut Persistence Compatibility

When changing Mofei Dashboard shortcuts, keep the persisted shortcut key/id contract separate from the visible slot
contract.

#### 1. Scope / Trigger

Use this rule whenever changing `DashboardShortcutId`, default dashboard shortcuts, dashboard rendering/opening logic, or
`DashboardShortcutStore` repair behavior.

#### 2. Signatures

```cpp
enum class DashboardShortcutId : uint8_t;
static constexpr size_t DashboardShortcutStore::SLOT_COUNT;
bool DashboardShortcutStore::isAvailable(DashboardShortcutId id);
const DashboardShortcutDefinition* DashboardShortcutStore::definitionFor(DashboardShortcutId id);
bool DashboardShortcutStore::idFromKey(const char* key, DashboardShortcutId* outId);
```

#### 3. Contracts

- `SLOT_COUNT` is the visible Dashboard slot count, not automatically the enum count.
- Persisted string keys in `shortcuts.json` may outlive visible shortcuts.
- Retired shortcuts should remain parseable as legacy keys/ids when needed, but `isAvailable()` must exclude them from
  defaults, rendering, cycling, and repaired lists.
- Dashboard open/render code must consume only available shortcuts from the normalized store.

#### 4. Validation & Error Matrix

| Condition | Required behavior |
| --- | --- |
| Stored config contains retired shortcut key/id | Repair to defaults or first available unused shortcut |
| Stored config has wrong slot count | Restore and save defaults |
| Shortcut id is valid enum but retired | `definitionFor()` returns `nullptr`; store repair excludes it |
| Visible shortcut is File Browser or Settings | Keep protected replacement behavior |

#### 5. Good/Base/Bad Cases

- Good: Keep `DesktopHub` as a legacy enum/key while excluding it via `isAvailable()`.
- Base: Add a new visible shortcut by updating definition, key, default list, and repair/cycle availability together.
- Bad: Renumber enum values or tie `SLOT_COUNT` directly to `DashboardShortcutId::Count` after retiring visible items.

#### 6. Tests Required

- Run `git diff --check`.
- Run `pio run -e mofei`.
- Run `pio check -e mofei --fail-on-defect low --fail-on-defect medium --fail-on-defect high` when shortcut repair,
  Dashboard touch handling, or rendering changes.

#### 7. Wrong vs Correct

Wrong:

```cpp
static constexpr size_t SLOT_COUNT = static_cast<size_t>(DashboardShortcutId::Count);
```

Correct:

```cpp
static constexpr size_t SLOT_COUNT = 9;
bool DashboardShortcutStore::isAvailable(const DashboardShortcutId id) {
  return isValid(id) && id != DashboardShortcutId::DesktopHub;
}
```

### Keep One Owner Per Touch Coordinate Stage

A Mofei touch event passes through five distinct coordinate transformations
between the FT6336U panel and the activity that handles it. Each stage owns
exactly one transformation. Higher layers must not paper over a bug in a
lower layer; the fix belongs to the layer where the misalignment is
introduced. The `mofei_touch_debug` build emits per-event traces so a real
tap can be attributed to one stage instead of guessing.

#### 1. Scope / Trigger

Use this rule whenever changing touch event flow: HAL conversion, orientation
transforms, button-hint hit rects, activity hit-testing, or new activities
that consume `InputTouchEvent`.

#### 2. Signatures

```cpp
// Stage 1 — FT6336U raw frame
bool MofeiTouchDriver::update(MofeiTouchDriver::Event* outEvent);

// Stage 2 — HAL conversion (axis macros applied: MOFEI_TOUCH_SWAP_XY,
// MOFEI_TOUCH_INVERT_X, MOFEI_TOUCH_INVERT_Y, scale to LOGICAL_WIDTH/HEIGHT)
InputTouchEvent toInputTouchEvent(const MofeiTouchDriver::Event& event);

// Stage 3 — orientation transform (rotates logical-portrait coords to active
// renderer orientation)
InputTouchEvent TouchHitTest::eventForRendererOrientation(const InputTouchEvent& event,
                                                           const GfxRenderer& renderer);
bool MappedInputManager::consumeTouchEvent(InputTouchEvent* outEvent,
                                            const GfxRenderer& renderer) const;

// Stage 4 — activity hit-test (oriented coords)
bool TouchHitTest::pointInRect(uint16_t x, uint16_t y, const Rect& rect);
bool TouchHitTest::gridCellAt(const Rect& gridRect, int rows, int cols,
                              uint16_t x, uint16_t y, int* outRow, int* outCol);
TouchHitTest::ReaderAction TouchHitTest::readerActionForTouch(const InputTouchEvent& event,
                                                               const Rect& screenRect);

// Stage 5 — button-hint fallback (raw, pre-orientation source coords)
bool HalGPIO::mapMofeiButtonHintTapToButton(uint16_t x, uint16_t y,
                                             uint8_t* outButtonIndex) const;
bool MappedInputManager::isTouchButtonHintTap(const InputTouchEvent& event) const;
```

#### 3. Contracts

- Stage 1 owns the FT6336U frame. Bad raw frames are rejected in
  `MofeiTouch.cpp` (e.g. AHT20-like frames). Activities never touch raw
  frames.
- Stage 2 owns the axis macros (`MOFEI_TOUCH_SWAP_XY`, `INVERT_X/Y`, scale to
  `MOFEI_TOUCH_LOGICAL_WIDTH/HEIGHT`). Output is logical-portrait 480×800.
  Activities never re-apply axis math.
- Stage 3 owns the rotation between logical-portrait and active renderer
  orientation. It is the ONLY place that knows about
  `GfxRenderer::Orientation`. Activities never read `getOrientation()` to
  compensate for tap coords.
- Stage 4 hit-tests in oriented coords. The activity owns its own rect math
  via `pointInRect` / `gridCellAt` / `readerActionForTouch`. Re-projecting
  oriented coords back is forbidden.
- Stage 5 button-hint fallback uses `event.sourceX/Y` (raw, pre-Stage-3) by
  design because the visible button bar is drawn after `setOrientation(Portrait)`.
  Anything the activity wants in oriented coords must use `event.x/event.y`.
- The `mofei_touch_debug` env (defined in `platformio.ini`) compiles
  `MOFEI_TOUCH_DEBUG=1`. This activates `[TOUCHDBG]` `LOG_INF` lines in
  `MappedInputManager::consumeTouchEvent(..., renderer)` and
  `[TOUCHDBG] dashboard_hit/dashboard_miss` traces in
  `DashboardActivity::loop`. Diagnostic logs must stay behind this flag.

#### 4. Validation & Error Matrix

| Symptom | Likely Stage | Fix Owner | Do NOT |
| --- | --- | --- | --- |
| Raw coords wrong before any activity | Stage 1 / 2 (FT6336U + HAL) | `MofeiTouch.cpp` frame validation, `MOFEI_TOUCH_*` axis macros | Adjust hit-test rects in activities |
| Raw correct, oriented mirrored or flipped | Stage 3 (orientation transform) | `TouchHitTest::eventForRendererOrientation` | Add `if (orientation == ...)` branches in activities |
| Oriented correct, button-hint button wrong | Stage 5 (HAL hint map) | `MOFEI_TOUCH_BUTTON_HINT_RECTS` in `lib/hal/HalGPIO.cpp` | Move hint logic to themes/activities |
| Oriented correct, activity action wrong | Stage 4 (activity hit-test) | That activity's `pointInRect` / `gridCellAt` rect math | Edit shared `TouchHitTest` helpers globally |
| Touch consumed but button event also fires | Missing `suppressTouchButtonFallback()` | The activity that consumed the touch | — |

#### 5. Good/Base/Bad Cases

- Good: A new activity uses `mappedInput.consumeTouchEvent(&e, renderer)`,
  hit-tests with `TouchHitTest::pointInRect(e.x, e.y, rect)`, and only reads
  `e.sourceX/Y` when explicitly checking against the button-hint zone via
  `mappedInput.isTouchButtonHintTap(e)`.
- Base: A new diagnostic site adds a `[TOUCHDBG]` `LOG_INF` line gated by
  `#if MOFEI_TOUCH_DEBUG`, mirroring the existing format
  `raw=(...) oriented=(...) screen=... orient=...`.
- Bad: An activity reads `event.x` and applies its own
  `if (renderer.getOrientation() == GfxRenderer::Landscape) x = w - x;`
  fixup. This hides a Stage 3 bug behind the activity and breaks every other
  consumer of the same broken transform.

#### 6. Tests Required

- `pio run -e mofei` (production build still compiles with debug flag absent).
- `pio run -e mofei_touch_debug` (debug build compiles with `MOFEI_TOUCH_DEBUG=1`).
- Flash the debug build and reproduce the Repro Matrix in
  `.trellis/tasks/05-01-05-01-mofei-activity-wishlist/research/touch-diagnostics-debug-plan.md`.
  For each tap, the captured `[TOUCHDBG]` line must show `oriented=` inside
  the rect of the visual element under the finger.
- For Mofei orientation rotation paths, capture serial and verify `oriented`
  matches the expected rect of the visual element under the finger across all
  active `GfxRenderer::Orientation` values used by the screen.

#### 7. Wrong vs Correct

Wrong:

```cpp
// Activity-level orientation compensation. Hides a Stage 3 bug.
InputTouchEvent event;
if (mappedInput.consumeTouchEvent(&event, renderer)) {
  uint16_t x = event.x;
  uint16_t y = event.y;
  if (renderer.getOrientation() == GfxRenderer::Orientation::LandscapeClockwise) {
    std::swap(x, y);  // patch the landscape bug here
  }
  if (TouchHitTest::pointInRect(x, y, cellRect)) { /* ... */ }
}
```

Correct:

```cpp
// Activity trusts Stage 3. If landscape coords are wrong, fix
// TouchHitTest::eventForRendererOrientation, not this call site.
InputTouchEvent event;
if (mappedInput.consumeTouchEvent(&event, renderer)) {
  if (TouchHitTest::pointInRect(event.x, event.y, cellRect)) { /* ... */ }
}
```

---

## Testing Requirements

<!-- What level of testing is expected -->

(To be filled by the team)

---

## Code Review Checklist

<!-- What reviewers should check -->

(To be filled by the team)
