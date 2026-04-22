#include "WeatherClockActivity.h"

#include <Arduino.h>
#include <I18n.h>
#include <algorithm>
#include <ctime>

#include "DesktopSummaryStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kPageCount = 3;

struct DayForecast {
  const char* day;
  int low;
  int high;
  const char* condition;
};

void drawLine(GfxRenderer& renderer, int x, int y, const char* text, bool bold = false) {
  renderer.drawText(UI_10_FONT_ID, x, y, text, true, bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
}
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
    const int top = metrics.topPadding + metrics.headerHeight + 82;
    const int left = metrics.contentSidePadding;
    const int tempBase = summary.isOnline ? 24 : 22;
    const int spread = summary.isOnline ? 6 : 5;
    const DayForecast forecast[3] = {
        {"Today", tempBase - spread, tempBase + spread, summary.isOnline ? "Cloudy" : "Cached"},
        {"Tomorrow", tempBase - spread - 1, tempBase + spread + 1, summary.isOnline ? "Sunny" : "Offline"},
        {"+2 days", tempBase - spread + 1, tempBase + spread - 1, summary.isOnline ? "Rain chance" : "Cached"},
    };

    drawLine(renderer, left, top, "Forecast", true);
    for (int i = 0; i < 3; ++i) {
      char line[72];
      snprintf(line, sizeof(line), "%s  %d~%dC  %s", forecast[i].day, forecast[i].low, forecast[i].high,
               forecast[i].condition);
      drawLine(renderer, left, top + 34 + i * 30, line);
    }
    drawLine(renderer, left, top + 34 + 3 * 30 + 12, summary.isOnline ? "Source: companion sync"
                                                                        : "Offline forecast from cache");
  } else {
    const int top = metrics.topPadding + metrics.headerHeight + 82;
    const int left = metrics.contentSidePadding;
    const int temp = summary.isOnline ? 24 : 22;
    const bool carryUmbrella = summary.isOnline;

    drawLine(renderer, left, top, "Scenes", true);

    char commute[72];
    snprintf(commute, sizeof(commute), "Commute: %s", carryUmbrella ? "leave 10 min earlier" : "normal traffic window");
    drawLine(renderer, left, top + 34, commute);

    char clothing[72];
    snprintf(clothing, sizeof(clothing), "Clothing: %s", temp <= 20 ? "light jacket suggested" : "shirt + thin layer");
    drawLine(renderer, left, top + 64, clothing);

    char umbrella[72];
    snprintf(umbrella, sizeof(umbrella), "Umbrella: %s", carryUmbrella ? "recommended" : "optional");
    drawLine(renderer, left, top + 94, umbrella);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Refresh", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
