#include "EpubReaderFootnotesActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kFootnoteStartY = 72;
constexpr int kFootnoteLineHeight = 42;

Rect footnoteContentRect(const GfxRenderer& renderer) {
  const auto orientation = renderer.getOrientation();
  const bool isLandscapeCw = orientation == GfxRenderer::Orientation::LandscapeClockwise;
  const bool isLandscapeCcw = orientation == GfxRenderer::Orientation::LandscapeCounterClockwise;
  const bool isPortraitInverted = orientation == GfxRenderer::Orientation::PortraitInverted;
  const int hintGutterWidth = (isLandscapeCw || isLandscapeCcw) ? 30 : 0;
  const int contentX = isLandscapeCw ? hintGutterWidth : 0;
  const int hintGutterHeight = isPortraitInverted ? 50 : 0;
  return Rect{contentX, hintGutterHeight, renderer.getScreenWidth() - hintGutterWidth,
              renderer.getScreenHeight() - hintGutterHeight};
}
}  // namespace

void EpubReaderFootnotesActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void EpubReaderFootnotesActivity::onExit() { Activity::onExit(); }

void EpubReaderFootnotesActivity::openSelectedFootnote() {
  if (selectedIndex >= 0 && selectedIndex < static_cast<int>(footnotes.size())) {
    setResult(FootnoteResult{footnotes[selectedIndex].href});
    finish();
  }
}

void EpubReaderFootnotesActivity::showSelectedFootnoteDetail() {
  if (selectedIndex >= 0 && selectedIndex < static_cast<int>(footnotes.size())) {
    showingDetail = true;
    requestUpdate();
  }
}

void EpubReaderFootnotesActivity::closeFootnoteDetail() {
  showingDetail = false;
  requestUpdate();
}

void EpubReaderFootnotesActivity::loop() {
  if (showingDetail) {
    InputTouchEvent touchEvent;
    if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
      const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
      if (!buttonHintTap && touchEvent.isTap()) {
        mappedInput.suppressTouchButtonFallback();
        openSelectedFootnote();
        return;
      }
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      closeFootnoteDetail();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      openSelectedFootnote();
      return;
    }
    return;
  }

  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && !footnotes.empty() && touchEvent.isTap()) {
      const Rect contentRect = footnoteContentRect(renderer);
      const int visibleCount = std::max(1, contentRect.height / kFootnoteLineHeight);
      const Rect listRect{contentRect.x, contentRect.y + kFootnoteStartY, contentRect.width,
                          contentRect.height - kFootnoteStartY};
      const int clickedRow = TouchHitTest::pointInRect(touchEvent.x, touchEvent.y, listRect)
                                 ? (touchEvent.y - listRect.y) / kFootnoteLineHeight
                                 : -1;
      const int clickedIndex = clickedRow >= 0 && clickedRow < visibleCount ? scrollOffset + clickedRow : -1;
      if (clickedIndex >= 0 && clickedIndex < static_cast<int>(footnotes.size())) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        showSelectedFootnoteDetail();
        return;
      }
    } else if (!buttonHintTap && !footnotes.empty() && TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(footnotes.size()));
      requestUpdate();
      return;
    } else if (!buttonHintTap && !footnotes.empty() && TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(footnotes.size()));
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    showSelectedFootnoteDetail();
    return;
  }

  buttonNavigator.onNext([this] {
    if (!footnotes.empty()) {
      selectedIndex = (selectedIndex + 1) % footnotes.size();
      requestUpdate();
    }
  });

  buttonNavigator.onPrevious([this] {
    if (!footnotes.empty()) {
      selectedIndex = (selectedIndex - 1 + footnotes.size()) % footnotes.size();
      requestUpdate();
    }
  });
}

void EpubReaderFootnotesActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto orientation = renderer.getOrientation();
  // Landscape orientation: reserve a horizontal gutter for button hints.
  const bool isLandscapeCw = orientation == GfxRenderer::Orientation::LandscapeClockwise;
  const bool isLandscapeCcw = orientation == GfxRenderer::Orientation::LandscapeCounterClockwise;
  // Inverted portrait: reserve vertical space for hints at the top.
  const bool isPortraitInverted = orientation == GfxRenderer::Orientation::PortraitInverted;
  const int hintGutterWidth = (isLandscapeCw || isLandscapeCcw) ? 30 : 0;
  // Landscape CW places hints on the left edge; CCW keeps them on the right.
  const int contentX = isLandscapeCw ? hintGutterWidth : 0;
  const int contentWidth = pageWidth - hintGutterWidth;
  const int hintGutterHeight = isPortraitInverted ? 50 : 0;
  const int contentY = hintGutterHeight;

  // Manual centering to honor content gutters.
  const int titleX =
      contentX + (contentWidth - renderer.getTextWidth(UI_12_FONT_ID, tr(STR_FOOTNOTES), EpdFontFamily::BOLD)) / 2;
  renderer.drawText(UI_12_FONT_ID, titleX, 15 + contentY, tr(STR_FOOTNOTES), true, EpdFontFamily::BOLD);
  std::string countLine = std::to_string(footnotes.size()) + " " + tr(STR_FOOTNOTE_COUNT_SUFFIX);
  renderer.drawCenteredText(SMALL_FONT_ID, 43 + contentY, countLine.c_str());

  if (footnotes.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID,
                              renderer.getTextYForCentering(contentY, pageHeight - contentY, UI_10_FONT_ID),
                              tr(STR_NO_FOOTNOTES));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (showingDetail && selectedIndex >= 0 && selectedIndex < static_cast<int>(footnotes.size())) {
    const auto& footnote = footnotes[selectedIndex];
    int y = kFootnoteStartY + contentY;
    std::string label = footnote.number;
    if (label.empty()) {
      label = tr(STR_LINK);
    }
    label = renderer.truncatedText(UI_12_FONT_ID, label.c_str(), contentWidth - 40, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, contentX + 20, y, label.c_str(), true, EpdFontFamily::BOLD);
    y += 34;

    std::string href = footnote.href;
    if (href.empty()) {
      href = tr(STR_LINK);
    }
    const auto lines = renderer.wrappedText(UI_10_FONT_ID, href.c_str(), contentWidth - 40, 5);
    for (const auto& line : lines) {
      renderer.drawText(UI_10_FONT_ID, contentX + 20, y, line.c_str(), true);
      y += 24;
    }

    renderer.drawText(SMALL_FONT_ID, contentX + 20, y + 10, tr(STR_FOOTNOTE_DETAIL_HINT), true);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OPEN), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  const int screenWidth = renderer.getScreenWidth();
  const int marginLeft = contentX + 20;

  const int visibleCount = std::max(1, (renderer.getScreenHeight() - contentY) / kFootnoteLineHeight);
  if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
  if (selectedIndex >= scrollOffset + visibleCount) scrollOffset = selectedIndex - visibleCount + 1;

  for (int i = scrollOffset; i < static_cast<int>(footnotes.size()) && i < scrollOffset + visibleCount; i++) {
    const int y = kFootnoteStartY + contentY + (i - scrollOffset) * kFootnoteLineHeight;
    const bool isSelected = (i == selectedIndex);

    if (isSelected) {
      renderer.fillRect(0, y, screenWidth, kFootnoteLineHeight, true);
    }

    std::string label = footnotes[i].number;
    if (label.empty()) {
      label = tr(STR_LINK);
    }
    label = renderer.truncatedText(UI_10_FONT_ID, label.c_str(), contentWidth - 40);
    renderer.drawText(UI_10_FONT_ID, marginLeft, y + 4, label.c_str(), !isSelected);

    std::string href = footnotes[i].href;
    if (!href.empty()) {
      href = renderer.truncatedText(SMALL_FONT_ID, href.c_str(), contentWidth - 40);
      renderer.drawText(SMALL_FONT_ID, marginLeft, y + 23, href.c_str(), !isSelected);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
