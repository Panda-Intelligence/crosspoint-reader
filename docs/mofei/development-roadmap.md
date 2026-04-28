# CrossPoint Reader — Mofei Development Roadmap

> Hardware: ESP32-S3 (dual-core 240MHz, 8MB PSRAM, 16MB Flash)
> Display: GDEQ0426T82-T01C (800×480, 4-grayscale e-ink, resistive touch)

## Overview

Based on the wishlist, features are grouped into **5 phases** ordered by user value, technical dependency, and risk.
Each feature includes: scope summary, key files to modify, complexity estimate (S/M/L/XL), and RAM impact.

---

## Phase 1 — Core Reading Quality (Highest Impact, 2–3 weeks)

These directly improve the primary use case: reading.

### 1.1 EPUB 内页图片显示
- **What**: Render `<img>` in EPUB sections as 1-bit or grayscale bitmaps within page layout.
- **Status**: `lib/Epub/Epub/blocks/ImageBlock.cpp` + `converters/` already exist. Need to verify end-to-end rendering in `EpubReaderActivity`.
- **Files**: `ImageBlock.cpp`, `PngToFramebufferConverter.cpp`, `JpegToFramebufferConverter.cpp`, `Section.cpp`, `EpubReaderActivity.cpp`
- **Complexity**: M (pipeline exists, may need RAM tuning with PSRAM)
- **RAM**: Moderate — image decode buffers use PSRAM; no DRAM concern on S3.

### 1.2 EPUB 封面显示（书单/首页）
- **What**: Extract cover image from EPUB metadata and display in HomeActivity grid + RecentBooks.
- **Status**: `HomeActivity` already renders cover bitmaps for recent books. Need to extract cover from EPUB `<metadata>` / manifest.
- **Files**: `HomeActivity.cpp`, `RecentBooksStore.cpp`, `lib/Epub/Epub.cpp` (add `getCoverImage()`)
- **Complexity**: S–M (EPUB cover extraction is well-defined)
- **RAM**: Low — cover thumbnails cached on SD.

### 1.3 简转繁
- **What**: Real-time Simplified→Traditional Chinese conversion during text rendering.
- **Status**: `TraditionalChineseFontsActivity.cpp` exists (font side). Need text conversion layer.
- **Approach**: Build a static `constexpr` lookup table (~7000 entries, stored in Flash). Convert at render time via `string_view` transform.
- **Files**: New `lib/I18n/S2TConverter.h/.cpp`, integrate into `Section.cpp` text pipeline
- **Complexity**: S (lookup table + integration point)
- **RAM**: Zero runtime cost — table in Flash, convert per-char at render.

### 1.4 显示页码
- **What**: Show "Page X / Y" in the reader footer bar.
- **Status**: `EpubReaderActivity` already tracks `currentPage` and `totalPages` via section cache. Need UI rendering.
- **Files**: `EpubReaderActivity.cpp` (render footer), `EpubReaderMenuActivity.h`
- **Complexity**: S (data exists, just render)
- **RAM**: Negligible.

### 1.5 一键关闭触控（防手触）
- **What**: Toggle to disable touch input, keeping only button navigation.
- **Files**: `EpubReaderMenuActivity.h` (add `TOUCH_LOCK` MenuAction), `MappedInputManager.cpp`, `CrossPointSettings.cpp` (add `touchLocked` bool)
- **Complexity**: S
- **RAM**: 1 byte in settings.

### 1.6 字体直接调整大小（阅读中）
- **What**: Allow font size change without leaving the reader, then re-render current section.
- **Files**: `EpubReaderMenuActivity.h` (add `FONT_SIZE` MenuAction), `EpubReaderActivity.cpp` (invalidate cache + re-parse)
- **Complexity**: M (cache invalidation + re-layout needed)
- **RAM**: No extra — reuses existing cache path.

---

## Phase 2 — Navigation & Interaction (2–3 weeks)

### 2.1 註解超連結（点击跳出註解框）
- **What**: Tap footnote links in EPUB text to open inline footnote popup.
- **Status**: `EpubReaderFootnotesActivity.cpp` already collects footnotes and shows a list. Need touch-on-link detection + inline popup.
- **Files**: `EpubReaderActivity.cpp` (touch hit-test on link regions), `EpubReaderFootnotesActivity.cpp`
- **Complexity**: M (touch coordinate → link mapping + popup UI)
- **RAM**: Low — footnote data already collected.

### 2.2 搜尋功能
- **What**: Full-text search within current EPUB book.
- **Approach**: Load section text, run `strstr()` / simple UTF-8 search, highlight matches, navigate to result page.
- **Files**: New `EpubReaderSearchActivity.cpp`, `lib/Epub/Epub.cpp` (add `searchInSection()`)
- **Complexity**: M (need to scan cached text; PSRAM allows large buffers)
- **RAM**: PSRAM for search buffer; DRAM minimal.

### 2.3 書籤功能
- **What**: Save named bookmarks per book, list them in menu, navigate to them.
- **Files**: New `BookmarkStore.cpp`, `EpubReaderMenuActivity.h` (add `BOOKMARKS`), `EpubReaderActivity.cpp`
- **Storage**: SD card JSON file per book.
- **Complexity**: S–M (CRUD + storage + UI)
- **RAM**: Minimal.

### 2.4 畫線功能
- **What**: Select text range, mark as highlighted, persist per-book.
- **Approach**: Touch-select text (start/end positions), store as (section index, page, char range) tuples. Render as inverted highlight band.
- **Files**: New `HighlightStore.cpp`, `Section.cpp` (render highlights), `EpubReaderActivity.cpp` (touch selection mode)
- **Complexity**: L (touch selection UI + range mapping + persistence)
- **RAM**: Low per-highlight, stored on SD.

### 2.5 九宫格自訂義 + 最近阅读
- **What**: Customizable home grid with pinned books + recent reads.
- **Status**: `HomeActivity` already has a grid. Need drag-reorder + pin + recent reads integration.
- **Files**: `HomeActivity.cpp`, `DashboardActivity.cpp`, new `HomeGridStore.cpp`
- **Complexity**: M (grid customization UI on e-ink is tricky)
- **RAM**: Low — grid config on SD.

---

## Phase 3 — Typography & Rendering (2–3 weeks)

### 3.1 閱讀字體 2-bit 灰階抗鋸齒
- **What**: Replace 1-bit font rendering with 2-bit grayscale anti-aliasing for CJK text.
- **Status**: `GfxRenderer` and `EpdFont` have some grayscale infrastructure. `MofeiDisplay` supports 4-grayscale mode.
- **Approach**: Extend font glyph format to 2-bit per pixel. Use the BW+RED channel trick (2-bit: 00=white, 01=light, 10=dark, 11=black).
- **Files**: `lib/EpdFont/` (glyph decoder), `GfxRenderer.cpp` (2-bit blit), `MofeiDisplay.cpp` (grayscale write)
- **Complexity**: L (font format change + renderer rewrite + significant Flash increase)
- **RAM**: Double framebuffer bandwidth (BW + RED planes). PSRAM absorbs this.

### 3.2 直排/橫排切换
- **What**: Support vertical writing mode (traditional Chinese/Japanese style) with toggle.
- **Approach**: Rotate CJK characters 90° during render, lay out columns right-to-left. Toggle via settings.
- **Files**: `Section.cpp` (layout engine: column-based instead of line-based), `GfxRenderer.cpp` (rotated glyph draw)
- **Complexity**: XL (fundamental layout engine change — new pagination algorithm)
- **RAM**: Similar to horizontal mode.

### 3.3 英文排版優化
- **What**: Proper kerning, ligatures, hyphenation, and word spacing for English text.
- **Status**: `lib/Epub/Epub/hyphenation/LiangHyphenation.cpp` exists. Kerning data may need font-side support.
- **Files**: `Section.cpp` (line-breaking), `GfxRenderer.cpp` (kerning lookup)
- **Complexity**: M–L (hyphenation exists; kerning + ligature need font metrics)
- **RAM**: Kerning table in Flash, minimal DRAM.

### 3.4 内建英文字典
- **What**: Select a word in reader, show definition from built-in dictionary.
- **Status**: `DictionaryActivity.cpp` already exists.
- **Approach**: Bundle a compressed dictionary (WordNet or similar, ~2MB compressed) on SD. Lookup by selected word.
- **Files**: Extend `DictionaryActivity.cpp`, new `DictionaryStore.cpp`, SD dictionary file
- **Complexity**: M (UI exists; need dictionary data + lookup)
- **RAM**: PSRAM for decompression; Flash for index.

---

## Phase 4 — System & Lifestyle Features (2–3 weeks)

### 4.1 便利貼（待辦事項）
- **What**: Sticky notes / todo list accessible from desktop hub.
- **Files**: New `StickyNoteActivity.cpp`, `StickyNoteStore.cpp` (SD-backed)
- **Complexity**: M (CRUD + e-ink text entry UI)
- **RAM**: Low.

### 4.2 求籤/擲硬幣
- **What**: Fortune stick / coin toss mini-activity.
- **Files**: New `FortuneActivity.cpp` in `src/activities/arcade/`
- **Complexity**: S (random number + static text pool + animation)
- **RAM**: Minimal.

### 4.3 自建單字背單字
- **What**: User creates word cards from dictionary lookups, then reviews via SRS.
- **Status**: Full study system already exists (`StudyHub`, `ReviewQueue`, `StudyQuiz`). Need "add to study" from dictionary/reader.
- **Files**: `DictionaryActivity.cpp` (add "study this word" action), `StudyCardsTodayActivity.cpp`
- **Complexity**: M (integration with existing study system)
- **RAM**: Minimal — reuses study infrastructure.

### 4.4 設定同步（跨裝置）
- **What**: Sync settings and reading progress across devices (via WebDAV or KOReader sync).
- **Status**: `KOReaderSyncActivity.cpp` already exists for progress sync. Extend to include settings.
- **Files**: `KOReaderSyncActivity.cpp`, `CrossPointSettings.cpp`
- **Complexity**: M (settings serialization + conflict resolution)
- **RAM**: Low.

### 4.5 桌布自訂 + 旋轉180°
- **What**: Custom wallpaper from SD images, with 180° rotation option.
- **Files**: `SleepActivity.cpp` (sleep wallpaper), `SettingsActivity.cpp`, new wallpaper setting
- **Complexity**: S (load BMP/PNG from SD, rotate)
- **RAM**: PSRAM for image decode.

### 4.6 閒置/待機畫面設計
- **What**: Aesthetic idle/sleep screen (clock, quote, book cover, artwork).
- **Files**: `SleepActivity.cpp`, new `IdleScreenActivity.cpp`
- **Complexity**: M (design + template system)
- **RAM**: Low — static assets in Flash.

### 4.7 關機桌布自訂 + 資料夾輪播
- **What**: Custom shutdown wallpaper, optionally cycling through a folder of images.
- **Files**: `SleepActivity.cpp`, wallpaper folder scanner
- **Complexity**: S (file listing + random pick)
- **RAM**: PSRAM for image decode.

---

## Phase 5 — Advanced & Polish (3–4 weeks)

### 5.1 閱讀進度視覺化（電子雞養成）
- **What**: Gamified reading progress — virtual pet that grows with reading time.
- **Approach**: Pixel-art pet states tracked by daily reading minutes. Display on home screen.
- **Files**: New `ReadingPetActivity.cpp`, `ReadingPetStore.cpp`
- **Complexity**: M (state machine + pixel art + daily tracking)
- **RAM**: Low — pet state is tiny.

### 5.2 閱讀進度互動化（提升黏著度）
- **What**: Reading streaks, daily goals, achievement badges.
- **Files**: New `ReadingStatsActivity.cpp`, extend `DesktopSummaryStore.cpp`
- **Complexity**: M (stats tracking + badge system)
- **RAM**: Minimal stats stored on SD.

### 5.3 支援 XTC / XTCH 格式
- **Status**: `XtcReaderActivity.cpp` and `lib/Xtc/` already exist. Verify completeness and add XTCH if needed.
- **Complexity**: S (verify + extend parser)
- **RAM**: Existing.

### 5.4 書中圖片解析度調整 / 滿版
- **What**: Settings for image resolution (full/half/quarter) and full-bleed rendering.
- **Files**: `ImageBlock.cpp`, `EpubReaderMenuActivity.h` (add image quality setting)
- **Complexity**: S–M
- **RAM**: Lower res = less PSRAM.

---

## Feature → Phase Quick Reference

| # | Feature | Phase | Complexity | RAM Impact |
|---|---------|-------|------------|------------|
| 一1 | 九宫格自訂義 | 2.5 | M | Low |
| 一2 | 简转繁 | 1.3 | S | Flash table |
| 一3 | EPUB显示图片 | 1.1 | M | PSRAM |
| 一4 | 註解超連結 | 2.1 | M | Low |
| 一5 | 书籍封面显示 | 1.2 | S–M | Low |
| 一6 | 阅读进度可视化 | 5.1 | M | Low |
| 一7 | 直排/横排 | 3.2 | XL | Normal |
| 一8 | 字体调整大小 | 1.6 | M | Reuse cache |
| 一9 | 显示页码 | 1.4 | S | Negligible |
| 一10 | 搜尋功能 | 2.2 | M | PSRAM |
| 一11 | 書籤功能 | 2.3 | S–M | Low |
| 一12 | 2-bit灰阶抗锯齿 | 3.1 | L | Double BW+RED |
| 一13 | 一键关闭触控 | 1.5 | S | 1 byte |
| 一14 | 英文排版+字典 | 3.3+3.4 | M–L | Flash index |
| 一15 | 画线功能 | 2.4 | L | Low |
| 二1 | 便利贴 | 4.1 | M | Low |
| 二2 | 设定同步 | 4.4 | M | Low |
| 二3 | XTC/XTCH | 5.3 | S | Existing |
| 二4 | EPUB封面辨识 | 1.2 | — | (merged) |
| 二5 | 求籤擲硬幣 | 4.2 | S | Minimal |
| 二6 | 自建單字背單字 | 4.3 | M | Minimal |
| 三1 | 书中图片正常显示 | 1.1 | — | (merged) |
| 三2 | 桌布自訂 | 4.5 | S | PSRAM |
| 三3 | 桌布旋转180° | 4.5 | S | None |
| 三4 | 閒置待机画面 | 4.6 | M | Low |
| 三5 | 灰阶半刷新 | 3.1 | — | (prerequisite) |
| 四1 | 关机桌布轮播 | 4.7 | S | PSRAM |
| 四2 | 进度互动化 | 5.2 | M | Low |

---

## Technical Risks

1. **2-bit grayscale anti-aliasing (Phase 3.1)**: Font files double in size. All glyph rendering paths change. This is the highest-risk single feature.
2. **Vertical writing mode (Phase 3.2)**: Requires a parallel layout engine. Could be scoped to CJK-only to reduce complexity.
3. **PSRAM dependency**: Mofei (ESP32-S3 + 8MB PSRAM) removes most RAM constraints, but X4 (ESP32-C3, no PSRAM) features must stay compatible. Use `#if MOFEI_DEVICE` guards.
4. **Touch interaction quality**: E-ink + resistive touch has ~100ms latency. Text selection and drawing features need generous touch targets.

---

## Priority Recommendation

**Ship Phase 1 first** — these 6 features dramatically improve daily reading with manageable complexity. Then collect user feedback before committing to Phase 3's heavy rendering changes.
