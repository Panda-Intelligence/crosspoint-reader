#include "ArcadeHubActivity.h"

#include <I18n.h>

#include "components/UITheme.h"

namespace {
constexpr int kItemCount = 7;

const char* itemLabel(int index) {
  switch (index) {
    case 0:
      return "2048";
    case 1:
      return "Sudoku";
    case 2:
      return "Sokoban";
    case 3:
      return "Memory";
    case 4:
      return "Word Puzzle";
    case 5:
      return "Daily Maze";
    case 6:
    default:
      return "Challenges";
  }
}
}  // namespace

void ArcadeHubActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void ArcadeHubActivity::loop() {
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
    requestUpdate();
  }
}

void ArcadeHubActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Arcade");
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
           pageHeight - (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight +
                         metrics.verticalSpacing * 2)},
      kItemCount, selectedIndex, [](int index) { return std::string(itemLabel(index)); },
      [](int index) {
        if (index == 6) {
          return std::string("Daily challenge and achievements");
        }
        return std::string("Low-refresh gameplay");
      });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Open", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
