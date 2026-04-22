#include "WeatherClockActivity.h"

#include <Arduino.h>
#include <I18n.h>
#include <ctime>

#include "DesktopSummaryStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kPageCount = 3;
}

void WeatherClockActivity::onEnter() {
  Activity::onEnter();
  pageIndex = 0;
  DESKTOP_SUMMARY.refresh();
  requestUpdate();
}

void WeatherClockActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    pageIndex = ButtonNavigator::nextIndex(pageIndex, kPageCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    pageIndex = ButtonNavigator::previousIndex(pageIndex, kPageCount);
    requestUpdate();
  });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    DESKTOP_SUMMARY.refresh();
    requestUpdate();
  }
}

void WeatherClockActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& summary = DESKTOP_SUMMARY.getState();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Weather & Clock");

  time_t now = time(nullptr);
  struct tm localTm = {};
  localtime_r(&now, &localTm);

  char timeBuffer[16];
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", localTm.tm_hour, localTm.tm_min);
  char dateBuffer[32];
  snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d", localTm.tm_year + 1900, localTm.tm_mon + 1,
           localTm.tm_mday);

  renderer.drawCenteredText(UI_12_FONT_ID, metrics.topPadding + metrics.headerHeight + 10, dateBuffer, true,
                            EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, metrics.topPadding + metrics.headerHeight + 55, summary.city.c_str());

  if (pageIndex == 0) {
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 40, timeBuffer, true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, summary.weatherLine.c_str());
  } else if (pageIndex == 1) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 30, "Forecast");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, "24h and 7d forecast placeholder");
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 30, "Scenes");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, "Commute, clothing, umbrella");
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Refresh", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
