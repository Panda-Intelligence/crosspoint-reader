#include "CalendarActivity.h"

#include <Arduino.h>
#include <I18n.h>
#include <algorithm>
#include <ctime>

#include "DesktopSummaryStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kPageCount = 3;

const char* weekdayLabel(int weekday) {
  static const char* kLabels[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return kLabels[std::clamp(weekday, 0, 6)];
}

void drawLine(GfxRenderer& renderer, int x, int y, const char* text, bool bold = false) {
  renderer.drawText(UI_10_FONT_ID, x, y, text, true, bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
}
}

void CalendarActivity::onEnter() {
  Activity::onEnter();
  pageIndex = 0;
  DESKTOP_SUMMARY.refresh();
  requestUpdate();
}

void CalendarActivity::loop() {
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

void CalendarActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& summary = DESKTOP_SUMMARY.getState();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Calendar");

  time_t now = time(nullptr);
  struct tm localTm = {};
  localtime_r(&now, &localTm);

  char titleBuffer[32];
  snprintf(titleBuffer, sizeof(titleBuffer), "%04d-%02d-%02d", localTm.tm_year + 1900, localTm.tm_mon + 1,
           localTm.tm_mday);
  renderer.drawCenteredText(UI_12_FONT_ID, metrics.topPadding + metrics.headerHeight + 12, titleBuffer, true,
                            EpdFontFamily::BOLD);

  if (pageIndex == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 35, summary.todayPrimary.c_str());
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 5, summary.todaySecondary.c_str());
  } else if (pageIndex == 1) {
    const int top = metrics.topPadding + metrics.headerHeight + 84;
    const int left = metrics.contentSidePadding;
    drawLine(renderer, left, top, "Week View", true);

    const int todayWeekday = localTm.tm_wday;  // 0=Sun
    for (int i = 0; i < 7; ++i) {
      const int weekday = (todayWeekday + i) % 7;
      const int futureDay = localTm.tm_mday + i;
      char line[64];
      snprintf(line, sizeof(line), "%s  %s", weekdayLabel(weekday),
               (i == 0) ? "Today" : (futureDay <= 31 ? "Free" : "Next month"));
      drawLine(renderer, left, top + 32 + i * 24, line, i == 0);
    }
  } else {
    const int top = metrics.topPadding + metrics.headerHeight + 84;
    const int left = metrics.contentSidePadding;
    drawLine(renderer, left, top, "Month View", true);

    const int monthDay = localTm.tm_mday;
    // Compute days-in-month via mktime: advance to month+1 day 0 = last day of current month
    struct tm lastDayTm = localTm;
    lastDayTm.tm_mday = 0;
    lastDayTm.tm_mon += 1;
    mktime(&lastDayTm);
    const int daysInMonth = lastDayTm.tm_mday;

    const int monthProgress = daysInMonth > 0 ? std::clamp((monthDay * 100) / daysInMonth, 0, 100) : 0;
    const int barWidth = pageWidth - left * 2;
    const int fillWidth = (barWidth * monthProgress) / 100;
    const int barY = top + 34;

    renderer.drawRect(left, barY, barWidth, 18);
    if (fillWidth > 2) {
      renderer.fillRect(left + 1, barY + 1, fillWidth - 2, 16);
    }

    char progressLine[64];
    snprintf(progressLine, sizeof(progressLine), "Month progress: %d%%", monthProgress);
    drawLine(renderer, left, barY + 36, progressLine, true);

    char countLine[64];
    snprintf(countLine, sizeof(countLine), "Day %d of %d · %s", monthDay, daysInMonth,
             monthDay >= daysInMonth - 7 ? "near month end" : "in progress");
    drawLine(renderer, left, barY + 66, countLine);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Refresh", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
