# Reader Advanced Reading Interface Wishlist

Date: 2026-05-05
Executor: Codex

## Goal

Implement the remaining Reader-specific wishlist items in dependency order, starting with Simplified-to-Traditional conversion in the current EPUB Reader. Keep non-Reader work out of this task.

## What I Already Know

- The current Reader paginates EPUB chapters through `Section::createSectionFile`, `ChapterHtmlSlimParser`, `ParsedText`, `TextBlock`, and serialized `Page` records.
- Current-chapter search, QR text export, footnote labels, and rendering all read text from serialized `Page` / `TextBlock` data.
- Existing Reader wishlist work added page-level line marks, footnote target confirmation, clearer current-chapter search labels, and a future-plan document.
- Firmware flash usage is high, so large runtime conversion or dictionary libraries must not be added blindly.

## Ordered Reader Plan

1. Reader Simplified-to-Traditional text conversion.
2. Reader vertical CJK layout mode.
3. Reader text-range highlight selection.
4. Reader footnote body popup.
5. Reader full-book search.
6. Reader in-reader dictionary expansion.

## Current Slice

Task 1 adds a Reader setting and text transform hook before EPUB text is paginated and cached.

## Requirements

- Add a persistent Reader setting for Simplified-to-Traditional conversion.
- Apply conversion before text enters `ParsedText` / `TextBlock` so render/search/QR/footnote labels share the same text.
- Include the setting in Section cache compatibility, so toggling it invalidates cached chapter pages.
- Use a compact phrase-first and character fallback table that is safe for firmware size.
- Document that this slice is not OpenCC-complete.

## Acceptance Criteria

- [ ] Reader settings include a Simplified-to-Traditional toggle with English, Simplified Chinese, and Traditional Chinese labels.
- [ ] EPUB body text runs through the conversion pipeline before pagination/cache serialization when enabled.
- [ ] Current-chapter search and QR text operate on converted page text because they read serialized pages.
- [ ] Section cache is rebuilt when the conversion setting changes.
- [ ] `git diff --check` passes.
- [ ] `pio run` passes.

## Definition of Done

- Tests or local verification commands executed.
- Behavior boundaries documented.
- Existing Reader features remain in scope and are not reverted.
- Non-Reader wishlist items remain out of scope.

## Out Of Scope

- Full OpenCC-compatible conversion coverage.
- Vertical layout, range highlights, footnote body extraction, full-book search, and in-reader dictionary implementation in this first slice.
- Mate/app/network/system changes.

## Technical Notes

- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp` is the text ingestion point.
- `lib/Epub/Epub/Section.cpp` stores cache compatibility parameters.
- `src/SettingsList.h` drives both device settings and web settings persistence.
- `scripts/gen_i18n.py` regenerates `lib/I18n/I18nKeys.h` and `lib/I18n/I18nStrings.cpp`.
