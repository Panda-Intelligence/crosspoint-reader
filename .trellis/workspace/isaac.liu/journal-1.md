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


## Session 3: P1 dashboard icons — fix 5 misaligned shortcut icons (Bootstrap Icons)

**Date**: 2026-05-04
**Task**: P1 dashboard icons — fix 5 misaligned shortcut icons (Bootstrap Icons)
**Package**: open-x4-sdk
**Branch**: `feat/murphy`

### Summary

P1 dashboard icon semantics task closed. 5 shortcut icons remapped to new Bootstrap Icons SVGs: WeatherClock→cloud-sun (was Recent/clock), Today→calendar3 (was Recent dup), RecentReading→bookmark (was Recent triple-dup), ArcadeHub→controller (was Hotspot wifi-style), DesktopHub→grid-3x3-gap (was Folder dup). New scripts/scratch/build_dashboard_icons.py renders SVG via cairosvg, applies 90° CCW rotation so byte layout matches panel's physical 800x480 landscape orientation (drawIcon writes row-major in panel orient; firmware applies +90° CW to display). 10 generated 1-bit byte-array .h files (5 icons × 32+64 px). Visual verification via screenshot after flash; all 9 active shortcuts show distinct, semantically appropriate icons. Build clean, pio check no high defects.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `e71e3bd` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete


## Session 4: P1 lockscreen passcode — 4-digit numeric, configurable timeout

**Date**: 2026-05-05
**Task**: P1 lockscreen passcode — 4-digit numeric, configurable timeout
**Package**: open-x4-sdk
**Branch**: `feat/murphy`

### Summary

P1 lockscreen passcode task closed (implemented in parallel by codex runtime per the PRD I drove via brainstorm). Locked decisions: 4-digit numeric, SHA-256+16B-salt hash via mbedtls, 3-attempts-then-30s lockout, configurable timeout enum (EveryWake/1m/5m/15m/1h/Disabled, default 5min), opt-in Settings flow with Set/Change/Disable + auto-lock-after enum. Files added: src/activities/lockscreen/{LockScreenActivity,PasscodeEnrollActivity}.{cpp,h}, src/components/{PasscodeKeypad,PasscodeHash}.{cpp,h}, 6 new SETTINGS fields (lockScreenEnabled/Salt/Hash/TimeoutMinutes/WrongCount/LockoutUntilEpoch). Build clean. Task archived.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `05f9f8c` | (see git log) |
| `41ba426` | (see git log) |
| `6ddff38` | (see git log) |
| `243ee6a` | (see git log) |
| `6614a61` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete


## Session 5: P2 grayscale antialiasing — audit confirms already shipping

**Date**: 2026-05-05
**Task**: P2 grayscale antialiasing — audit confirms already shipping
**Package**: open-x4-sdk
**Branch**: `feat/murphy`

### Summary

P2 grayscale antialiasing task closed without code change. Audit found the originally-scoped feature is already in production: TC EPF font packs are 2-bit (is2Bit=1, verified by parsing FontPackHeader of every notosans_tc_*.epf), build_multilingual_font_pack.py invokes fontconvert.py with --2bit --compress, GfxRenderer::renderCharImpl 2-bit branch is wired and dispatched by renderMode (BW / GRAYSCALE_LSB / GRAYSCALE_MSB), EpubReaderActivity::render does the 3-pass (BW -> gray-LSB -> gray-MSB -> displayGrayBuffer) when SETTINGS.textAntiAliasing == 1 (default 1), drawBitmap reads 2-bit pixel value and dispatches per renderMode. Same pattern in XtcReaderActivity via ReaderUtils.h. Non-reader activities (Dashboard, Settings, all hubs, keyboard) use single-pass BW by design; extending grayscale would triple full-refresh time (3.5 s vs 1.5 s on Mofei panel) for visual gain that is lost on 1-bit icon assets. Audit + screenshot evidence in research/grayscale-audit.md and current-state.bmp/png. Two follow-up candidates documented (extend grayscale to UI / improve screenshot tool to dump gray buffers) but neither is blocking.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `5cb2a48` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete
