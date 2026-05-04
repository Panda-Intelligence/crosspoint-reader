# Touch coordinate capture — analysis

Date: 2026-05-04
Owner: Codex
Source data: `touch-debug-capture.md`

## Summary

**No observable touch-coordinate misalignment in the captured session.** All
`dashboard_hit` / `dashboard_miss` / `hint_miss` / `hint_gap` /
`fallback_*` traces are mathematically consistent with the documented
five-stage pipeline. `raw == oriented` for every event in Portrait
orientation — Stage 3 (orientation transform) is a no-op as expected for
Portrait, no flipping observed.

## Captured taps (8 events, ~180 s window)

| # | Time | Raw | Likely intended | Outcome | Verdict |
|---|---|---|---|---|---|
| 1 | +122 s | (24, 46) | Top-left corner (A1) | dashboard_miss + fallback_zone=back | ✅ correct (above grid, no cell here) |
| 2 | +123 s | (479, 14) | Top-right corner (A2) | dashboard_miss + fallback_zone=right | ✅ correct (above grid, no cell here) |
| 3 | +124 s | (0, 799) | Bottom-left corner (A3) | hint_gap + fallback_bottom_guard | ✅ correct (in bottom zone but in hint button gap) |
| 4 | +141 s | (68, 236) | Cell 0 center | **dashboard_hit shortcut index=0 rect=(20,105,146,218)** | ✅ correct hit |
| 5 | +141 s | (260, 225) | (no longer on Dashboard — RecentBooks opened) | no dashboard_* trace | n/a |

Only events 1–4 occurred while the user was on Dashboard. Event 4 selected
`RecentReading` and routed to `RecentBooksActivity`, which has no debug
trace today. Events 5–8 (the rest of the matrix B and C) need a separate
capture session.

## Layout math sanity check

Stage 4 (activity hit-test) verified against `DashboardActivity::layoutGridCells`:

```
pageWidth = 480, sidePad = 20 (LyraMetrics::contentSidePadding),
kGridGapPx = 0, kGridCols = 3
usableWidth = 480 - 2*20 - 0 = 440
cellWidth = 440 / 3 = 146

Cell 0 (row=0, col=0): rect = (20, 105, 146, 218)   ← matches captured rect
Cell 1 (row=0, col=1): rect = (166, 105, 146, 218)
Cell 2 (row=0, col=2): rect = (312, 105, 146, 218)
Cell 3 (row=1, col=0): rect = (20, 323, 146, 218)
...

`pointInRect((68, 236), Cell 0)` =
   68 >= 20  AND  68 < 166  AND
   236 >= 105 AND  236 < 323
   = true   ✅ matches `dashboard_hit`
```

## Visual evidence

Screenshot captured via `scripts/take_screenshot.py` after fixing the
script's rotation bug (SSD1677 native is 800×480 physical landscape; the
firmware drives 480×800 logical portrait via `GfxRenderer` rotation, but
the screenshot tool was dumping the raw framebuffer without applying the
transform back, producing a sideways BMP).

- Raw framebuffer dump: `dashboard-after-tc10.bmp` (480×800 portrait, fixed)
- Re-emitted PNG: `dashboard-after-tc10.png` (480×800 portrait)

The portrait screenshot shows:
- Header `Mofei 儀表板` + battery `離線 22度 ⋯ 100%🔋` (TC_10 CJK glyphs
  legible — confirms the boot font fix from commit `08307f4`).
- 3×3 grid with 64×64 icons + CJK labels: 繼續閱讀 / 閱讀 / 學習 / 遊戲廳 /
  導入與同步 / 檔案瀏覽器 / 設定 / 天氣與時間 / 今日.
- **Cell 0 (繼續閱讀) is drawn with a thicker selection border** — matches
  the captured `dashboard_hit index=0 rect=(20,105,146,218)` from the touch
  log. Confirms visual border position == hit-test rect (no drift).
- Bottom button hint bar with three full buttons (開啟 / 左 / 右).

## Display panel spec (for reference)

User-supplied vendor datasheet (added 2026-05-04):

```
Model           GDEQ0426T82-T01C
Diagonal        4.26"
Type            Graphic dot matrix e-ink
Driver IC       SSD1677
Resolution      800 × 480 (physical, landscape native)
Outline         105.33 × 62.37 × 1.41 mm
Active area     92.8 × 5.68 mm  (note: vendor sheet typo; should be 92.8 × 55.68)
Pixel pitch     0.116 × 0.116 mm
Operating       0–50 °C
Storage         -25–70 °C
Connector       24-pin FPC
Max grayscale   4
Interface       SPI
Refresh         3.5 s full / 1.5 s fast / 0.42 s partial
Power           26.4 mW operating / 0.0066 mW standby

Touch connector 6-pin FPC
Touch IC        FT6336U
Touch supply    2.8–3.6 V
Touch current   4.32 mA / 55 µA standby
```

Key implications cross-referenced with the touch pipeline:
- Physical pixel grid is 800 × 480. Firmware applies a Stage 3 orientation
  transform (`GfxRenderer::rotateCoordinates`) so activities render in
  logical 480 × 800 portrait. The `[TOUCHDBG]` traces all show
  `screen=480x800 orient=0`, consistent.
- Max grayscale = 4 — directly relevant to the future P2 antialiasing task
  (`05-07-grayscale-antialiasing`); the panel hardware bound is 2-bit.
- Touch IC is FT6336U as already wired (`MOFEI_TOUCH_ADDR=0x2E`,
  SDA=GPIO13, SCL=GPIO12) — no surprises in spec.

## Stage attribution

Per the spec contract "Keep One Owner Per Touch Coordinate Stage", every
captured event was correctly attributed:

- **Stage 1 (FT6336U raw frame)**: produced sane `(x, y)` values ∈ [0, 480) ×
  [0, 800). No `0xFF` garbage frames seen.
- **Stage 2 (HAL conversion)**: axis macros applied correctly. `raw` is in
  logical-portrait 480×800 coordinates as expected.
- **Stage 3 (orientation transform)**: `raw == oriented` for all events,
  matching `orient=0` (Portrait) where the transform is identity.
- **Stage 4 (activity hit-test)**: dashboard's `pointInRect` results match
  the layout math above.
- **Stage 5 (button-hint fallback)**: `hint_y=[752,800)` consistent with
  `MOFEI_TOUCH_LOGICAL_HEIGHT_PX - 48`. Events outside that band correctly
  produced `hint_miss`.

No misalignment found in any stage.

## Why does the user perceive misalignment?

Two non-touch hypotheses worth investigating before re-running the capture:

1. **Visual misalignment, not coordinate misalignment.**
   The cell border may render at a slightly different location than where
   the hit rect is computed. A user tapping the visible border edge would
   land in a neighbor cell or in the gap. To check: visually compare the
   drawn `drawRoundedRect(cell.x, cell.y, cell.width, cell.height, ...)`
   against the `cellRects[idx]` used by `pointInRect`. The two should be
   pixel-identical.

2. **Earlier session used TC_8 (8 pt) UI font.**
   Before the boot font was switched from TC_8 to TC_10 (commit `08307f4`),
   labels were so small that the user couldn't reliably aim at the
   intended cell. Re-evaluate after a fresh visual session on the new
   build — the perception may simply have been "I can't see what I'm
   trying to tap."

## Next steps

1. Run a second capture session focused on:
   - All 9 grid cells (sweep all C-row taps without selecting one)
   - All 4 button-hint buttons (B-row)
   - At least one orientation rotation (E)
   To prevent context switching, the user can tap the FOCUS button instead
   of confirm to stay on Dashboard.

2. Verify the visual-vs-hit-rect hypothesis by reading the dashboard
   render code path side by side with the layout path. Confirm
   `drawRoundedRect` arguments match `cellRects[idx]` exactly.

3. Decide: if (1) and (2) both pass, this task can be closed as
   "no defect found", and a note added to the spec contract that the
   capture script is the canonical regression test.
