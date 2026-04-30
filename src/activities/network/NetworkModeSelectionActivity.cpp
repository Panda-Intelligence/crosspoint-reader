#include "NetworkModeSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int MENU_ITEM_COUNT = 3;
}  // namespace

void NetworkModeSelectionActivity::onEnter() {
  Activity::onEnter();

  // Reset selection
  selectedIndex = 0;

  // Trigger first update
  requestUpdate();
}

void NetworkModeSelectionActivity::onExit() { Activity::onExit(); }

void NetworkModeSelectionActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (touchEvent.isTap()) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
      const int contentHeight =
          renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
      const Rect listRect{0, contentTop, renderer.getScreenWidth(), contentHeight};
      const int clickedIndex = TouchHitTest::listItemAt(listRect, metrics.listWithSubtitleRowHeight, selectedIndex,
                                                        MENU_ITEM_COUNT, touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        onModeSelected(clickedIndex == 1
                           ? NetworkMode::CONNECT_CALIBRE
                           : (clickedIndex == 2 ? NetworkMode::CREATE_HOTSPOT : NetworkMode::JOIN_NETWORK));
        return;
      }
    } else {
      const auto gestureAction = TouchHitTest::listGestureActionForTouch(touchEvent);
      if (gestureAction == TouchHitTest::ListGestureAction::NextItem) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, MENU_ITEM_COUNT);
        requestUpdate();
        return;
      }
      if (gestureAction == TouchHitTest::ListGestureAction::PreviousItem) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, MENU_ITEM_COUNT);
        requestUpdate();
        return;
      }
    }
  }

  // Handle back button - cancel
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onCancel();
    return;
  }

  // Handle confirm button - select current option
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    NetworkMode mode = NetworkMode::JOIN_NETWORK;
    if (selectedIndex == 1) {
      mode = NetworkMode::CONNECT_CALIBRE;
    } else if (selectedIndex == 2) {
      mode = NetworkMode::CREATE_HOTSPOT;
    }
    onModeSelected(mode);
    return;
  }

  // Handle navigation
  buttonNavigator.onNext([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, MENU_ITEM_COUNT);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, MENU_ITEM_COUNT);
    requestUpdate();
  });
}

void NetworkModeSelectionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FILE_TRANSFER));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  // Menu items and descriptions
  static constexpr StrId menuItems[MENU_ITEM_COUNT] = {StrId::STR_JOIN_NETWORK, StrId::STR_CALIBRE_WIRELESS,
                                                       StrId::STR_CREATE_HOTSPOT};
  static constexpr StrId menuDescs[MENU_ITEM_COUNT] = {StrId::STR_JOIN_DESC, StrId::STR_CALIBRE_DESC,
                                                       StrId::STR_HOTSPOT_DESC};
  static constexpr UIIcon menuIcons[MENU_ITEM_COUNT] = {UIIcon::Wifi, UIIcon::Library, UIIcon::Hotspot};

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(MENU_ITEM_COUNT), selectedIndex,
      [](int index) { return std::string(I18N.get(menuItems[index])); },
      [](int index) { return std::string(I18N.get(menuDescs[index])); }, [](int index) { return menuIcons[index]; });

  // Draw help text at bottom
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void NetworkModeSelectionActivity::onModeSelected(NetworkMode mode) {
  setResult(NetworkModeResult{mode});
  finish();
}

void NetworkModeSelectionActivity::onCancel() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}
