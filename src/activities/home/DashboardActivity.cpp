#include "DashboardActivity.h"

#include <I18n.h>

#include "DesktopSummaryStore.h"
#include "StudyStateStore.h"
#include "activities/desktop/CalendarActivity.h"
#include "activities/desktop/WeatherClockActivity.h"
#include "activities/arcade/ArcadeHubActivity.h"
#include "activities/desktop/DesktopHubActivity.h"
#include "activities/reader/ReadingHubActivity.h"
#include "activities/study/StudyHubActivity.h"
#include "components/UITheme.h"

namespace {
constexpr int kItemCount = 11;

const char* itemLabel(int index) {
  switch (index) {
    case 0:
      return "Weather & Clock";
    case 1:
      return "Today";
    case 2:
      return "Study Today";
    case 3:
      return "Continue Reading";
    case 4:
      return "Desktop";
    case 5:
      return "Study";
    case 6:
      return "Reading";
    case 7:
      return "Arcade";
    case 8:
      return "Import / Sync";
    case 9:
      return "Settings";
    case 10:
    default:
      return "Network Status";
  }
}

const char* itemSubtitle(int index) {
  switch (index) {
    case 0:
      return "Time, weather and AQI";
    case 1:
      return "Events and countdowns";
    case 2:
      return "Due cards and progress";
    case 3:
      return "Resume the latest title";
    case 4:
      return "Weather, calendar, focus";
    case 5:
      return "Cards, reports, deck sync";
    case 6:
      return "Reader, dictionary, read later";
    case 7:
      return "Games and daily challenge";
    case 8:
      return "Upload books, cards and articles";
    case 9:
      return "System and package settings";
    case 10:
    default:
      return "Connection and account state";
  }
}
}  // namespace

void DashboardActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  DESKTOP_SUMMARY.refresh();
  STUDY_STATE.loadFromFile();
  requestUpdate();
}

void DashboardActivity::openCurrentSelection() {
  switch (selectedIndex) {
    case 0:
      activityManager.replaceActivity(std::make_unique<WeatherClockActivity>(renderer, mappedInput));
      break;
    case 1:
      activityManager.replaceActivity(std::make_unique<CalendarActivity>(renderer, mappedInput));
      break;
    case 4:
      activityManager.replaceActivity(std::make_unique<DesktopHubActivity>(renderer, mappedInput));
      break;
    case 5:
      activityManager.replaceActivity(std::make_unique<StudyHubActivity>(renderer, mappedInput));
      break;
    case 6:
      activityManager.replaceActivity(std::make_unique<ReadingHubActivity>(renderer, mappedInput));
      break;
    case 7:
      activityManager.replaceActivity(std::make_unique<ArcadeHubActivity>(renderer, mappedInput));
      break;
    case 8:
      activityManager.goToFileTransfer();
      break;
    case 9:
      activityManager.goToSettings();
      break;
    case 3:
      activityManager.goToRecentBooks();
      break;
    default:
      if (selectedIndex == 2) {
        activityManager.goToStudyHub();
        return;
      }
      requestUpdate();
      break;
  }
}

void DashboardActivity::loop() {
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

void DashboardActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& summary = DESKTOP_SUMMARY.getState();
  const auto& study = STUDY_STATE.getState();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Mofei Dashboard");
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
           pageHeight - (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight +
                         metrics.verticalSpacing * 2)},
      kItemCount, selectedIndex, [](int index) { return std::string(itemLabel(index)); },
      [&summary, &study](int index) {
        switch (index) {
          case 0:
            return summary.weatherLine;
          case 1:
            return summary.todaySecondary;
          case 2:
            if (summary.loadedCards <= 0) {
              return std::string("Import study decks to begin");
            }
            if (summary.againCards > 0) {
              return "Again queue: " + std::to_string(summary.againCards);
            }
            return summary.dueCards > 0 ? ("Due cards: " + std::to_string(summary.dueCards)) : std::string("All caught up");
          case 5:
            return "Cards " + std::to_string(summary.loadedCards) + "  Later " + std::to_string(summary.laterCards);
          default:
            return std::string(itemSubtitle(index));
        }
      });

  const auto labels = mappedInput.mapLabels("", "Open", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
