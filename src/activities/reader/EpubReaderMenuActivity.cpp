#include "EpubReaderMenuActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kMenuStartY = 75;
constexpr int kMenuLineHeight = 30;

Rect menuContentRect(const GfxRenderer& renderer) {
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

int menuVisibleItemCount(const GfxRenderer& renderer) {
  const Rect contentRect = menuContentRect(renderer);
  const int availableHeight = contentRect.height - kMenuStartY - UITheme::getInstance().getMetrics().buttonHintsHeight;
  return std::max(1, availableHeight / kMenuLineHeight);
}
}  // namespace

EpubReaderMenuActivity::EpubReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                               const std::string& title, const int currentPage, const int totalPages,
                                               const int bookProgressPercent, const uint8_t currentOrientation,
                                               const bool hasFootnotes, const uint8_t fontSize,
                                               const bool touchLockEnabled, const bool currentPageBookmarked,
                                               const bool currentPageHighlighted)
    : Activity("EpubReaderMenu", renderer, mappedInput),
      menuItems(buildMenuItems(hasFootnotes)),
      title(title.empty() ? tr(STR_READING) : title),
      pendingOrientation(currentOrientation),
      currentFontSize(fontSize),
      touchLockEnabled(touchLockEnabled),
      currentPageBookmarked(currentPageBookmarked),
      currentPageHighlighted(currentPageHighlighted),
      currentPage(currentPage),
      totalPages(totalPages),
      bookProgressPercent(bookProgressPercent) {}

std::vector<EpubReaderMenuActivity::MenuItem> EpubReaderMenuActivity::buildMenuItems(bool hasFootnotes) {
  std::vector<MenuItem> items;
  items.reserve(10);
  items.push_back({MenuAction::SELECT_CHAPTER, StrId::STR_SELECT_CHAPTER});
  if (hasFootnotes) {
    items.push_back({MenuAction::FOOTNOTES, StrId::STR_FOOTNOTES});
  }
  items.push_back({MenuAction::SEARCH, StrId::STR_SEARCH_CURRENT_CHAPTER});
  items.push_back({MenuAction::FULL_BOOK_SEARCH, StrId::STR_SEARCH_FULL_BOOK});
  items.push_back({MenuAction::TOGGLE_BOOKMARK, StrId::STR_ADD_BOOKMARK});
  items.push_back({MenuAction::TOGGLE_HIGHLIGHT, StrId::STR_ADD_HIGHLIGHT});
  items.push_back({MenuAction::DICTIONARY_LOOKUP, StrId::STR_DICTIONARY_LOOKUP});
  items.push_back({MenuAction::BOOKMARKS, StrId::STR_BOOKMARKS});
  items.push_back({MenuAction::FONT_SIZE_DOWN, StrId::STR_READER_FONT_SIZE_DOWN});
  items.push_back({MenuAction::FONT_SIZE_UP, StrId::STR_READER_FONT_SIZE_UP});
  items.push_back({MenuAction::TOUCH_LOCK, StrId::STR_TOUCH_LOCK});
  items.push_back({MenuAction::ROTATE_SCREEN, StrId::STR_ORIENTATION});
  items.push_back({MenuAction::AUTO_PAGE_TURN, StrId::STR_AUTO_TURN_PAGES_PER_MIN});
  items.push_back({MenuAction::GO_TO_PERCENT, StrId::STR_GO_TO_PERCENT});
  items.push_back({MenuAction::SCREENSHOT, StrId::STR_SCREENSHOT_BUTTON});
  items.push_back({MenuAction::DISPLAY_QR, StrId::STR_DISPLAY_QR});
  items.push_back({MenuAction::GO_HOME, StrId::STR_GO_HOME_BUTTON});
  items.push_back({MenuAction::SYNC, StrId::STR_SYNC_PROGRESS});
  items.push_back({MenuAction::DELETE_CACHE, StrId::STR_DELETE_CACHE});
  return items;
}

void EpubReaderMenuActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void EpubReaderMenuActivity::onExit() { Activity::onExit(); }

void EpubReaderMenuActivity::selectCurrentItem() {
  const auto selectedAction = menuItems[selectedIndex].action;
  if (selectedAction == MenuAction::ROTATE_SCREEN) {
    // Cycle orientation preview locally; actual rotation happens on menu exit.
    pendingOrientation = (pendingOrientation + 1) % orientationLabels.size();
    requestUpdate();
    return;
  }

  if (selectedAction == MenuAction::AUTO_PAGE_TURN) {
    selectedPageTurnOption = (selectedPageTurnOption + 1) % pageTurnLabels.size();
    pageTurnOptionChanged = true;
    requestUpdate();
    return;
  }

  if (selectedAction == MenuAction::FONT_SIZE_DOWN) {
    if (currentFontSize > 0) {
      currentFontSize--;
      requestUpdate();
    }
    return;
  }

  if (selectedAction == MenuAction::FONT_SIZE_UP) {
    if (currentFontSize < fontSizeLabels.size() - 1) {
      currentFontSize++;
      requestUpdate();
    }
    return;
  }

  if (selectedAction == MenuAction::TOUCH_LOCK) {
    touchLockEnabled = !touchLockEnabled;
    requestUpdate();
    return;
  }

  setResult(MenuResult{static_cast<int>(selectedAction), pendingOrientation, selectedPageTurnOption,
                       pageTurnOptionChanged, currentFontSize, touchLockEnabled});
  finish();
}

void EpubReaderMenuActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      const Rect contentRect = menuContentRect(renderer);
      const Rect listRect{contentRect.x, contentRect.y + kMenuStartY, contentRect.width,
                          menuVisibleItemCount(renderer) * kMenuLineHeight};
      const int clickedIndex = TouchHitTest::listItemAt(listRect, kMenuLineHeight, selectedIndex,
                                                        static_cast<int>(menuItems.size()), touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        selectCurrentItem();
        return;
      }
      mappedInput.suppressTouchButtonFallback();
      return;
    } else if (!buttonHintTap && TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(menuItems.size()));
      requestUpdate();
      return;
    } else if (!buttonHintTap && TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(menuItems.size()));
      requestUpdate();
      return;
    } else if (buttonHintTap) {
      uint8_t buttonIndex = 0;
#if MOFEI_DEVICE
      if (gpio.mapMofeiButtonHintTapToButton(touchEvent.sourceX(), touchEvent.sourceY(), &buttonIndex)) {
        if (buttonIndex == HalGPIO::BTN_BACK) {
          ActivityResult result;
          result.isCancelled = true;
          result.data = MenuResult{
              -1, pendingOrientation, selectedPageTurnOption, pageTurnOptionChanged, currentFontSize, touchLockEnabled};
          setResult(std::move(result));
          finish();
        } else if (buttonIndex == HalGPIO::BTN_CONFIRM) {
          selectCurrentItem();
        } else if (buttonIndex == HalGPIO::BTN_LEFT) {
          selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(menuItems.size()));
          requestUpdate();
        } else if (buttonIndex == HalGPIO::BTN_RIGHT) {
          selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(menuItems.size()));
          requestUpdate();
        }
      }
#endif
      mappedInput.suppressTouchButtonFallback();
      return;
    }
  }

  // Handle navigation
  buttonNavigator.onNext([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(menuItems.size()));
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(menuItems.size()));
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    selectCurrentItem();
    return;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    result.data = MenuResult{-1, pendingOrientation, selectedPageTurnOption, pageTurnOptionChanged};
    setResult(std::move(result));
    finish();
    return;
  }
}

void EpubReaderMenuActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto orientation = renderer.getOrientation();
  // Landscape orientation: button hints are drawn along a vertical edge, so we
  // reserve a horizontal gutter to prevent overlap with menu content.
  const bool isLandscapeCw = orientation == GfxRenderer::Orientation::LandscapeClockwise;
  const bool isLandscapeCcw = orientation == GfxRenderer::Orientation::LandscapeCounterClockwise;
  // Inverted portrait: button hints appear near the logical top, so we reserve
  // vertical space to keep the header and list clear.
  const bool isPortraitInverted = orientation == GfxRenderer::Orientation::PortraitInverted;
  const int hintGutterWidth = (isLandscapeCw || isLandscapeCcw) ? 30 : 0;
  // Landscape CW places hints on the left edge; CCW keeps them on the right.
  const int contentX = isLandscapeCw ? hintGutterWidth : 0;
  const int contentWidth = pageWidth - hintGutterWidth;
  const int hintGutterHeight = isPortraitInverted ? 50 : 0;
  const int contentY = hintGutterHeight;

  // Title
  const std::string truncTitle =
      renderer.truncatedText(UI_12_FONT_ID, title.c_str(), contentWidth - 40, EpdFontFamily::BOLD);
  // Manual centering so we can respect the content gutter.
  const int titleX =
      contentX + (contentWidth - renderer.getTextWidth(UI_12_FONT_ID, truncTitle.c_str(), EpdFontFamily::BOLD)) / 2;
  renderer.drawText(UI_12_FONT_ID, titleX, 15 + contentY, truncTitle.c_str(), true, EpdFontFamily::BOLD);

  // Progress summary
  std::string progressLine;
  if (totalPages > 0) {
    progressLine = std::string(tr(STR_CHAPTER_PREFIX)) + std::to_string(currentPage) + "/" +
                   std::to_string(totalPages) + std::string(tr(STR_PAGES_SEPARATOR));
  }
  progressLine += std::string(tr(STR_BOOK_PREFIX)) + std::to_string(bookProgressPercent) + "%";
  renderer.drawCenteredText(UI_10_FONT_ID, 45, progressLine.c_str());

  // Menu Items
  const int startY = kMenuStartY + contentY;
  const int visibleItems = menuVisibleItemCount(renderer);
  const int pageStartIndex = (selectedIndex / visibleItems) * visibleItems;
  const int pageEndIndex = std::min(static_cast<int>(menuItems.size()), pageStartIndex + visibleItems);

  for (int i = pageStartIndex; i < pageEndIndex; ++i) {
    const int displayY = startY + ((i - pageStartIndex) * kMenuLineHeight);
    const bool isSelected = (i == selectedIndex);

    if (isSelected) {
      // Highlight only the content area so we don't paint over hint gutters.
      renderer.fillRect(contentX, displayY, contentWidth - 1, kMenuLineHeight, true);
    }

    StrId labelId = menuItems[i].labelId;
    if (menuItems[i].action == MenuAction::TOGGLE_BOOKMARK && currentPageBookmarked) {
      labelId = StrId::STR_REMOVE_BOOKMARK;
    }
    if (menuItems[i].action == MenuAction::TOGGLE_HIGHLIGHT && currentPageHighlighted) {
      labelId = StrId::STR_REMOVE_HIGHLIGHT;
    }
    renderer.drawText(UI_10_FONT_ID, contentX + 20, displayY, I18N.get(labelId), !isSelected);

    if (menuItems[i].action == MenuAction::ROTATE_SCREEN) {
      // Render current orientation value on the right edge of the content area.
      const char* value = I18N.get(orientationLabels[pendingOrientation]);
      const auto width = renderer.getTextWidth(UI_10_FONT_ID, value);
      renderer.drawText(UI_10_FONT_ID, contentX + contentWidth - 20 - width, displayY, value, !isSelected);
    }

    if (menuItems[i].action == MenuAction::AUTO_PAGE_TURN) {
      // Render current page turn value on the right edge of the content area.
      const auto value = pageTurnLabels[selectedPageTurnOption];
      const auto width = renderer.getTextWidth(UI_10_FONT_ID, value);
      renderer.drawText(UI_10_FONT_ID, contentX + contentWidth - 20 - width, displayY, value, !isSelected);
    }

    if (menuItems[i].action == MenuAction::FONT_SIZE_DOWN || menuItems[i].action == MenuAction::FONT_SIZE_UP) {
      const char* value = currentFontSize < fontSizeLabels.size() ? I18N.get(fontSizeLabels[currentFontSize])
                                                                  : I18N.get(StrId::STR_MEDIUM);
      const auto width = renderer.getTextWidth(UI_10_FONT_ID, value);
      renderer.drawText(UI_10_FONT_ID, contentX + contentWidth - 20 - width, displayY, value, !isSelected);
    }

    if (menuItems[i].action == MenuAction::TOUCH_LOCK) {
      const char* value = touchLockEnabled ? tr(STR_STATE_ON) : tr(STR_STATE_OFF);
      const auto width = renderer.getTextWidth(UI_10_FONT_ID, value);
      renderer.drawText(UI_10_FONT_ID, contentX + contentWidth - 20 - width, displayY, value, !isSelected);
    }
  }

  // Footer / Hints
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
