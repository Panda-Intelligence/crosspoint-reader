# Reader Advanced Wishlist Code Context

Date: 2026-05-05
Executor: Codex

## EPUB Text Pipeline

- `EpubReaderActivity::render` loads or creates a `Section` for the current spine.
- `Section::createSectionFile` streams the spine XHTML into a temp file, then runs `ChapterHtmlSlimParser`.
- `ChapterHtmlSlimParser::characterData` receives visible text from Expat and emits words through `flushPartWordBuffer`.
- `ParsedText::layoutAndExtractLines` converts words into `TextBlock` lines.
- `Page::serialize` stores those `TextBlock` lines in the section cache.

## Current Consumers

- Rendering calls `Page::render`.
- Search uses `pageTextFromPage` over `PageLine` / `TextBlock`.
- QR export manually walks the same `PageLine` / `TextBlock` words.
- Footnote link labels are collected in `ChapterHtmlSlimParser::characterData`.

## Implementation Implication

The conversion must happen before `ParsedText::addWord`. A late render-only conversion would leave search, QR export, and footnote labels inconsistent.

## Cache Implication

`Section` cache headers already include font/layout/image settings. The conversion flag must be added to the header and comparison path so toggling it causes a rebuild.
