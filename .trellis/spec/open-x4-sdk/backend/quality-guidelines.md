# Quality Guidelines

> Code quality standards for backend development.

---

## Overview

<!--
Document your project's quality standards here.

Questions to answer:
- What patterns are forbidden?
- What linting rules do you enforce?
- What are your testing requirements?
- What code review standards apply?
-->

(To be filled by the team)

---

## Forbidden Patterns

<!-- Patterns that should never be used and why -->

### Do Not Trust I2C Address ACKs As Device Identity

An I2C ACK or successful register read only proves that something answered on
the bus. It is not enough to identify a chip. Mofei touch support is the
current example: the working-system evidence points production Mofei FT6336U
traffic at 7-bit `0x2E`, while the vendor/sample `0x38` path can return
AHT20-style frames on the same board bus.

Wrong:

```cpp
uint8_t status = 0;
if (readFt6336(FT6336_REG_TD_STATUS, &status, 1)) {
  ready = true;
}
```

Correct:

```cpp
uint8_t data[FT6336_FRAME_LEN] = {};
const bool readOk = readRegister(FT6336_REG_TD_STATUS, data, sizeof(data));
ready = readOk && validateFt6336Frame(data, sizeof(data), "detect") == Ft6336FrameStatus::Valid;
```

For FT6336U specifically, validate at least these contract points before
marking the controller ready:

- `TD_STATUS` high nibble must be zero.
- point count must be `0..2`.
- touch point event code must not be reserved.
- raw coordinates must fit the configured touch geometry.
- AHT20-like frames such as `98 38 80 76 41` must be rejected and logged as
  the wrong device at the active `MOFEI_TOUCH_ADDR`, not parsed as touch data.

For Mofei production builds, the active FT6336U address must come from
`MOFEI_TOUCH_ADDR` and default to the working-system 7-bit address `0x2E`.
The vendor/sample `0x38` address is allowed only through explicit diagnostic
build flags or environments.

---

## Required Patterns

<!-- Patterns that must always be used -->

### Preserve Native Touch Event Ownership

Mofei touch input must cross the HAL boundary as the generic
`InputTouchEvent` contract, not as `MofeiTouchDriver::Event` or FT6336U
register data.

#### 1. Scope / Trigger

Use this rule whenever a screen handles taps, swipes, hit-testing, or button
hint taps from touch input. It prevents the same physical touch from being
processed twice: once as a native coordinate event and again as the legacy
touch-to-button fallback.

#### 2. Signatures

```cpp
struct InputTouchEvent {
  enum class Type : uint8_t { None, Tap, SwipeLeft, SwipeRight, SwipeUp, SwipeDown };
  Type type;
  uint16_t x;
  uint16_t y;
  bool isTap() const;
};

bool MappedInputManager::consumeTouchEvent(InputTouchEvent* outEvent) const;
void MappedInputManager::suppressTouchButtonFallback() const;
bool MappedInputManager::isTouchButtonHintTap(const InputTouchEvent& event) const;
```

#### 3. Contracts

- HAL/input code converts chip-specific events into `InputTouchEvent`.
- Activity code may branch on generic touch types and shared hit-test helpers.
- Activity code must not include `MofeiTouch.h` or inspect FT6336U registers.
- A native consumer owns a touch after `consumeTouchEvent()` returns `true`.
- If the screen handles that touch natively, it must call
  `suppressTouchButtonFallback()` before any later `wasPressed()` or
  `wasReleased()` checks in the same loop.
- Button-hint/footer taps may intentionally be left to fallback by checking
  `isTouchButtonHintTap()` and not suppressing them.

#### 4. Validation & Error Matrix

| Condition | Required behavior |
| --- | --- |
| Native screen handles tap or swipe | Suppress fallback and return or avoid button checks for that event |
| Touch is a button-hint tap | Let fallback run unless the screen handles the hint itself |
| Screen only supports buttons | Do not consume the touch; HAL fallback keeps buttons working |
| Reader orientation differs from touch orientation | Transform through shared helpers before hit-testing |

#### 5. Good/Base/Bad Cases

- Good: `EpubReaderActivity` consumes a touch, transforms it with
  `TouchHitTest::eventForRendererOrientation()`, then suppresses fallback.
- Base: a button-only screen ignores `consumeTouchEvent()` and relies on the
  existing virtual button adapter.
- Bad: an activity consumes a tap, opens a menu, then lets the same tap also
  reach `wasReleased(Button::Confirm)`.

#### 6. Tests Required

- Run `pio run -e mofei` for compile coverage.
- Run `pio check -e mofei --fail-on-defect low --fail-on-defect medium --fail-on-defect high`
  after broad input changes.
- On device, flash `env:mofei` and confirm serial logs show
  `FT6336U status=ready ... addr=0x2E`.

#### 7. Wrong vs Correct

Wrong:

```cpp
MofeiTouchDriver::Event event;
if (touch.update(&event) && event.type == MofeiTouchDriver::EventType::Tap) {
  openItem(event.x, event.y);
}
if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
  openSelectedItem();
}
```

Correct:

```cpp
InputTouchEvent event;
if (mappedInput.consumeTouchEvent(&event)) {
  const bool buttonHintTap = mappedInput.isTouchButtonHintTap(event);
  if (!buttonHintTap && event.isTap()) {
    mappedInput.suppressTouchButtonFallback();
    openItem(event.x, event.y);
    return;
  }
}
if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
  openSelectedItem();
}
```

### Treat Mofei GPIO45 As A Sensitive Strap-Capable Output

For Mofei touch power, GPIO45 is documented by `docs/mofei/ic.png` as
`TOUCH_PWR`. The local ESP32-S3 datasheet (`docs/mofei/esp32-s3_datasheet_en.pdf`)
shows GPIO45 is `I/O/T` and can be used as regular IO after reset, but it is
also the `VDD_SPI` voltage strapping pin sampled at chip reset.

Do not mark GPIO45 unusable as a normal output based on an input-only
assumption. Keep production behavior aligned with the schematic unless direct
hardware evidence disproves it. Any experiment that disables or changes GPIO45
power driving must use an explicit diagnostic build flag or environment, and
must not become a broad GPIO scan in production firmware.

### Restrict Mofei TC Font Hot-Swap To TC_8 And TC_10

PSRAM on Mofei (8 MB) is shared by the EPF font pack, the FontDecompressor
hot-group buffer (~3.3 MB peak for builtin Noto CJK fonts), reader caches, and
image decoders. Loading two TC packs simultaneously fragments the heap below
the 3.3 MB contiguous block the decompressor needs and starves builtin glyph
rendering. Lock the resident TC pack set to two compact sizes and hot-swap
between them.

#### 1. Scope / Trigger

Use this rule whenever changing `StorageFontRegistry`, adding a new TC font
size, plumbing a new caller through `loadTraditionalChineseFont*`, or routing
`SETTINGS.fontSize` to a TC pack.

#### 2. Signatures

```cpp
constexpr int kStartupTraditionalChineseFontId = NOTOSANS_TC_10_FONT_ID;
constexpr int kSmallTraditionalChineseFontId = NOTOSANS_TC_10_FONT_ID;
constexpr int kReaderExtraSmallTraditionalChineseFontId = NOTOSANS_TC_8_FONT_ID;

constexpr bool isAllowedTraditionalChineseFontId(int fontId);
const PackRuntime* getRuntimeByFontSize(uint8_t fontSize);
bool loadTraditionalChineseRuntime(GfxRenderer& renderer, const PackRuntime& target,
                                   bool forceReload = false);

bool StorageFontRegistry::loadTraditionalChineseFonts(GfxRenderer& renderer);
bool StorageFontRegistry::loadTraditionalChineseFont(GfxRenderer& renderer, uint8_t fontSize);
bool StorageFontRegistry::loadTraditionalChineseFontById(GfxRenderer& renderer, int fontId);
int  StorageFontRegistry::getCurrentTraditionalChineseFontId();
```

#### 3. Contracts

- `isAllowedTraditionalChineseFontId(fontId)` returns `true` only for
  `NOTOSANS_TC_8_FONT_ID` or `NOTOSANS_TC_10_FONT_ID`. Any other id is
  rejected with `LOG_ERR("TCFONT", ...)` and `false`.
- `loadTraditionalChineseRuntime()` is the single chokepoint. All three public
  load entry points (`loadTraditionalChineseFonts`, `loadTraditionalChineseFont`,
  `loadTraditionalChineseFontById`) eventually call it; no caller may bypass.
- Hot-swap rule: before loading the new pack, `loadTraditionalChineseRuntime`
  iterates `kPackRuntimes` and calls `unloadTraditionalChineseFont(...)` on
  every pack whose `fontId` differs from the target (or every pack when
  `forceReload == true`). At most one TC pack is resident at any time.
- Routing rule: `getRuntimeByFontSize(fontSize)` returns `TC_8` only when
  `fontSize == CrossPointSettings::EXTRA_SMALL`; every other reader font size
  routes to `TC_10`. Reader text below 12 pt is the explicit trade-off.
- Boot path: `setupDisplayAndFonts()` calls `loadTraditionalChineseFonts()`
  which loads `kStartupTraditionalChineseFontId` (= `TC_10`).
  `updateUiFontMapping()` then queries `getCurrentTraditionalChineseFontId()`
  and remaps `UI_10/UI_12/SMALL` slots to that id.
- Reader path: `EpubReaderActivity::currentReaderFontId()` calls
  `loadTraditionalChineseFont(renderer, SETTINGS.fontSize)`. If the active id
  changed, it follows up with `updateUiFontMapping()` so the UI strip stays
  consistent.
- Web upload path: `src/network/CrossPointWebServer.cpp` re-runs
  `loadTraditionalChineseFonts(renderer)` after a pack upload. The whitelist
  still gates the actual load; uploads of TC_12+ files do not go resident.
- Fallback rule: when the preferred pack file is missing on the SD card,
  `getRuntimeByFontSize` returns the alternate allowed pack (TC_10 ↔ TC_8)
  so something CJK still renders.

#### 4. Validation & Error Matrix

| Condition | Required behavior |
| --- | --- |
| Caller asks to load TC_12 / TC_14 / TC_16 / TC_18 | `loadTraditionalChineseRuntime` logs ERR + returns `false`; nothing loads |
| Reader enters EXTRA_SMALL with TC_10 resident | Hot-swap unloads TC_10 first, loads TC_8, then `updateUiFontMapping()` runs |
| Reader exits EXTRA_SMALL with TC_8 resident | Hot-swap unloads TC_8 first, loads TC_10, then `updateUiFontMapping()` runs |
| TC_10 file missing on SD, TC_8 present | Boot falls back to TC_8; UI is small but still CJK |
| Both TC_8 and TC_10 missing | `loadTraditionalChineseFonts` returns `false`; UI stays Latin Ubuntu, no crash |
| `forceReload == true` (boot path) | Every previously-resident pack is unloaded even if the target was already loaded |

#### 5. Good/Base/Bad Cases

- Good: Add a new compact size by extending the whitelist after measuring its
  PSRAM impact and confirming `(new_size + 3.3 MB hot-group + reader caches) <
  total PSRAM budget`. Update `isAllowedTraditionalChineseFontId` and
  `getRuntimeByFontSize` together with a feature flag.
- Base: A new reader feature mutates `SETTINGS.fontSize`. It must call
  `StorageFontRegistry::loadTraditionalChineseFont(renderer, newSize)` and
  follow with `updateUiFontMapping()` so the UI strip resolves the active id.
- Bad: Calling `target.family->regular->load(path)` directly, or registering a
  font via `renderer.insertFont(NOTOSANS_TC_14_FONT_ID, family)` from outside
  `loadTraditionalChineseRuntime`. This bypasses the whitelist guard and the
  unload-other invariant.

#### 6. Tests Required

- `git diff --check`.
- `pio run -e mofei` (production build).
- Mofei device flash via
  `scripts/mofei_flash_validate.sh --port /dev/cu.usbmodemXXXX`.
- Capture boot serial. The opening sequence must contain exactly one
  `[TCFONT_MEM] load_begin path=/.mofei/fonts/notosans_tc_10.epf` followed by
  the matching `load_end` line, and `[UIFONT] Current TC UI font id: 1242001001`
  (= `NOTOSANS_TC_10_FONT_ID`). A `1242000801` value (= `NOTOSANS_TC_8_FONT_ID`)
  at boot is a regression unless the device is intentionally launched into
  EXTRA_SMALL reader mode.

#### 7. Wrong vs Correct

Wrong:

```cpp
// Loads TC_14 directly, sidestepping the whitelist guard, leaves TC_10 still
// resident, fragments PSRAM below the 3.3 MB hot-group threshold.
auto* face = StorageFontRegistry::getTraditionalChineseFontFaceById(
    NOTOSANS_TC_14_FONT_ID, EpdFontFamily::REGULAR);
tc14Family.regular->load(face->path);
renderer.insertFont(NOTOSANS_TC_14_FONT_ID, *tc14Family.family);
```

Correct:

```cpp
// Routes through the chokepoint; non-EXTRA_SMALL sizes resolve to TC_10 via
// getRuntimeByFontSize, which unloads any other resident pack first.
StorageFontRegistry::loadTraditionalChineseFont(renderer, SETTINGS.fontSize);
updateUiFontMapping();
```

---

## Testing Requirements

<!-- What level of testing is expected -->

For Mofei firmware driver changes, run the narrow build gate:

```bash
scripts/mofei_flash_validate.sh --build-only
```

When a Mofei device is attached, build-only is not enough. Always follow with
a flash test on the actual USB port before calling the slice verified:

```bash
scripts/mofei_flash_validate.sh --port /dev/cu.usbmodemXXXX
```

For touch, display, input, boot, power, font, i18n, or UI rendering changes,
also inspect serial logs or perform the relevant manual screen interaction
after flashing. If flashing is impossible, record the exact upload/esptool
error and the physical recovery step needed before claiming verification.

---

## EPUB Reader Text Pipeline Patterns

These conventions come from the reader advanced wishlist implementation
(Task: `05-05-reader-advanced-wishlist`).

### Pattern: Text Conversion Hook Location

**Problem**: Where to insert text transforms so search, QR, and footnote labels
stay consistent with rendered text.

**Solution**: Insert conversions at parser ingestion time
(`ChapterHtmlSlimParser::flushPartWordBuffer`), **before** `ParsedText::addWord`.
A late render-only conversion would make search results, QR exports, and
footnote link labels inconsistent with on-screen text.

```cpp
// Correct — Parsed words contain already-converted text
void ChapterHtmlSlimParser::flushPartWordBuffer() {
    if (simplifiedToTraditional && !partWordBuffer.empty()) {
        std::string converted = EpubTextTransform::simplifiedToTraditional(partWordBuffer);
        // ... addWord with converted ...
    }
}

// Wrong — converting only at render time, search/QR/footnotes see raw text
void TextBlock::render(GfxRenderer& renderer, ...) {
    std::string displayText = EpubTextTransform::simplifiedToTraditional(words[0].word);
    renderer.drawText(..., displayText.c_str());
}
```

**Why**: All downstream consumers (page render, chapter search, QR text export,
footnote labels) read serialized `TextBlock` / `PageLine` data. The conversion
must be baked into the serialized payload.

### Pattern: Section Cache Invalidation

**Problem**: When a user toggles a text-affecting setting (conversion, layout,
font), cached chapter pages must be rebuilt or they'll show stale content.

**Solution**: Add the setting field to `Section` cache header, serialize it in
`createSectionFile`, deserialize and compare in `loadSectionFile`. Mismatch
triggers a full cache rebuild via `Section::createSectionFile`.

```cpp
// Section.cpp — header serialization
void Section::writeHeader(FILE* f) {
    writeUint32(f, simplifiedToTraditional ? 1 : 0);   // setting flag
    writeUint32(f, static_cast<uint32_t>(textLayout)); // vertical/horizontal
    // ... font, image, markup flags ...
}

// Section.cpp — header comparison at load time
bool Section::loadSectionFile(...) {
    // ...
    bool savedSimplifiedToTraditional = readUint32(f) != 0;
    if (savedSimplifiedToTraditional != simplifiedToTraditional) {
        // Cache mismatch — rebuild
        fclose(f);
        return createSectionFile(...);
    }
}
```

**Cache header lifetime**: New fields append to the header; bump
`Section::kSectionVersion` when changing the header format. Old caches are
automatically invalidated by version mismatch.

### Pattern: Vertical CJK Layout

**Problem**: Supporting vertical text layout where characters flow top-to-bottom
and columns flow right-to-left.

**Solution (compact, firmware-safe)**:
1. `ParsedText::applyVerticalTokenSplit()` — split CJK words into individual
   codepoints so each character occupies one grid cell.
2. `TextBlock::render()` — use `drawTextRotated90CW` for each character when
   `verticalLayout` flag is set.
3. `ChapterHtmlSlimParser::addLineToPage()` — compute X/Y offsets with
   RTL column placement: `pageX = pageRenderWidth - columnIndex * columnWidth`.
4. `BlockStyle` — add `topInset` / `bottomInset` / `totalVerticalInset` for
   vertical-aware margin calculation.

```cpp
// BlockStyle.h
int topInset() const { return marginTop + blockVerticalSpacing; }
int bottomInset() const { return marginBottom + blockVerticalSpacing; }
int totalVerticalInset() const { return topInset() + bottomInset(); }

// TextBlock.cpp — vertical rendering
if (verticalLayout) {
    renderer.drawTextRotated90CW(fontId, x, y + lineHeight, word.word.c_str());
}
```

### Pattern: Highlight Store with Range Anchors

**Problem**: Persisting highlight selections per book so they survive reboots.

**Solution**: Serialize `EpubHighlight` structs as JSON to
`.mofei/highlights/epub_<hash>.json`, using range anchors
(`startBlockIndex`, `startTokenIndex`, `endBlockIndex`, `endTokenIndex`)
for deduplication.

```cpp
// EpubHighlightStore.h
struct EpubHighlight {
    std::string snippet;
    std::string note;
    int startBlockIndex;   // TextBlock index in page
    int startTokenIndex;   // word/token index within block
    int endBlockIndex;
    int endTokenIndex;
};

// Deduplication by range anchor
bool hasRangeAnchor(int startBlk, int startTok, int endBlk, int endTok) const;
```

### Pattern: Full-Book Search Encoding

**Problem**: Search results need to encode which chapter (spine) and which page
they belong to, so selecting a result can navigate there.

**Solution**: Encode spine index in upper 16 bits and page number in lower 16
bits of the `EpubSearchResult::page` field.

```cpp
// Encoding in searchFullBook()
result.page = (spineIndex << 16) | pageInSection;

// Decoding at navigation time
int targetSpine = result.page >> 16;
int targetPage = result.page & 0xFFFF;
```

### i18n Workflow

**When adding new reader UI strings**:
1. Add keys to all three YAML files:
   `lib/I18n/translations/{english,simplified_chinese,traditional_chinese}.yaml`
2. Run `python3 scripts/gen_i18n.py` to regenerate `I18nKeys.h` and `I18nStrings.cpp`
3. The generator automatically fills 22 other languages with English fallback

**YAML key naming convention**: UPPER_SNAKE_CASE, e.g. `SEARCH_FULL_BOOK`,
`FOOTNOTE_BODY_TITLE`, `HIGHLIGHT_PREFIX`.

## Code Review Checklist

- [ ] Text transforms happen at parser level, not render level
- [ ] New text-affecting settings are added to Section cache header
- [ ] Section cache version is bumped when header format changes
- [ ] New i18n keys exist in all 3 target YAML files
- [ ] `pio run` passes (Flash < 95%, RAM < 35%)
- [ ] `pio check --fail-on-defect low --fail-on-defect medium --fail-on-defect high` passes
