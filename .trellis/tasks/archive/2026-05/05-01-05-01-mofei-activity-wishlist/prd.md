# Mofei Activity Wishlist

Date: 2026-05-01
Owner: Codex

## Problem

User feedback describes a broad "wish card" for the Mofei reader experience: richer reading controls, visual book/library
surfaces, notes, settings sync, wallpaper/sleep customization, lightweight fortune/coin tools, vocabulary study, and display
quality improvements. The current app already has many building blocks, including Dashboard, ReadingHub, EpubReader,
EpubReaderMenu, footnotes, XTC reader, study queues, arcade hub, sleep screen, settings, generic touch events, and Mofei
flash validation. The next work should turn those capabilities into discoverable touch-first activities without regressing
button-first X3/X4 workflows.

## User Wishlist

### Reading

- Add recent reading into a configurable 9-grid dashboard.
- Simplified-to-traditional conversion.
- EPUB image display, including inline images and covers.
- Footnote hyperlinks that open notes in a popup or note view.
- Visual book list with covers.
- Visualized reading progress, eventually gamified.
- Custom horizontal/vertical reading layout.
- Direct font size adjustment.
- Page number display.
- Search.
- Bookmarks.
- Reader font support for 2-bit grayscale anti-aliasing, not 1-bit dithering.
- One-tap touch lock for button-only page turning.
- English layout improvements and built-in English dictionary.
- Highlight/underline support.

### System And Features

- Sticky notes / todo activity.
- Settings sync across devices.
- XTC / XTCH support.
- EPUB cover detection.
- Fortune draw and coin flip tools.
- User-built vocabulary memorization.

### Display And Experience

- Normal book image display with adjustable resolution and fullscreen/fill modes.
- Custom wallpaper.
- Wallpaper 180-degree rotation.
- Better idle/sleep screen design.
- Grayscale partial refresh to reduce ghosting and flashing.

### Advanced

- Custom shutdown wallpaper or folder slideshow.
- Interactive reading progress to improve engagement.

## Current Evidence

- `DashboardActivity` is the Mofei home entry point and already routes Weather, Today, Study, Recent Books, Desktop,
  Reading, Arcade, Import, File Browser, and Settings.
- `EpubReaderActivity` already has generic touch zones, footnote navigation, go-to-percent, rotation, KOReader sync,
  screenshot, QR display, progress saving, page status rendering, image-rendering settings, and 2-bit grayscale render
  passes when text anti-aliasing is enabled.
- `EpubReaderMenuActivity` is the right first integration point for reader controls; it already supports touch hit-testing
  and button navigation.
- `XtcReaderActivity` already renders XTC pages and logs 2-bit grayscale rendering.
- `StudyHubActivity` and existing study activities can host a vocabulary memorization flow after a wordbook store exists.
- `ArcadeHubActivity` can host lightweight fortune/coin tools.
- `SleepActivity` is the correct surface for wallpaper/sleep/shutdown customization.

## Phase 1 Requirements

Implement the first shippable slice only. Keep the code narrowly scoped and commit-ready.

1. Add a configurable Mofei dashboard grid/list management entry that lets the user inspect and reorder a 9-slot activity
   layout. It does not need full drag-and-drop; button/touch list controls are acceptable.
2. Store dashboard shortcut choices in a small local settings/store file under the existing storage conventions. Defaults
   must include recent reading, reading hub, study, desktop, arcade, import/sync, file browser, settings, and weather/today
   style utilities.
3. Make the Dashboard render/open logic consume this shortcut store instead of hardcoding only a fixed 11-row menu. Preserve
   a clear way to open Settings and File Browser.
4. Add reader menu actions for direct font-size down/up and touch lock. Font-size changes must persist to `SETTINGS` and
   force the current EPUB section to reflow while keeping approximate reading position.
5. Touch lock should disable native reader tap-zone page turns while still allowing physical button navigation. It must be
   reversible from the reader menu and must not disable button hint taps in menu activities.
6. Add bookmark add/remove and bookmark list entry points only if they can be done without building a large new annotation
   system in this slice. If not, leave explicit TODO hooks and keep the menu items out of production UI.
7. Preserve existing button navigation on X3/X4 and Mofei.
8. Do not implement high-risk renderer work in this slice: vertical layout, 2-bit font engine changes, grayscale partial
   refresh tuning, EPUB image pipeline changes, or highlight drawing.

## Follow-Up Roadmap

1. Cover and image pipeline: EPUB cover extraction, library cover thumbnails, inline image fit/fill/fullscreen settings.
2. Reader navigation data model: search, bookmarks, footnote popup polish, highlights/underlines.
3. Language/layout: simplified-to-traditional conversion, English layout tuning, vertical/horizontal layout.
4. Study integration: dictionary lookup, wordbook store, vocabulary review activity.
5. Desktop tools: sticky notes/todos, fortune draw, coin flip.
6. Wallpaper/sleep: wallpaper picker, 180-degree rotation, shutdown wallpaper/slideshow.
7. Display HAL: grayscale partial refresh policy and any 2-bit text-rendering hardening.
8. Sync: local import/export first, then cross-device settings sync.

## Acceptance Criteria

- Dashboard can show/open the default 9 shortcuts and a customization activity.
- The customization activity is navigable by buttons and touch, persists changes, and does not crash on empty/corrupt config.
- Reader menu exposes font size up/down and touch lock actions.
- Changing font size from the reader menu persists settings and causes EPUB reflow without losing the current chapter.
- Touch lock prevents native reader screen tap page turns while physical buttons still turn pages.
- `git diff --check` passes.
- `pio run -e mofei` passes.
- A Mofei device flash is attempted with `scripts/mofei_flash_validate.sh --port /dev/cu.usbmodem1101` whenever the device
  is attached. Record the exact result.

## Non-Goals For Phase 1

- No new EPUB parser replacement.
- No new rendering engine.
- No broad display HAL refresh changes.
- No cloud settings sync.
- No full annotation database unless it is already small and naturally available.
- No removal of existing physical button behavior.
