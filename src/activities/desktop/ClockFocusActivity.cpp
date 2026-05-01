#include "ClockFocusActivity.h"

#include <Arduino.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
struct FocusPreset {
  StrId name;
  uint32_t durationMs;
};

constexpr FocusPreset kPresets[] = {
    {StrId::STR_FOCUS_PRESET_FOCUS_25M, 25UL * 60UL * 1000UL},
    {StrId::STR_FOCUS_PRESET_SHORT_BREAK_5M, 5UL * 60UL * 1000UL},
    {StrId::STR_FOCUS_PRESET_LONG_BREAK_15M, 15UL * 60UL * 1000UL},
    {StrId::STR_FOCUS_PRESET_COUNTDOWN_45M, 45UL * 60UL * 1000UL},
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
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      mappedInput.suppressTouchButtonFallback();
      if (hasFinished) {
        resetTimer();
      } else if (isRunning) {
        isRunning = false;
      } else {
        isRunning = true;
        lastTickMs = millis();
      }
      requestUpdate();
      return;
    }
    if (!buttonHintTap && !isRunning && TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      presetIndex = ButtonNavigator::nextIndex(presetIndex, kPresetCount);
      resetTimer();
      requestUpdate();
      return;
    }
    if (!buttonHintTap && !isRunning && TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      presetIndex = ButtonNavigator::previousIndex(presetIndex, kPresetCount);
      resetTimer();
      requestUpdate();
      return;
    }
  }

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

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DESKTOP_CLOCK_FOCUS));

  const uint32_t totalSeconds = (remainingMs + 500) / 1000;
  const uint32_t minutes = totalSeconds / 60;
  const uint32_t seconds = totalSeconds % 60;

  char timeBuffer[16];
  snprintf(timeBuffer, sizeof(timeBuffer), "%02lu:%02lu", static_cast<unsigned long>(minutes),
           static_cast<unsigned long>(seconds));

  renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + 12,
                            I18n::getInstance().get(kPresets[presetIndex].name), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 40, timeBuffer, true, EpdFontFamily::BOLD);

  if (hasFinished) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, tr(STR_FOCUS_COMPLETE));
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 45, tr(STR_FOCUS_RESET_HINT));
  } else if (isRunning) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, tr(STR_FOCUS_RUNNING));
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 45, tr(STR_FOCUS_PAUSE_HINT));
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, tr(STR_FOCUS_READY));
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 45, tr(STR_FOCUS_PRESET_HINT));
  }

  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), hasFinished ? tr(STR_RESET) : (isRunning ? tr(STR_PAUSE) : tr(STR_START)),
                            tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
