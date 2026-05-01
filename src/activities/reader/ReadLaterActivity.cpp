#include "ReadLaterActivity.h"

#include <I18n.h>

#include "ReadLaterStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

std::string ReadLaterActivity::sizeLabel(const size_t bytes) const {
  if (bytes < 1024) {
    return std::to_string(bytes) + " B";
  }
  if (bytes < 1024 * 1024) {
    return std::to_string(bytes / 1024) + " KB";
  }
  return std::to_string(bytes / (1024 * 1024)) + " MB";
}

void ReadLaterActivity::onEnter() {
  Activity::onEnter();
  READ_LATER_STORE.refresh();
  selectedIndex = 0;
  requestUpdate();
}

void ReadLaterActivity::openCurrentSelectionOrRefresh() {
  if (!READ_LATER_STORE.getItems().empty()) {
    activityManager.goToReader(READ_LATER_STORE.getItems()[selectedIndex].path);
  } else {
    READ_LATER_STORE.refresh();
    selectedIndex = 0;
    requestUpdate();
  }
}

void ReadLaterActivity::loop() {
  const int itemCount = static_cast<int>(READ_LATER_STORE.getItems().size());
  const auto& metrics = UITheme::getInstance().getMetrics();

  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (itemCount <= 0) {
      if (touchEvent.isTap()) {
        mappedInput.suppressTouchButtonFallback();
        openCurrentSelectionOrRefresh();
        return;
      }
    }

    if (touchEvent.isTap()) {
      const Rect listRect{
          0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, renderer.getScreenWidth(),
          renderer.getScreenHeight() -
              (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)};
      const int clickedIndex = TouchHitTest::listItemAt(listRect, metrics.listWithSubtitleRowHeight, selectedIndex,
                                                        itemCount, touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        openCurrentSelectionOrRefresh();
        return;
      }
    } else {
      const auto gestureAction = TouchHitTest::listGestureActionForTouch(touchEvent);
      if (gestureAction == TouchHitTest::ListGestureAction::NextItem) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount);
        requestUpdate();
        return;
      }
      if (gestureAction == TouchHitTest::ListGestureAction::PreviousItem) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount);
        requestUpdate();
        return;
      }
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (itemCount > 0) {
    buttonNavigator.onNextRelease([this, itemCount] {
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount);
      requestUpdate();
    });

    buttonNavigator.onPreviousRelease([this, itemCount] {
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount);
      requestUpdate();
    });
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    openCurrentSelectionOrRefresh();
  }
}

void ReadLaterActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& items = READ_LATER_STORE.getItems();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_READ_LATER));

  if (items.empty()) {
    const int centerY = pageHeight / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - 42, tr(STR_READ_LATER_EMPTY_TITLE));
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - 8, tr(STR_READ_LATER_EMPTY_HINT));
    renderer.drawCenteredText(UI_10_FONT_ID, centerY + 26, tr(STR_READ_LATER_TARGET_PATH));
    renderer.drawCenteredText(SMALL_FONT_ID, centerY + 58, tr(STR_READ_LATER_REFRESH_HINT));
  } else {
    GUI.drawList(
        renderer,
        Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
             pageHeight -
                 (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)},
        static_cast<int>(items.size()), selectedIndex, [&items](int index) { return items[index].filename; },
        [this, &items](int index) { return std::string(tr(STR_SIZE_LABEL)) + ": " + sizeLabel(items[index].bytes); });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), items.empty() ? tr(STR_REFRESH) : tr(STR_OPEN),
                                            tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
