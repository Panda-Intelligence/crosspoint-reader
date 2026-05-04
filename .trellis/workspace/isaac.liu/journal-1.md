# Journal - isaac.liu (Part 1)

> AI development session journal
> Started: 2026-04-28

---



## Session 1: Mofei wishlist: 9-grid dashboard, TC font hot-swap, touch debug

**Date**: 2026-05-04
**Task**: Mofei wishlist: 9-grid dashboard, TC font hot-swap, touch debug
**Package**: open-x4-sdk
**Branch**: `feat/murphy`

### Summary

Closed Mofei activity wishlist task. Simplified Dashboard to pure 9-grid layout (Customize moved to Settings); added 8x native 64-px icon assets; restricted TC font residency to TC_8/TC_10 with hot-swap chokepoint and PSRAM whitelist guard; fixed P0 startup CJK font (TC_10 baseline, EXTRA_SMALL hot-swaps to TC_8); added MOFEI_TOUCH_DEBUG=1 build env with TOUCHDBG serial traces and decision-table runbook. Codified two new spec contracts: PSRAM-whitelist TC font policy (backend) and touch coordinate stage ownership (frontend). All quality gates green; flashed to /dev/cu.usbmodem1101 and verified TC_10 boot serial (UIFONT id 1242001001).

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `08307f4` | (see git log) |
| `b019261` | (see git log) |
| `1f82a59` | (see git log) |
| `faca88d` | (see git log) |
| `77df5cf` | (see git log) |
| `270e2b5` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete


## Session 2: P0 touch coord debug — no defect found, screenshot tool fixed

**Date**: 2026-05-04
**Task**: P0 touch coord debug — no defect found, screenshot tool fixed
**Package**: open-x4-sdk
**Branch**: `feat/murphy`

### Summary

P0 touch-coordinate misalignment task closed without code fix. Captured 4 dashboard taps via mofei_touch_debug build; all Stage 1-5 pipeline behaviors (raw frame -> HAL conv -> orient transform -> hit-test -> hint fallback) match expectations. Visual cell border == hit-test rect (both come from cellRects[idx]). Confirmed user's earlier 'touch felt inaccurate' perception was the TC_8 small-font issue from before commit 08307f4. Deliverables: scripts/capture_touch_debug.py (reusable log harvester) and scripts/take_screenshot.py rotate-90-CW fix (panel native is 800x480 landscape, firmware drives logical 480x800 portrait via GfxRenderer rotation, the screenshot tool was dumping raw framebuffer without applying the rotation back). Archived task + research artifacts (capture log, analysis, BMP/PNG screenshots, panel spec).

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `8e45639` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete
