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

- (Preference) Phasing strategy:
  1. Font first, then image (each gets its own commit + flash test).
  2. Image first (smaller scope, easier to verify visually).
  3. Both together (single large commit, harder to bisect).
- (Preference) PSRAM trade-off if 2-bit fonts exceed budget:
  1. Drop TC_10 from resident set; only TC_8 (smaller pack) loaded; UI
     gets smaller as a side effect.
  2. Keep both 1-bit fonts available as a runtime "low-memory mode"
     toggle.
  3. Accept tighter PSRAM headroom; rely on FontDecompressor's failure
     tracking to skip hot groups when OOM.
- (Blocking) Is there hardware time available for full A/B visual
  comparison on a real Mofei device? Without it, we can't tell whether
  the 2-bit output is meaningfully better than the current dithering.

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
