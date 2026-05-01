#include "DashboardCustomizeActivity.h"

#include <I18n.h>

#include "DashboardShortcutStore.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "util/ButtonNavigator.h"
#include "util/TouchHitTest.h"

void DashboardCustomizeActivity::onEnter() {
  Activity::onEnter();
  DASHBOARD_SHORTCUTS.loadFromFile();
  selectedIndex = 0;
  moving = false;
  requestUpdate();
}

void DashboardCustomizeActivity::onExit() { Activity::onExit(); }

void DashboardCustomizeActivity::selectRelative(const int delta) {
  if (delta > 0) {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(DashboardShortcutStore::SLOT_COUNT));
  } else if (delta < 0) {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(DashboardShortcutStore::SLOT_COUNT));
  }
  requestUpdate();
}

void DashboardCustomizeActivity::moveSelected(const int delta) {
  const int targetIndex = selectedIndex + delta;
  if (targetIndex < 0 || targetIndex >= static_cast<int>(DashboardShortcutStore::SLOT_COUNT)) {
    return;
  }
  if (DASHBOARD_SHORTCUTS.moveSlot(static_cast<size_t>(selectedIndex), static_cast<size_t>(targetIndex))) {
    selectedIndex = targetIndex;
    requestUpdate();
  }
}

void DashboardCustomizeActivity::cycleSelected(const int direction) {
  if (DASHBOARD_SHORTCUTS.cycleSlot(static_cast<size_t>(selectedIndex), direction)) {
    requestUpdate();
  }
}

void DashboardCustomizeActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.verticalSpacing;
      const int contentHeight =
          renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
      const Rect listRect{0, contentTop, renderer.getScreenWidth(), contentHeight};
      const int clickedIndex =
          TouchHitTest::listItemAt(listRect, metrics.listRowHeight, selectedIndex,
                                   static_cast<int>(DashboardShortcutStore::SLOT_COUNT), touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        if (clickedIndex == selectedIndex) {
          cycleSelected(1);
          return;
        }
        selectedIndex = clickedIndex;
        requestUpdate();
        return;
      }
      mappedInput.suppressTouchButtonFallback();
      return;
    } else if (!buttonHintTap && TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      if (moving) {
        moveSelected(1);
        return;
      }
      selectRelative(1);
      return;
    } else if (!buttonHintTap && TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      if (moving) {
        moveSelected(-1);
        return;
      }
      selectRelative(-1);
      return;
    } else if (!buttonHintTap) {
      mappedInput.suppressTouchButtonFallback();
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (moving) {
      moving = false;
      requestUpdate();
      return;
    }
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    if (moving) {
      moveSelected(1);
      return;
    }
    selectRelative(1);
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    if (moving) {
      moveSelected(-1);
      return;
    }
    selectRelative(-1);
    return;
  }

  if (!moving && mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    cycleSelected(1);
    return;
  }

  if (!moving && mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    cycleSelected(-1);
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    moving = !moving;
    requestUpdate();
    return;
  }
}

void DashboardCustomizeActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_DASHBOARD_CUSTOMIZE_TITLE));
  GUI.drawSubHeader(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight},
                    tr(STR_DASHBOARD_CUSTOMIZE_HINT));

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(DashboardShortcutStore::SLOT_COUNT),
      selectedIndex,
      [](int index) {
        const auto id = DASHBOARD_SHORTCUTS.getShortcut(static_cast<size_t>(index));
        const auto* definition = DashboardShortcutStore::definitionFor(id);
        return definition != nullptr ? std::string(I18N.get(definition->labelId)) : std::string();
      },
      nullptr, nullptr, [](int index) { return std::to_string(index + 1); }, true);

  const auto labels = moving ? mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_DONE), tr(STR_DIR_UP), tr(STR_DIR_DOWN))
                             : mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
