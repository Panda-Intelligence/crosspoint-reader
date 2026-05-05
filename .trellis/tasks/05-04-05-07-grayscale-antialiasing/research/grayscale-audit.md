# Grayscale antialiasing audit

Date: 2026-05-05
Owner: Codex

## Executive summary

Reader paths (EPUB / XTC / TXT readers, Sleep cover mode) **already
ship 4-level grayscale antialiasing**. The TC EPF font packs are
already 2-bit (`is2Bit=1`). The `GfxRenderer` 2-bit blit branch is
used. The reader does a 3-pass render (BW → GRAYSCALE_LSB →
GRAYSCALE_MSB → display gray buffer overlay).

Non-reader activities (Dashboard, all Hub/list activities, Settings,
Keyboard) render single-pass BW only. They have **never been on the
grayscale path**. Their CJK text is hard-edged 1-bit and their icons
are 1-bit only.

## Detailed audit

### Already grayscale (3 activities)

| Activity | File | Path |
|---|---|---|
| EpubReaderActivity | `src/activities/reader/EpubReaderActivity.cpp:1101-1124` | 3-pass when `SETTINGS.textAntiAliasing == 1` (default 1) |
| XtcReaderActivity | via `src/activities/reader/ReaderUtils.h:79` | Same pattern |
| SleepActivity | `src/activities/boot_sleep/SleepActivity.cpp:252` | Cover-image mode only |

The Reader 3-pass:

```cpp
renderer.storeBwBuffer();
if (SETTINGS.textAntiAliasing) {
  renderer.clearScreen(0x00);
  renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
  page->render(...);
  renderer.copyGrayscaleLsbBuffers();

  renderer.clearScreen(0x00);
  renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
  page->render(...);
  renderer.copyGrayscaleMsbBuffers();

  renderer.displayGrayBuffer();
  renderer.setRenderMode(GfxRenderer::BW);
  renderer.restoreBwBuffer();
}
```

### Single-pass BW only (~50 activities)

All `setRenderMode` and `displayGrayBuffer` callsites are confined to
the 3 reader activities above. Every other activity uses the default
`renderer.displayBuffer()` (BW framebuffer).

Sample (incomplete list):
- `DashboardActivity` — 1-pass BW
- All Settings activities — 1-pass BW
- All Hub activities (Reading / Study / Desktop / Arcade) — 1-pass BW
- Keyboard / Confirmation / Crash / FullScreenMessage — 1-pass BW
- All list activities (FileBrowser / Saved Cards / etc.) — 1-pass BW

## Visual evidence

`research/current-state.bmp` is a 800×480 framebuffer dump captured
while on Dashboard. Histogram analysis:

```
mode = '1' (1-bit)
distinct values = 2 (only 0 and 255)
  0   pixels: 24,856
  255 pixels: 359,144
```

Confirms Dashboard is pure 1-bit. CJK labels look hard-edged.

(For a Reader-page screenshot to show 4 levels, the screenshot tool
would need to dump the gray-overlay buffers in addition to the BW
framebuffer. The current `CMD:SCREENSHOT` only dumps the BW buffer.
This is a separate tooling gap, not a render gap.)

## Decision

The originally-scoped task ("implement 2-bit grayscale font + image
pipeline") is **already done**. Closing this task without code change.

### Follow-up candidates (not blocking)

1. **Extend grayscale to non-reader UI** — Dashboard icons + CJK labels
   currently render 1-bit; could benefit from grayscale antialiasing.
   Trade-offs:
   - Pro: visually smoother icons, less pixely CJK text
   - Con: every activity becomes 3 render passes (slower full refresh,
     ~3.7 s vs 1.5 s today on Mofei panel)
   - Con: icon assets are themselves 1-bit; grayscale render of a 1-bit
     bitmap doesn't add levels
   - Verdict: not worth the e-ink refresh cost unless icons are
     regenerated as 2-bit too.

2. **Screenshot tool grayscale support** — `CMD:SCREENSHOT` only dumps
   the BW framebuffer. To capture a real Reader screenshot showing
   grayscale, the screenshot serial protocol would need to dump the
   gray-LSB and gray-MSB buffers too, and the host side would need to
   compose them. Useful for QA evidence but not blocking.

3. **Audit `SETTINGS.textAntiAliasing` default** — currently 1 (on).
   No need to change.

### Recommendation

Close this task as "no defect; already shipping". File the two
follow-up candidates as separate planning tasks if the user wants
them later.
