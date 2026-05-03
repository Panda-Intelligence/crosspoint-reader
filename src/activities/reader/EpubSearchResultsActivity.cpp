#include "EpubSearchResultsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kSearchStartY = 60;
constexpr int kSearchLineHeight = 36;

Rect searchContentRect(const GfxRenderer& renderer) {
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

void EpubSearchResultsActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void EpubSearchResultsActivity::onExit() { Activity::onExit(); }

int EpubSearchResultsActivity::getPageItems() const {
  const Rect contentRect = searchContentRect(renderer);
  const int availableHeight =
      contentRect.height - kSearchStartY - UITheme::getInstance().getMetrics().buttonHintsHeight;
  return std::max(1, availableHeight / kSearchLineHeight);
}

void EpubSearchResultsActivity::openSelectedResult() {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(results.size())) {
    return;
  }

  setResult(PageResult{results[selectedIndex].page});
  finish();
}

std::string EpubSearchResultsActivity::labelForResult(const EpubSearchResult& result) const {
  return std::string(tr(STR_PAGE_LABEL)) + " " + std::to_string(result.page + 1) + "  " + result.snippet;
}

void EpubSearchResultsActivity::loop() {
  const int totalItems = static_cast<int>(results.size());
  const int pageItems = getPageItems();

  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && totalItems > 0 && touchEvent.isTap()) {
      const Rect contentRect = searchContentRect(renderer);
      const Rect listRect{contentRect.x, contentRect.y + kSearchStartY, contentRect.width,
                          contentRect.height - kSearchStartY};
      const int clickedIndex =
          TouchHitTest::listItemAt(listRect, kSearchLineHeight, selectedIndex, totalItems, touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        openSelectedResult();
        return;
      }
      mappedInput.suppressTouchButtonFallback();
      return;
    } else if (!buttonHintTap && totalItems > 0 && TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::nextPageIndex(selectedIndex, totalItems, pageItems);
      requestUpdate();
      return;
    } else if (!buttonHintTap && totalItems > 0 && TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::previousPageIndex(selectedIndex, totalItems, pageItems);
      requestUpdate();
      return;
    } else if (!buttonHintTap) {
      mappedInput.suppressTouchButtonFallback();
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    openSelectedResult();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  buttonNavigator.onNextRelease([this, totalItems] {
    if (totalItems > 0) {
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, totalItems);
      requestUpdate();
    }
  });

  buttonNavigator.onPreviousRelease([this, totalItems] {
    if (totalItems > 0) {
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, totalItems);
      requestUpdate();
    }
  });

  buttonNavigator.onNextContinuous([this, totalItems, pageItems] {
    if (totalItems > 0) {
      selectedIndex = ButtonNavigator::nextPageIndex(selectedIndex, totalItems, pageItems);
      requestUpdate();
    }
  });

  buttonNavigator.onPreviousContinuous([this, totalItems, pageItems] {
    if (totalItems > 0) {
      selectedIndex = ButtonNavigator::previousPageIndex(selectedIndex, totalItems, pageItems);
      requestUpdate();
    }
  });
}

void EpubSearchResultsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto orientation = renderer.getOrientation();
  const bool isLandscapeCw = orientation == GfxRenderer::Orientation::LandscapeClockwise;
  const bool isLandscapeCcw = orientation == GfxRenderer::Orientation::LandscapeCounterClockwise;
  const bool isPortraitInverted = orientation == GfxRenderer::Orientation::PortraitInverted;
  const int hintGutterWidth = (isLandscapeCw || isLandscapeCcw) ? 30 : 0;
  const int contentX = isLandscapeCw ? hintGutterWidth : 0;
  const int contentWidth = pageWidth - hintGutterWidth;
  const int hintGutterHeight = isPortraitInverted ? 50 : 0;
  const int contentY = hintGutterHeight;
  const int contentHeight = renderer.getScreenHeight() - contentY;

  const int titleX =
      contentX + (contentWidth - renderer.getTextWidth(UI_12_FONT_ID, tr(STR_SEARCH_RESULTS), EpdFontFamily::BOLD)) / 2;
  renderer.drawText(UI_12_FONT_ID, titleX, 15 + contentY, tr(STR_SEARCH_RESULTS), true, EpdFontFamily::BOLD);

  if (results.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getTextYForCentering(contentY, contentHeight, UI_10_FONT_ID),
                              tr(STR_NO_SEARCH_RESULTS));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  const int pageItems = getPageItems();
  const int pageStartIndex = selectedIndex / pageItems * pageItems;
  const int pageEndIndex = std::min(static_cast<int>(results.size()), pageStartIndex + pageItems);

  renderer.fillRect(contentX, kSearchStartY + contentY + (selectedIndex - pageStartIndex) * kSearchLineHeight - 2,
                    contentWidth - 1, kSearchLineHeight);

  for (int i = pageStartIndex; i < pageEndIndex; i++) {
    const int y = kSearchStartY + contentY + (i - pageStartIndex) * kSearchLineHeight;
    const bool isSelected = (i == selectedIndex);
    const std::string label =
        renderer.truncatedText(UI_10_FONT_ID, labelForResult(results[i]).c_str(), contentWidth - 40);
    renderer.drawText(UI_10_FONT_ID, contentX + 20, y + 3, label.c_str(), !isSelected);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
