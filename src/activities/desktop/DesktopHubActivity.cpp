#include "DesktopHubActivity.h"

#include <I18n.h>

#include "CalendarActivity.h"
#include "ClockFocusActivity.h"
#include "WeatherClockActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kItemCount = 3;

const char* itemLabel(int index) {
  switch (index) {
    case 0:
      return "Weather & Clock";
    case 1:
      return "Calendar";
    case 2:
    default:
      return "Clock Focus";
  }
}
}  // namespace

void DesktopHubActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void DesktopHubActivity::loop() {
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
        activityManager.replaceActivity(std::make_unique<WeatherClockActivity>(renderer, mappedInput));
        break;
      case 1:
        activityManager.replaceActivity(std::make_unique<CalendarActivity>(renderer, mappedInput));
        break;
      case 2:
        activityManager.replaceActivity(std::make_unique<ClockFocusActivity>(renderer, mappedInput));
        break;
      default:
        requestUpdate();
        break;
    }
  }
}

void DesktopHubActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Desktop");
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
           pageHeight - (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight +
                         metrics.verticalSpacing * 2)},
      kItemCount, selectedIndex, [](int index) { return std::string(itemLabel(index)); },
      [](int index) {
        switch (index) {
          case 0:
            return std::string("Time, weather, AQI");
          case 1:
            return std::string("Today, week, month");
          case 2:
          default:
            return std::string("Pomodoro and countdowns");
        }
      });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Open", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
