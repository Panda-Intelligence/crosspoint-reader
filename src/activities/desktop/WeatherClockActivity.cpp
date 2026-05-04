#include "WeatherClockActivity.h"

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

void drawBulletLine(const GfxRenderer& renderer, int x, int y, int bulletR, const char* text) {
  renderer.fillRoundedRect(x, y + 3, bulletR * 2, bulletR * 2, bulletR, Color::Black);
  renderer.drawText(SMALL_FONT_ID, x + bulletR * 2 + 6, y, text);
}

void drawMetricCard(const GfxRenderer& renderer, int x, int y, int w, int h, const char* label, const char* value) {
  renderer.drawRoundedRect(x, y, w, h, 1, 8, true);
  const int lw = renderer.getTextWidth(SMALL_FONT_ID, label);
  renderer.drawText(SMALL_FONT_ID, x + (w - lw) / 2, y + 6, label);
  const int vw = renderer.getTextWidth(UI_10_FONT_ID, value);
  renderer.drawText(UI_10_FONT_ID, x + (w - vw) / 2, y + h - 22, value, true, EpdFontFamily::BOLD);
}
}  // namespace

void WeatherClockActivity::onEnter() {
  Activity::onEnter();
  pageIndex = 0;
  DESKTOP_SUMMARY.refresh();
  requestUpdate();
}

void WeatherClockActivity::loop() {
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

void WeatherClockActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& summary = DESKTOP_SUMMARY.getState();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DASHBOARD_WEATHER));

  time_t now = time(nullptr);
  struct tm localTm = {};
  localtime_r(&now, &localTm);

  char timeBuffer[8];
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", localTm.tm_hour, localTm.tm_min);
  char dateBuffer[24];
  snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d", localTm.tm_year + 1900, localTm.tm_mon + 1,
           localTm.tm_mday);

  const int pad = metrics.contentSidePadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + 8;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - 8;

  if (pageIndex == 0) {
    // ── Page 0: Clock + weather + metrics ──────────────────────────────

    // Date small label + location on same row
    renderer.drawText(SMALL_FONT_ID, pad, contentTop + 4, dateBuffer);
    const int locW = renderer.getTextWidth(SMALL_FONT_ID, summary.city.c_str());
    renderer.drawText(SMALL_FONT_ID, pageWidth - pad - locW, contentTop + 4, summary.city.c_str());

    // Large time (primary display element)
    renderer.drawCenteredText(UI_12_FONT_ID, contentTop + 28, timeBuffer, true, EpdFontFamily::BOLD);

    // Weather line below time
    renderer.drawCenteredText(UI_10_FONT_ID, contentTop + 92, summary.weatherLine.c_str());

    // Thin divider
    const int divY = contentTop + 118;
    renderer.drawLine(pad, divY, pageWidth - pad, divY, true);

    // 3-column metrics grid
    const int metricTop = divY + 10;
    const int metricH = 58;
    const int gap = 8;
    const int metricW = (pageWidth - pad * 2 - gap * 2) / 3;
    const int temp = summary.temperatureC;

    char rainStr[8];
    snprintf(rainStr, sizeof(rainStr), "%d%%", summary.isOnline ? 70 : 0);
    char windStr[12];
    snprintf(windStr, sizeof(windStr), "%d km/h", summary.isOnline ? 8 : 0);
    char tempStr[8];
    snprintf(tempStr, sizeof(tempStr), "%d %s", temp, tr(STR_TEMP_UNIT_C));

    drawMetricCard(renderer, pad, metricTop, metricW, metricH, tr(STR_WEATHER_METRIC_RAIN), rainStr);
    drawMetricCard(renderer, pad + metricW + gap, metricTop, metricW, metricH, tr(STR_WEATHER_METRIC_WIND), windStr);
    drawMetricCard(renderer, pad + (metricW + gap) * 2, metricTop, metricW, metricH, tr(STR_WEATHER_METRIC_TEMP),
                   tempStr);

    // Bullet commute hints
    const int bulletTop = metricTop + metricH + 12;
    const int bulletR = 3;
    drawBulletLine(renderer, pad, bulletTop, bulletR,
                   summary.isOnline ? tr(STR_WEATHER_UMBRELLA_RECOMMENDED) : tr(STR_WEATHER_OFFLINE_NO_FORECAST));
    drawBulletLine(renderer, pad, bulletTop + 22, bulletR,
                   summary.isOnline ? tr(STR_WEATHER_LIGHT_TRAFFIC_WINDOW) : tr(STR_WEATHER_CACHED_DATA_ONLY));

  } else if (pageIndex == 1) {
    // ── Page 1: 3-day forecast ─────────────────────────────────────────
    renderer.drawText(UI_10_FONT_ID, pad, contentTop + 4, tr(STR_WEATHER_FORECAST), true, EpdFontFamily::BOLD);

    const int temp = summary.temperatureC;
    const int spread = summary.isOnline ? 6 : 5;
    struct DayForecast {
      StrId day;
      int low;
      int high;
      StrId cond;
    };
    const DayForecast forecasts[3] = {
        {StrId::STR_DASHBOARD_TODAY, temp - spread, temp + spread,
         summary.isOnline ? summary.weatherConditionId : StrId::STR_WEATHER_CACHED},
        {StrId::STR_WEATHER_TOMORROW, temp - spread - 1, temp + spread + 1,
         summary.isOnline ? StrId::STR_WEATHER_SUNNY : StrId::STR_WEATHER_OFFLINE},
        {StrId::STR_WEATHER_PLUS_2_DAYS, temp - spread + 1, temp + spread - 1,
         summary.isOnline ? StrId::STR_WEATHER_RAIN_CHANCE : StrId::STR_WEATHER_CACHED},
    };

    const int rowH = 56;
    for (int i = 0; i < 3; i++) {
      const int rowY = contentTop + 28 + i * rowH;
      renderer.drawRoundedRect(pad, rowY, pageWidth - pad * 2, rowH - 6, 1, 8, true);

      const char* dayLabel = I18n::getInstance().get(forecasts[i].day);
      const char* conditionLabel = I18n::getInstance().get(forecasts[i].cond);
      renderer.drawText(UI_10_FONT_ID, pad + 12, rowY + 8, dayLabel, true,
                        i == 0 ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      char tempRange[20];
      snprintf(tempRange, sizeof(tempRange), "%d - %d %s", forecasts[i].low, forecasts[i].high, tr(STR_TEMP_UNIT_C));
      renderer.drawText(SMALL_FONT_ID, pad + 12, rowY + 30, tempRange);

      const int condW = renderer.getTextWidth(SMALL_FONT_ID, conditionLabel);
      renderer.drawText(SMALL_FONT_ID, pageWidth - pad - condW - 12, rowY + 19, conditionLabel);
    }

    // Source note
    renderer.drawCenteredText(SMALL_FONT_ID, contentBottom - 14,
                              summary.isOnline ? tr(STR_WEATHER_SOURCE_COMPANION) : tr(STR_WEATHER_SOURCE_CACHED));

  } else {
    // ── Page 2: Commute scenes ─────────────────────────────────────────
    renderer.drawText(UI_10_FONT_ID, pad, contentTop + 4, tr(STR_WEATHER_SCENES), true, EpdFontFamily::BOLD);

    const int temp = summary.temperatureC;
    const bool carryUmbrella = summary.isOnline;

    struct Scene {
      StrId title;
      const char* detail;
    };

    Scene scenes[3];
    scenes[0].title = StrId::STR_WEATHER_SCENE_COMMUTE;
    scenes[0].detail = carryUmbrella ? tr(STR_WEATHER_LEAVE_10_MIN_EARLY) : tr(STR_WEATHER_NORMAL_TRAFFIC);
    scenes[1].title = StrId::STR_WEATHER_SCENE_CLOTHING;
    scenes[1].detail = temp <= 20 ? tr(STR_WEATHER_LIGHT_JACKET) : tr(STR_WEATHER_SHIRT_LAYER);
    scenes[2].title = StrId::STR_WEATHER_SCENE_UMBRELLA;
    scenes[2].detail = carryUmbrella ? tr(STR_WEATHER_RECOMMENDED_TODAY) : tr(STR_WEATHER_OPTIONAL);

    const int rowH = 52;
    for (int i = 0; i < 3; i++) {
      const int rowY = contentTop + 28 + i * rowH;
      renderer.drawRoundedRect(pad, rowY, pageWidth - pad * 2, rowH - 6, 1, 8, true);
      renderer.drawText(SMALL_FONT_ID, pad + 12, rowY + 6, I18n::getInstance().get(scenes[i].title));
      renderer.drawText(UI_10_FONT_ID, pad + 12, rowY + 24, scenes[i].detail, true, EpdFontFamily::BOLD);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
