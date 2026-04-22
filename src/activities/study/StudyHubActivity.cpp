#include "StudyHubActivity.h"

#include <I18n.h>

#include "LearningReportActivity.h"
#include "StudyCardsTodayActivity.h"
#include "StudyStateStore.h"
#include "components/UITheme.h"

namespace {
constexpr int kItemCount = 3;

const char* itemLabel(int index) {
  switch (index) {
    case 0:
      return "Study Cards";
    case 1:
      return "Learning Report";
    case 2:
    default:
      return "Deck Import Status";
  }
}
}  // namespace

void StudyHubActivity::onEnter() {
  Activity::onEnter();
  STUDY_STATE.loadFromFile();
  selectedIndex = 0;
  requestUpdate();
}

void StudyHubActivity::loop() {
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
    switch (selectedIndex) {
      case 0:
        activityManager.replaceActivity(std::make_unique<StudyCardsTodayActivity>(renderer, mappedInput));
        break;
      case 1:
        activityManager.replaceActivity(std::make_unique<LearningReportActivity>(renderer, mappedInput));
        break;
      case 2:
      default:
        requestUpdate();
        break;
    }
  }
}

void StudyHubActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& state = STUDY_STATE.getState();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Study");
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
           pageHeight - (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight +
                         metrics.verticalSpacing * 2)},
      kItemCount, selectedIndex, [](int index) { return std::string(itemLabel(index)); },
      [&state](int index) {
        switch (index) {
          case 0:
            return std::string("Review due cards: ") + std::to_string(state.dueToday - state.completedToday);
          case 1:
            return std::string("Progress and mastery");
          case 2:
          default:
            return std::string("Sync decks from companion");
        }
      });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Open", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
