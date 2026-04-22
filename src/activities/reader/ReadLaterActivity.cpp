#include "ReadLaterActivity.h"

#include <I18n.h>

#include "ReadLaterStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

void ReadLaterActivity::onEnter() {
  Activity::onEnter();
  READ_LATER_STORE.refresh();
  selectedIndex = 0;
  requestUpdate();
}

void ReadLaterActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const int itemCount = static_cast<int>(READ_LATER_STORE.getItems().size());

  buttonNavigator.onNextRelease([this, itemCount] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, itemCount] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount);
    requestUpdate();
  });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (!READ_LATER_STORE.getItems().empty()) {
      activityManager.goToReader(READ_LATER_STORE.getItems()[selectedIndex].path);
    }
  }
}

void ReadLaterActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& items = READ_LATER_STORE.getItems();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Read Later");

  if (items.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, "No local articles");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 20, "Import flow comes next");
  } else {
    GUI.drawList(
        renderer,
        Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
             pageHeight - (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight +
                           metrics.verticalSpacing * 2)},
        static_cast<int>(items.size()), selectedIndex, [&items](int index) { return items[index].filename; },
        [](int) { return std::string("Open in reader"); });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Open", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
