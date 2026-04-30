# Touch Event Architecture Notes

Date: 2026-04-30
Owner: Codex

## Current Input Flow

The repository still treats input as physical buttons first:

- `HalGPIO` polls physical buttons and exposes `wasPressed`, `wasReleased`, `isPressed`, and held-time state.
- `MappedInputManager` maps logical button roles to hardware button indices.
- `ButtonNavigator` wraps `MappedInputManager` for repeated list/menu navigation.
- Activities receive `MappedInputManager` and mostly express UI actions through button states.

Mofei touch currently enters through `HalGPIO`:

- `MofeiTouchDriver` emits chip-specific `MofeiTouchDriver::Event` values.
- `HalGPIO::updateMofeiTouch()` stores one pending raw touch event.
- The same method maps touch gestures/tap zones into synthetic button press/release bits for legacy screens.
- `consumeMofeiTouchEvent()` exposes the raw chip event only to Mofei-aware code.

This means button-first screens work through virtual buttons, but coordinate-aware touch cannot scale without leaking
`MofeiTouchDriver` into every screen.

## Existing Native Consumers

`DashboardActivity` has direct `#if MOFEI_DEVICE` code that consumes `MofeiTouchDriver::Event` and hit-tests list rows.
This proves native touch is useful, but it is a one-off implementation.

`KeyboardEntryActivity` consumes raw touch coordinates for keyboard hit-testing. It should keep its domain-specific
coordinate logic, but the input event source should become device-independent.

## Target Contract

Add a generic touch event at the HAL/input boundary. Suggested fields:

- event kind: `None`, `Tap`, `SwipeLeft`, `SwipeRight`, `SwipeUp`, `SwipeDown`
- current/end coordinates: `x`, `y`
- optional start coordinates or deltas when needed by reader/page gestures

`MofeiTouchDriver::Event` should be converted once inside HAL/input code. Activities should include the generic input
header and should not include `MofeiTouch.h`.

## Consumption Policy

The firmware needs two compatibility paths:

1. Native coordinate/gesture handling for touch-display screens.
2. Synthetic button fallback for screens that have not opted into native touch yet.

A touch event must not trigger both paths for the same screen loop. The safest implementation path is:

- Preserve the existing synthetic button adapter for broad compatibility.
- Add a way for native consumers to consume the pending generic touch event before button fallback is evaluated.
- For the first implementation slice, screens that explicitly consume native touch should return immediately after
  handling it, matching the existing Dashboard/KeyboardEntry behavior.
- A later cleanup can move fallback injection behind Activity-level dispatch once every important screen has a touch hook.

## Shared Hit-Testing

Repeated list/menu screens should not copy Dashboard's row math. Add a helper that accepts:

- list `Rect`
- row height
- selected index
- item count
- tap `x/y`

The helper should return the tapped absolute item index when the point is inside a visible row. It should use renderer
metrics and existing `Rect` values instead of hardcoding display dimensions.

Reader screens need a different helper:

- left zone: previous page
- right zone: next page
- center zone: menu/action
- swipe left/up: next
- swipe right/down: previous

## Implementation Order

1. Create generic `TouchEvent` and expose it through `MappedInputManager`.
2. Convert Mofei chip events to generic events in `HalGPIO`.
3. Move Dashboard and KeyboardEntry to the generic event source.
4. Add shared list hit-testing and migrate hub/list screens incrementally.
5. Add reader tap-zone/swipe support.
6. Keep physical button support unchanged and run `pio run -e mofei` after each bounded slice.
