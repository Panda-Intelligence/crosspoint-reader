#include "CalendarActivity.h"

#include <Arduino.h>
#include <I18n.h>
#include <ctime>

#include "DesktopSummaryStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kPageCount = 3;
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
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, "Week View");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 20, "Weekly plan placeholder");
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, "Month View");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 20, "Month progress placeholder");
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Refresh", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
