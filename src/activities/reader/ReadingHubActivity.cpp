#include "ReadingHubActivity.h"

#include <I18n.h>

#include "DictionaryActivity.h"
#include "ReadLaterActivity.h"
#include "components/UITheme.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kItemCount = 4;

const char* itemLabel(const int index) {
  switch (index) {
    case 0:
      return tr(STR_CONTINUE_READING);
    case 1:
      return tr(STR_READER_LIBRARY);
    case 2:
      return tr(STR_DICTIONARY);
    case 3:
    default:
      return tr(STR_READ_LATER);
  }
}

const char* itemSubtitle(const int index) {
  switch (index) {
    case 0:
      return tr(STR_READING_SUBTITLE_CONTINUE);
    case 1:
      return tr(STR_READING_SUBTITLE_LIBRARY);
    case 2:
      return tr(STR_READING_SUBTITLE_DICTIONARY);
    case 3:
    default:
      return tr(STR_READING_SUBTITLE_READ_LATER);
  }
}
}  // namespace

void ReadingHubActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void ReadingHubActivity::openCurrentSelection() {
  switch (selectedIndex) {
    case 0:
      activityManager.goToRecentBooks();
      break;
    case 1:
      activityManager.goToFileBrowser();
      break;
    case 2:
      activityManager.replaceActivity(std::make_unique<DictionaryActivity>(renderer, mappedInput));
      break;
    case 3:
      activityManager.replaceActivity(std::make_unique<ReadLaterActivity>(renderer, mappedInput));
      break;
    default:
      requestUpdate();
      break;
  }
}

void ReadingHubActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (touchEvent.isTap()) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const Rect listRect{
          0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, renderer.getScreenWidth(),
          renderer.getScreenHeight() -
              (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)};
      const int clickedIndex = TouchHitTest::listItemAt(listRect, metrics.listWithSubtitleRowHeight, selectedIndex,
                                                        kItemCount, touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        openCurrentSelection();
        return;
      }
    } else {
      const auto gestureAction = TouchHitTest::listGestureActionForTouch(touchEvent);
      if (gestureAction == TouchHitTest::ListGestureAction::NextItem) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, kItemCount);
        requestUpdate();
        return;
      }
      if (gestureAction == TouchHitTest::ListGestureAction::PreviousItem) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, kItemCount);
        requestUpdate();
        return;
      }
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, kItemCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, kItemCount);
    requestUpdate();
  });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    openCurrentSelection();
  }
}

void ReadingHubActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_READING));
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
           pageHeight -
               (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)},
      kItemCount, selectedIndex, [](int index) { return std::string(itemLabel(index)); },
      [](int index) { return std::string(itemSubtitle(index)); });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
