#include "CalendarActivity.h"

#include <Arduino.h>
#include <I18n.h>

#include <algorithm>
#include <ctime>

#include "DesktopSummaryStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kPageCount = 3;

StrId weekdayShortId(int weekday) {
  static constexpr StrId kLabels[] = {StrId::STR_WEEKDAY_SHORT_SUN, StrId::STR_WEEKDAY_SHORT_MON,
                                      StrId::STR_WEEKDAY_SHORT_TUE, StrId::STR_WEEKDAY_SHORT_WED,
                                      StrId::STR_WEEKDAY_SHORT_THU, StrId::STR_WEEKDAY_SHORT_FRI,
                                      StrId::STR_WEEKDAY_SHORT_SAT};
  return kLabels[std::clamp(weekday, 0, 6)];
}

StrId weekdayFullId(int weekday) {
  static constexpr StrId kFull[] = {StrId::STR_WEEKDAY_SUN, StrId::STR_WEEKDAY_MON, StrId::STR_WEEKDAY_TUE,
                                    StrId::STR_WEEKDAY_WED, StrId::STR_WEEKDAY_THU, StrId::STR_WEEKDAY_FRI,
                                    StrId::STR_WEEKDAY_SAT};
  return kFull[std::clamp(weekday, 0, 6)];
}

StrId monthId(int monthIndex) {
  static constexpr StrId kMonths[] = {StrId::STR_MONTH_JAN, StrId::STR_MONTH_FEB, StrId::STR_MONTH_MAR,
                                      StrId::STR_MONTH_APR, StrId::STR_MONTH_MAY, StrId::STR_MONTH_JUN,
                                      StrId::STR_MONTH_JUL, StrId::STR_MONTH_AUG, StrId::STR_MONTH_SEP,
                                      StrId::STR_MONTH_OCT, StrId::STR_MONTH_NOV, StrId::STR_MONTH_DEC};
  return kMonths[std::clamp(monthIndex, 0, 11)];
}

void drawLine(const GfxRenderer& renderer, int x, int y, const char* text, bool bold = false) {
  renderer.drawText(UI_10_FONT_ID, x, y, text, true, bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
}

// Compute number of days in a given month (1-based month, full year)
int daysInMonth(int year, int month) {
  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month;  // month+1 day 0 = last day of month
  t.tm_mday = 0;
  mktime(&t);
  return t.tm_mday;
}

// 0=Sun weekday of the 1st of the month
int firstWeekday(int year, int month) {
  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = 1;
  mktime(&t);
  return t.tm_wday;
}

// ISO week number (approx)
int isoWeek(const struct tm& tm) {
  char buf[4];
  strftime(buf, sizeof(buf), "%V", &tm);
  return atoi(buf);
}
}  // namespace

void CalendarActivity::onEnter() {
  Activity::onEnter();
  pageIndex = 0;
  DESKTOP_SUMMARY.refresh();
  requestUpdate();
}

void CalendarActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      mappedInput.suppressTouchButtonFallback();
      DESKTOP_SUMMARY.refresh();
      requestUpdate();
      return;
    }
    if (!buttonHintTap && TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      pageIndex = ButtonNavigator::nextIndex(pageIndex, kPageCount);
      requestUpdate();
      return;
    }
    if (!buttonHintTap && TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      pageIndex = ButtonNavigator::previousIndex(pageIndex, kPageCount);
      requestUpdate();
      return;
    }
  }

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

  time_t now = time(nullptr);
  struct tm localTm = {};
  localtime_r(&now, &localTm);

  const int year = localTm.tm_year + 1900;
  const int month = localTm.tm_mon + 1;
  const int day = localTm.tm_mday;
  const int todayWday = localTm.tm_wday;

  char headerBuf[32];
  snprintf(headerBuf, sizeof(headerBuf), "%s %04d", I18n::getInstance().get(monthId(localTm.tm_mon)), year);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, headerBuf);

  const int pad = metrics.contentSidePadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + 8;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - 8;

  if (pageIndex == 0) {
    // ── Page 0: Mini calendar grid + today highlights ───────────────────
    // Top meta row: weekday name + "Today N"
    renderer.drawText(SMALL_FONT_ID, pad, contentTop, I18n::getInstance().get(weekdayFullId(todayWday)));
    char todayLabel[16];
    snprintf(todayLabel, sizeof(todayLabel), tr(STR_CALENDAR_TODAY_FORMAT), day);
    const int tlW = renderer.getTextWidth(SMALL_FONT_ID, todayLabel);
    renderer.drawText(SMALL_FONT_ID, pageWidth - pad - tlW, contentTop, todayLabel);

    // Mini 7-column calendar grid showing 3 weeks (prev, current, next)
    const int gridTop = contentTop + 22;
    const int colW = (pageWidth - pad * 2) / 7;
    const int cellSize = colW - 2;

    // Weekday column headers
    for (int c = 0; c < 7; c++) {
      const char* dayLabel = I18n::getInstance().get(weekdayShortId(c));
      const int x = pad + c * colW + (colW - renderer.getTextWidth(SMALL_FONT_ID, dayLabel)) / 2;
      renderer.drawText(SMALL_FONT_ID, x, gridTop, dayLabel);
    }

    // Divider under headers
    renderer.drawLine(pad, gridTop + 18, pageWidth - pad, gridTop + 18, true);

    // Determine first weekday of the month and days-in-month
    const int dim = daysInMonth(year, month);
    const int fwd = firstWeekday(year, month);  // 0=Sun

    // Find the week row that contains today
    // Cell index of today: fwd + (day - 1)
    const int todayCellIdx = fwd + (day - 1);
    const int todayRow = todayCellIdx / 7;

    // Draw 3 rows: todayRow-1, todayRow, todayRow+1 (clamped to 0..5)
    const int rowStart = std::max(0, todayRow - 1);
    const int rowCount = 3;
    const int rowH = 26;
    const int rowTop = gridTop + 22;

    for (int row = rowStart; row < rowStart + rowCount && row <= 5; row++) {
      for (int col = 0; col < 7; col++) {
        const int cellIdx = row * 7 + col;
        const int d = cellIdx - fwd + 1;
        if (d < 1 || d > dim) continue;

        const int x = pad + col * colW;
        const int y = rowTop + (row - rowStart) * rowH;
        const bool isToday = (d == day);

        if (isToday) {
          // Inverted cell — filled black circle, white number
          renderer.fillRoundedRect(x + 1, y, cellSize, cellSize, cellSize / 2, Color::Black);
          char ds[4];
          snprintf(ds, sizeof(ds), "%d", d);
          const int tw = renderer.getTextWidth(SMALL_FONT_ID, ds);
          renderer.drawText(SMALL_FONT_ID, x + 1 + (cellSize - tw) / 2, y + 4, ds, false);
        } else {
          char ds[4];
          snprintf(ds, sizeof(ds), "%d", d);
          const int tw = renderer.getTextWidth(SMALL_FONT_ID, ds);
          renderer.drawText(SMALL_FONT_ID, x + 1 + (cellSize - tw) / 2, y + 4, ds, true);
        }
      }
    }

    // Divider before agenda
    const int agendaTop = rowTop + rowCount * rowH + 6;
    renderer.drawLine(pad, agendaTop, pageWidth - pad, agendaTop, true);

    // Bullet agenda items
    const int bulletR = 4;
    const int bulletX = pad + bulletR;
    const int textX = pad + bulletR * 2 + 6;
    const int lineH = 22;
    const int week = isoWeek(localTm);

    static constexpr StrId kAgenda[] = {StrId::STR_CALENDAR_AGENDA_STUDY, StrId::STR_CALENDAR_AGENDA_FOCUS,
                                        StrId::STR_CALENDAR_AGENDA_MAZE};
    for (int i = 0; i < 3; i++) {
      const int lineY = agendaTop + 10 + i * lineH;
      if (lineY + 16 > contentBottom) break;
      renderer.fillRoundedRect(bulletX - bulletR, lineY + 4, bulletR * 2, bulletR * 2, bulletR, Color::Black);
      renderer.drawText(SMALL_FONT_ID, textX, lineY, I18n::getInstance().get(kAgenda[i]));
    }

    // Bottom status row
    const int statusY = contentBottom - 14;
    char weekStr[16];
    snprintf(weekStr, sizeof(weekStr), tr(STR_CALENDAR_WEEK_FORMAT), week);
    renderer.drawText(SMALL_FONT_ID, pad, statusY, weekStr);
    char dimStr[24];
    snprintf(dimStr, sizeof(dimStr), tr(STR_CALENDAR_DAYS_LEFT_FORMAT), dim - day);
    const int dsW = renderer.getTextWidth(SMALL_FONT_ID, dimStr);
    renderer.drawText(SMALL_FONT_ID, pageWidth - pad - dsW, statusY, dimStr);

  } else if (pageIndex == 1) {
    // ── Page 1: Week view ───────────────────────────────────────────────
    const int top = contentTop + 8;
    drawLine(renderer, pad, top, tr(STR_CALENDAR_THIS_WEEK), true);

    const int dim = daysInMonth(year, month);
    for (int i = 0; i < 7; i++) {
      const int weekday = (todayWday + i) % 7;
      const int futureDay = day + i;
      const bool isToday = (i == 0);

      char line[48];
      if (futureDay <= dim) {
        snprintf(line, sizeof(line), "%s %d  %s", I18n::getInstance().get(weekdayFullId(weekday)), futureDay,
                 isToday ? tr(STR_DASHBOARD_TODAY) : tr(STR_CALENDAR_NO_EVENT));
      } else {
        snprintf(line, sizeof(line), "%s %d  %s", I18n::getInstance().get(weekdayFullId(weekday)), futureDay - dim,
                 tr(STR_CALENDAR_NEXT_MONTH));
      }
      drawLine(renderer, pad, top + 32 + i * 26, line, isToday);
    }

  } else {
    // ── Page 2: Month progress ──────────────────────────────────────────
    const int top = contentTop + 8;
    drawLine(renderer, pad, top, tr(STR_CALENDAR_MONTH_VIEW), true);

    const int dim = daysInMonth(year, month);
    const int monthProgress = dim > 0 ? std::clamp((day * 100) / dim, 0, 100) : 0;
    const int barWidth = pageWidth - pad * 2;
    const int fillWidth = (barWidth - 2) * monthProgress / 100;
    const int barY = top + 34;
    const int barH = 12;

    // Pill progress bar
    renderer.drawRoundedRect(pad, barY, barWidth, barH, 1, barH / 2, true);
    if (fillWidth > 2) {
      renderer.fillRoundedRect(pad + 1, barY + 1, fillWidth, barH - 2, (barH - 2) / 2, Color::Black);
    }

    char progressLine[48];
    snprintf(progressLine, sizeof(progressLine), tr(STR_CALENDAR_MONTH_PROGRESS_FORMAT), monthProgress);
    drawLine(renderer, pad, barY + barH + 12, progressLine, true);

    char countLine[48];
    snprintf(countLine, sizeof(countLine), tr(STR_CALENDAR_DAY_OF_MONTH_FORMAT), day, dim,
             day >= dim - 7 ? tr(STR_CALENDAR_NEAR_MONTH_END) : tr(STR_CALENDAR_IN_PROGRESS));
    drawLine(renderer, pad, barY + barH + 44, countLine);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
