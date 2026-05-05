# Grayscale antialiasing for reader and images

## Goal

Render reader text and images using 2-bit grayscale (4 levels) instead of
the current 1-bit dithered output, so CJK text and book illustrations look
sharper on the Mofei e-ink panel.

## What I already know

- Mofei panel (GDEQ0426T82-T01C) supports 4-grayscale mode; `MofeiDisplay`
  already implements the LUT.
- `EpdFontPackHeader.is2Bit` field exists; `fontconvert.py` supports 2-bit
  output via `--2bit`.
- `GfxRenderer::renderCharImpl` has both 1-bit and 2-bit branches.
- `JpegToFramebufferConverter` and `PngToFramebufferConverter` decode to
  grayscale internally, then quantize.
- PSRAM budget today (post `05-01` task): TC_8 ≈ 1.48 MB, TC_10 ≈ 1.87 MB
  (whitelist).

## Auto-discovery (2026-05-05)

While auditing the codebase before doing any work, I found grayscale
antialiasing is **already shipping**:

1. **TC EPF packs already 2-bit**:
   ```
   $ python3 -c 'parse FontPackHeader of each tc_*.epf'
     TC_8:  is2Bit=1, bitmapSize=1069981, ascender=20, advanceY=24
     TC_10: is2Bit=1, bitmapSize=1466793, ascender=25, advanceY=30
     TC_12: is2Bit=1, bitmapSize=1938680, ascender=29, advanceY=36
     TC_14: is2Bit=1, bitmapSize=2227485, ascender=34, advanceY=42
   ```
   `scripts/build_multilingual_font_pack.py:84` already calls
   `fontconvert.py --2bit --compress`.

2. **Reader already does the 3-pass grayscale render** in
   `src/activities/reader/EpubReaderActivity.cpp:1101-1124` when
   `SETTINGS.textAntiAliasing == 1` (default 1):
   ```
   render BW pass        → renderer.displayBuffer()
   render GRAYSCALE_LSB  → copyGrayscaleLsbBuffers()
   render GRAYSCALE_MSB  → copyGrayscaleMsbBuffers()
   displayGrayBuffer()   → 4-level overlay
   ```

3. **Image rendering already 2-bit-aware** in
   `lib/GfxRenderer/GfxRenderer.cpp:738-744`. `drawBitmap` reads the
   2-bit pixel value and dispatches `BW / GRAYSCALE_MSB / GRAYSCALE_LSB`
   based on `renderMode`. EPUB image cache files use `Bitmap` so they
   participate in the same 3-pass render.

So the task as originally framed ("regenerate 2-bit packs + extend image
pipeline") is **already done**. What remains is verification, and
identifying any UI surfaces that DON'T benefit (Dashboard, Sleep,
Settings — these are mostly 1-pass BW renders today).

## Decision (ADR-lite — Q1 phasing, REVISED)

**Context**: Originally scoped as "implement 2-bit grayscale". After
auto-discovery, the implementation is already shipping for
reader text + reader images. The new scope is **verification + visual
A/B + extending grayscale to UI surfaces that haven't been touched**.

**Decision**: Revised MVP — drop the implementation phase entirely, focus
on **visual verification** and **scope what (if anything) still needs
grayscale that doesn't have it**.

**Consequences**:
- No font regeneration commit needed.
- No image converter commit needed.
- Add a `research/grayscale-audit.md` documenting what's already
  grayscale-rendered and what isn't.
- Take before-vs-after-irrelevant screenshots that show CJK in reader
  using grayscale (already shipping).
- If non-reader surfaces (Dashboard / Settings / Sleep) would benefit,
  spin a follow-up task. Today they use 1-pass BW which is fine for
  short labels but suboptimal for icons.

## Implementation Plan (revised)

### Step 1 — Visual A/B verification
1. Boot, take screenshot of an EPUB reader page with CJK text.
2. Inspect for 4-level grayscale (anti-aliased edges) vs 1-bit dither.
3. If grayscale is clearly visible, the task is verified-as-shipped.

### Step 2 — UI grayscale audit
Scan all activities for `setRenderMode(GRAYSCALE_*)` callsites. Anything
that doesn't switch render mode is implicitly BW. Document the list:

```
Already grayscale: EpubReaderActivity, ReaderUtils, SleepActivity (cover mode)
Still BW:          DashboardActivity, SettingsActivity, all hub activities,
                   keypad, list activities
```

### Step 3 — Decide follow-up
Based on Step 2's audit, decide if any non-reader surface visibly
suffers from BW-only rendering (e.g. dashboard icon edges look jaggy).
If yes, file a separate task `05-08-ui-grayscale-extension`. If no,
close this task as "already shipping; visually verified".

## Assumptions (temporary)

- 2-bit is the target (4 levels: white, light gray, dark gray, black).
  Not 4-bit / 16-level — Mofei panel doesn't support that.
- Phase 1 = font (highest user impact); Phase 2 = images.
- It is acceptable to drop one of TC_8 or TC_10 from the resident
  whitelist if PSRAM budget no longer fits both at 2-bit.

## Open Questions

- ~~(Preference) Phasing strategy~~ — **Resolved (Q1)**: option 2 — font
  + image both in this task. Single MVP, ship together.
- ~~(Preference) PSRAM trade-off~~ — **Resolved**: keep both TC_8 and
  TC_10 if 2-bit pack sizes fit; if either exceeds the 3.3 MB hot-group
  contiguous block requirement, drop TC_10 first (TC_8 is the smaller
  pack and is already the EXTRA_SMALL reader font). Spec contract updated
  if whitelist changes.
- ~~(Blocking) Hardware time for A/B comparison~~ — **Resolved**: device
  is at `/dev/cu.usbmodem1101`; flash + screenshot tooling already
  proven (`scripts/take_screenshot.py`, `scripts/capture_touch_debug.py`).

## Decision (ADR-lite — Q1 phasing)

**Context**: 2-bit grayscale spans two pipelines (font + image). Each is
non-trivial individually, but they share verification overhead (one flash
cycle covers both).

**Decision**: Combine font and image into a single task. Two commits:
1. `feat(font): 2-bit grayscale TC packs + GfxRenderer 2-bit blit`
2. `feat(image): 4-grayscale JPEG/PNG framebuffer converters`

This keeps each commit individually revertable while sharing the visual
verification session.

**Consequences**:
- One PSRAM rebudget pass.
- One spec contract update (if TC_10 drops out of whitelist).
- One task close-out, two journal commits if needed.

## Implementation Plan (small commits)

### Commit 1 — Font 2-bit
1. Run `fontconvert.py --2bit` on the 4 input TC fonts at sizes 8 and 10
   (the 2 packs in the active whitelist). Output to `.mofei-fontpacks/`
   with same filenames; the SD-side path stays `/.mofei/fonts/...`.
2. Verify `GfxRenderer::renderCharImpl` 2-bit branch — already in code,
   gated by `fontData->is2Bit`.
3. Measure runtime PSRAM impact via `[TCFONT_MEM]` and `[MEM]` logs.
4. If TC_10 won't fit, drop it from `isAllowedTraditionalChineseFontId`
   whitelist; reader medium+ sizes fall back to TC_8 (8 pt CJK is
   readable in reader context where line height matters less than
   horizontal density).
5. Update spec `.trellis/spec/open-x4-sdk/backend/quality-guidelines.md`
   "Restrict Mofei TC Font Hot-Swap" if whitelist changes.

### Commit 2 — Image 2-bit
1. `JpegToFramebufferConverter` and `PngToFramebufferConverter` already
   decode to 8-bit grayscale internally; today they quantize to 1-bit
   via dither. Switch quantization to 4-level (cuts at 64 / 128 / 192).
2. Display layer must accept the 4-level pixel writes. Confirm
   `MofeiDisplay::writePixel` or equivalent supports 2-bit values.
3. Verify with an EPUB containing inline images (any sample EPUB will
   do).

## Requirements (evolving)

- 2-bit TC EPF packs (TC_8 + TC_10 at minimum) regenerated via
  `fontconvert.py` and committed to `.mofei-fontpacks/` /
  `/.mofei/fonts/`.
- `GfxRenderer` 2-bit blit path verified against the new packs.
- Image pipeline: `JpegToFramebufferConverter` and
  `PngToFramebufferConverter` extended to write 4-grayscale output to the
  framebuffer.
- PSRAM whitelist re-evaluated. If a pack drops out, update the spec
  contract in `backend/quality-guidelines.md`.
- Boot serial verification: TC pack load logs show `is2Bit=true`.

## Acceptance Criteria (evolving)

- [ ] 2-bit TC EPF packs regenerated and committed
- [ ] Reader CJK text renders without visible 1-bit dither pattern
- [ ] Inline EPUB image at default size renders with 4 levels (visual
      check)
- [ ] PSRAM headroom at runtime ≥ 2 MB free (for FontDecompressor hot
      group)
- [ ] No regression on FT6336U touch / boot / SD mount paths
- [ ] Spec updated if whitelist changes

## Definition of Done

- `pio run -e mofei` SUCCESS
- `pio check -e mofei --fail-on-defect high` SUCCESS
- Mofei device flash + manual side-by-side visual diff (before/after
  photo) recorded in research/
- Spec change committed if needed

## Out of Scope (explicit)

- 4-bit or 16-level grayscale (panel doesn't support it)
- Color rendering
- TTF runtime rasterization (separate task `05-08`)
- Custom dithering algorithms; use whatever the existing 4-grayscale
  blit produces

## Technical Notes

- Roadmap classified this XL complexity (~1 week of work, mostly
  visual-verification driven).
- Owner files:
  - `lib/EpdFont/scripts/fontconvert.py` for pack regeneration
  - `lib/GfxRenderer/GfxRenderer.cpp` for 2-bit blit verification
  - `lib/GfxRenderer/JpegToFramebufferConverter.cpp` and
    `lib/GfxRenderer/PngToFramebufferConverter.cpp` for image path
  - `src/StorageFontRegistry.cpp` if whitelist size changes
- Risk: this conflicts with the just-codified PSRAM whitelist contract.
  Any change to the whitelist must update
  `.trellis/spec/open-x4-sdk/backend/quality-guidelines.md` "Restrict
  Mofei TC Font Hot-Swap" section.

## Research References

- (none yet — may want a small `research/grayscale-budget.md` that
  measures actual PSRAM usage of regenerated 2-bit packs before
  committing the whitelist change)
