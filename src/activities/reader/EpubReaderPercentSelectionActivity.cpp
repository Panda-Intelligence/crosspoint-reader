#include "EpubReaderPercentSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
// Fine/coarse slider step sizes for percent adjustments.
constexpr int kSmallStep = 1;
constexpr int kLargeStep = 10;
constexpr int kSliderWidth = 360;
constexpr int kSliderHeight = 16;
constexpr int kSliderY = 140;
constexpr int kSliderTouchPadding = 24;

Rect sliderRect(const GfxRenderer& renderer) {
  const int screenWidth = renderer.getScreenWidth();
  const int barX = (screenWidth - kSliderWidth) / 2;
  return Rect{barX, kSliderY, kSliderWidth, kSliderHeight};
}

Rect expandedSliderTouchRect(const GfxRenderer& renderer) {
  const Rect rect = sliderRect(renderer);
  return Rect{rect.x, rect.y - kSliderTouchPadding, rect.width, rect.height + kSliderTouchPadding * 2};
}
}  // namespace

void EpubReaderPercentSelectionActivity::onEnter() {
  Activity::onEnter();
  // Set up rendering task and mark first frame dirty.
  requestUpdate();
}

void EpubReaderPercentSelectionActivity::onExit() { Activity::onExit(); }

void EpubReaderPercentSelectionActivity::adjustPercent(const int delta) {
  // Apply delta and clamp within 0-100.
  setPercent(percent + delta);
}

void EpubReaderPercentSelectionActivity::setPercent(const int value) {
  percent = value;
  if (percent < 0) {
    percent = 0;
  } else if (percent > 100) {
    percent = 100;
  }
  requestUpdate();
}

void EpubReaderPercentSelectionActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap() &&
        TouchHitTest::pointInRect(touchEvent.x, touchEvent.y, expandedSliderTouchRect(renderer))) {
      const Rect rect = sliderRect(renderer);
      const int clampedX = std::max(rect.x, std::min(static_cast<int>(touchEvent.x), rect.x + rect.width));
      mappedInput.suppressTouchButtonFallback();
      setPercent(((clampedX - rect.x) * 100 + rect.width / 2) / rect.width);
      return;
    }

    if (!buttonHintTap && touchEvent.isTap()) {
      const int leftBoundary = renderer.getScreenWidth() / 3;
      const int rightBoundary = (renderer.getScreenWidth() * 2) / 3;
      if (touchEvent.x < leftBoundary) {
        mappedInput.suppressTouchButtonFallback();
        adjustPercent(-kSmallStep);
        return;
      }
      if (touchEvent.x >= rightBoundary) {
        mappedInput.suppressTouchButtonFallback();
        adjustPercent(kSmallStep);
        return;
      }
    } else if (!buttonHintTap && touchEvent.type == InputTouchEvent::Type::SwipeLeft) {
      mappedInput.suppressTouchButtonFallback();
      adjustPercent(kSmallStep);
      return;
    } else if (!buttonHintTap && touchEvent.type == InputTouchEvent::Type::SwipeRight) {
      mappedInput.suppressTouchButtonFallback();
      adjustPercent(-kSmallStep);
      return;
    } else if (!buttonHintTap && touchEvent.type == InputTouchEvent::Type::SwipeUp) {
      mappedInput.suppressTouchButtonFallback();
      adjustPercent(kLargeStep);
      return;
    } else if (!buttonHintTap && touchEvent.type == InputTouchEvent::Type::SwipeDown) {
      mappedInput.suppressTouchButtonFallback();
      adjustPercent(-kLargeStep);
      return;
    }
  }

  // Back cancels, confirm selects, arrows adjust the percent.
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    setResult(PercentResult{percent});
    finish();
    return;
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] { adjustPercent(-kSmallStep); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] { adjustPercent(kSmallStep); });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] { adjustPercent(kLargeStep); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this] { adjustPercent(-kLargeStep); });
}

void EpubReaderPercentSelectionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  // Title and numeric percent value.
  renderer.drawCenteredText(UI_12_FONT_ID, 15, tr(STR_GO_TO_PERCENT), true, EpdFontFamily::BOLD);

  const std::string percentText = std::to_string(percent) + "%";
  renderer.drawCenteredText(UI_12_FONT_ID, 90, percentText.c_str(), true, EpdFontFamily::BOLD);

  // Draw slider track.
  const Rect rect = sliderRect(renderer);

  renderer.drawRect(rect.x, rect.y, rect.width, rect.height);

  // Fill slider based on percent.
  const int fillWidth = (rect.width - 4) * percent / 100;
  if (fillWidth > 0) {
    renderer.fillRect(rect.x + 2, rect.y + 2, fillWidth, rect.height - 4);
  }

  // Draw a simple knob centered at the current percent.
  const int knobX = rect.x + 2 + fillWidth - 2;
  renderer.fillRect(knobX, rect.y - 4, 4, rect.height + 8, true);

  // Hint text for step sizes.
  renderer.drawCenteredText(SMALL_FONT_ID, rect.y + 30, tr(STR_PERCENT_STEP_HINT), true);

  // Button hints follow the current front button layout.
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "-", "+");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
