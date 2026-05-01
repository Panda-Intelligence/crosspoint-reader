#include "DashboardActivity.h"

#include <I18n.h>

#include <cstdio>

#include "DashboardShortcutStore.h"
#include "DesktopSummaryStore.h"
#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kCustomizeRowCount = 1;

bool isShortcutRow(int index) { return index >= 0 && index < static_cast<int>(DashboardShortcutStore::SLOT_COUNT); }

std::string formatDashboardValue(const StrId id, const int value) {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), I18N.get(id), value);
  return buffer;
}

std::string formatDashboardValue(const StrId id, const int firstValue, const int secondValue) {
  char buffer[80];
  snprintf(buffer, sizeof(buffer), I18N.get(id), firstValue, secondValue);
  return buffer;
}
}  // namespace

void DashboardActivity::onEnter() {
  Activity::onEnter();
  DASHBOARD_SHORTCUTS.loadFromFile();
  selectedIndex = 0;
  STUDY_STATE.loadFromFile();
  DESKTOP_SUMMARY.refresh();
  requestUpdate();
}

int DashboardActivity::itemCount() const {
  return static_cast<int>(DashboardShortcutStore::SLOT_COUNT) + kCustomizeRowCount;
}

std::string DashboardActivity::subtitleForShortcut(const DashboardShortcutId id) const {
  const auto& summary = DESKTOP_SUMMARY.getState();
  switch (id) {
    case DashboardShortcutId::WeatherClock:
      return summary.weatherLine;
    case DashboardShortcutId::Today:
      return summary.todaySecondary;
    case DashboardShortcutId::StudyToday:
      if (summary.loadedCards <= 0) {
        return tr(STR_DASHBOARD_STUDY_IMPORT_HINT);
      }
      if (summary.againCards > 0) {
        return formatDashboardValue(StrId::STR_DASHBOARD_STUDY_AGAIN_FORMAT, summary.againCards);
      }
      return summary.dueCards > 0 ? formatDashboardValue(StrId::STR_DASHBOARD_STUDY_DUE_FORMAT, summary.dueCards)
                                  : std::string(tr(STR_DASHBOARD_STUDY_CAUGHT_UP));
    case DashboardShortcutId::StudyHub:
      return formatDashboardValue(StrId::STR_DASHBOARD_STUDY_SUMMARY_FORMAT, summary.loadedCards, summary.laterCards);
    default: {
      const auto* definition = DashboardShortcutStore::definitionFor(id);
      return definition != nullptr ? std::string(I18N.get(definition->subtitleId)) : std::string();
    }
  }
}

void DashboardActivity::openCurrentSelection() {
  if (!isShortcutRow(selectedIndex)) {
    activityManager.goToDashboardCustomize();
    return;
  }

  switch (DASHBOARD_SHORTCUTS.getShortcut(static_cast<size_t>(selectedIndex))) {
    case DashboardShortcutId::WeatherClock:
      activityManager.goToWeatherClock();
      break;
    case DashboardShortcutId::Today:
      activityManager.goToCalendar();
      break;
    case DashboardShortcutId::DesktopHub:
      activityManager.goToDesktopHub();
      break;
    case DashboardShortcutId::StudyHub:
    case DashboardShortcutId::StudyToday:
      activityManager.goToStudyHub();
      break;
    case DashboardShortcutId::ReadingHub:
      activityManager.goToReadingHub();
      break;
    case DashboardShortcutId::ArcadeHub:
      activityManager.goToArcadeHub();
      break;
    case DashboardShortcutId::ImportSync:
      activityManager.goToFileTransfer();
      break;
    case DashboardShortcutId::FileBrowser:
      activityManager.goToFileBrowser();
      break;
    case DashboardShortcutId::Settings:
      activityManager.goToSettings();
      break;
    case DashboardShortcutId::RecentReading:
      activityManager.goToRecentBooks();
      break;
    case DashboardShortcutId::Count:
      requestUpdate();
      return;
  }
}

void DashboardActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (touchEvent.isTap()) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const Rect listRect{
          0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, renderer.getScreenWidth(),
          renderer.getScreenHeight() -
              (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)};
      const int clickedIndex = TouchHitTest::listItemAt(listRect, metrics.listWithSubtitleRowHeight, selectedIndex,
                                                        itemCount(), touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        openCurrentSelection();
        return;
      } else if (!mappedInput.isTouchButtonHintTap(touchEvent)) {
        mappedInput.suppressTouchButtonFallback();
        return;
      }
    } else {
      const auto gestureAction = TouchHitTest::listGestureActionForTouch(touchEvent);
      if (gestureAction == TouchHitTest::ListGestureAction::NextItem) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount());
        requestUpdate();
        return;
      }
      if (gestureAction == TouchHitTest::ListGestureAction::PreviousItem) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount());
        requestUpdate();
        return;
      }
      if (!mappedInput.isTouchButtonHintTap(touchEvent)) {
        mappedInput.suppressTouchButtonFallback();
        return;
      }
    }
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount());
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount());
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

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_MOFEI_DASHBOARD_TITLE));
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
           pageHeight -
               (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)},
      itemCount(), selectedIndex,
      [](int index) {
        if (!isShortcutRow(index)) {
          return std::string(tr(STR_DASHBOARD_CUSTOMIZE));
        }
        const auto id = DASHBOARD_SHORTCUTS.getShortcut(static_cast<size_t>(index));
        const auto* definition = DashboardShortcutStore::definitionFor(id);
        return definition != nullptr ? std::string(I18N.get(definition->labelId)) : std::string();
      },
      [this](int index) {
        if (!isShortcutRow(index)) {
          return std::string(tr(STR_DASHBOARD_CUSTOMIZE_SUBTITLE));
        }
        return subtitleForShortcut(DASHBOARD_SHORTCUTS.getShortcut(static_cast<size_t>(index)));
      });

  const auto labels = mappedInput.mapLabels("", tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
