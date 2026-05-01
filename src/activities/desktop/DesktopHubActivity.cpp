#include "DesktopHubActivity.h"

#include <I18n.h>

#include "CalendarActivity.h"
#include "ClockFocusActivity.h"
#include "StickyNotesActivity.h"
#include "WeatherClockActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kItemCount = 4;

StrId itemLabel(int index) {
  switch (index) {
    case 0:
      return StrId::STR_DASHBOARD_WEATHER;
    case 1:
      return StrId::STR_DESKTOP_CALENDAR;
    case 2:
      return StrId::STR_DESKTOP_CLOCK_FOCUS;
    case 3:
    default:
      return StrId::STR_NOTES_TITLE;
  }
}

StrId itemSubtitle(int index) {
  switch (index) {
    case 0:
      return StrId::STR_DESKTOP_WEATHER_SUBTITLE;
    case 1:
      return StrId::STR_DESKTOP_CALENDAR_SUBTITLE;
    case 2:
      return StrId::STR_DESKTOP_CLOCK_FOCUS_SUBTITLE;
    case 3:
    default:
      return StrId::STR_DESKTOP_NOTES_SUBTITLE;
  }
}
}  // namespace

void DesktopHubActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void DesktopHubActivity::openCurrentSelection() {
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
    case 3:
      activityManager.replaceActivity(std::make_unique<StickyNotesActivity>(renderer, mappedInput));
      break;
    default:
      requestUpdate();
      break;
  }
}

void DesktopHubActivity::loop() {
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

void DesktopHubActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DASHBOARD_DESKTOP));
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
           pageHeight -
               (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)},
      kItemCount, selectedIndex, [](int index) { return std::string(I18N.get(itemLabel(index))); },
      [](int index) { return std::string(I18N.get(itemSubtitle(index))); });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
