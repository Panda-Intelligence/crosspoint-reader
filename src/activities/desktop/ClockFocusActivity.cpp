#include "ClockFocusActivity.h"

#include <Arduino.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
struct FocusPreset {
  const char* name;
  uint32_t durationMs;
};

constexpr FocusPreset kPresets[] = {
    {"Focus 25m", 25UL * 60UL * 1000UL},
    {"Short Break 5m", 5UL * 60UL * 1000UL},
    {"Long Break 15m", 15UL * 60UL * 1000UL},
    {"Countdown 45m", 45UL * 60UL * 1000UL},
};
constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));
}  // namespace

void ClockFocusActivity::resetTimer() {
  remainingMs = kPresets[presetIndex].durationMs;
  lastTickMs = millis();
  isRunning = false;
  hasFinished = false;
}

bool ClockFocusActivity::syncCountdown() {
  if (!isRunning || hasFinished || remainingMs == 0) {
    return false;
  }

  const uint32_t nowMs = millis();
  const uint32_t elapsedMs = nowMs - lastTickMs;
  if (elapsedMs < 1000) {
    return false;
  }

  lastTickMs = nowMs;
  if (elapsedMs >= remainingMs) {
    remainingMs = 0;
    isRunning = false;
    hasFinished = true;
  } else {
    remainingMs -= elapsedMs;
  }
  return true;
}

void ClockFocusActivity::onEnter() {
  Activity::onEnter();
  presetIndex = 0;
  resetTimer();
  requestUpdate();
}

void ClockFocusActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (!isRunning) {
    buttonNavigator.onNextRelease([this] {
      presetIndex = ButtonNavigator::nextIndex(presetIndex, kPresetCount);
      resetTimer();
      requestUpdate();
    });

    buttonNavigator.onPreviousRelease([this] {
      presetIndex = ButtonNavigator::previousIndex(presetIndex, kPresetCount);
      resetTimer();
      requestUpdate();
    });
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (hasFinished) {
      resetTimer();
    } else if (isRunning) {
      isRunning = false;
    } else {
      isRunning = true;
      lastTickMs = millis();
    }
    requestUpdate();
  }

  if (syncCountdown()) {
    requestUpdate();
  }
}

void ClockFocusActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Clock Focus");

  const uint32_t totalSeconds = (remainingMs + 500) / 1000;
  const uint32_t minutes = totalSeconds / 60;
  const uint32_t seconds = totalSeconds % 60;

  char timeBuffer[16];
  snprintf(timeBuffer, sizeof(timeBuffer), "%02lu:%02lu", static_cast<unsigned long>(minutes),
           static_cast<unsigned long>(seconds));

  renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + 12, kPresets[presetIndex].name,
                            true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 40, timeBuffer, true, EpdFontFamily::BOLD);

  if (hasFinished) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, "Focus complete");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 45, "Press Confirm to reset");
  } else if (isRunning) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, "Running");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 45, "Press Confirm to pause");
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, "Ready");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 45, "Use Up/Down to change preset");
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), hasFinished ? "Reset" : (isRunning ? "Pause" : "Start"),
                                            tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
