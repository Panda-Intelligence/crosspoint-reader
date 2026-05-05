# Grayscale antialiasing for reader and images

## Goal

Render reader text and images using 2-bit grayscale (4 levels) instead of
the current 1-bit dithered output, so CJK text and book illustrations look
sharper on the Mofei e-ink panel.

## What I already know

- Mofei panel (GDEQ0426T82-T01C) supports 4-grayscale mode; `MofeiDisplay`
  already implements the LUT.
- `EpdFontPackHeader.is2Bit` field exists; `fontconvert.py` supports 2-bit
  output. Current TC EPF packs are 1-bit (`is2Bit=false`).
- `GfxRenderer::renderCharImpl` has both 1-bit and 2-bit branches; the
  2-bit branch is in place but unused for production fonts today.
- `JpegToFramebufferConverter` and `PngToFramebufferConverter` decode to
  grayscale internally, then quantize to 1-bit dither for the framebuffer.
- PSRAM budget today (post `05-01` task): TC_8 ≈ 1.48 MB, TC_10 ≈ 1.87 MB
  (whitelist). 2-bit doubles bitmap data; expect TC_8 ≈ 2.5 MB, TC_10 ≈
  3.2 MB. Hot-group decompression for builtin Noto CJK still needs ~3.3 MB
  contiguous.
- Spec contract (just codified): `isAllowedTraditionalChineseFontId`
  whitelist must be re-evaluated if pack sizes change.

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
