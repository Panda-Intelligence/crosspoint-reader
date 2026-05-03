#include "FortuneActivity.h"

#include <Arduino.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr std::array<StrId, 8> kFortuneTitles = {
    StrId::STR_FORTUNE_RESULT_STEADY, StrId::STR_FORTUNE_RESULT_BEGIN,  StrId::STR_FORTUNE_RESULT_WAIT,
    StrId::STR_FORTUNE_RESULT_SHARE,  StrId::STR_FORTUNE_RESULT_REVIEW, StrId::STR_FORTUNE_RESULT_TRAVEL,
    StrId::STR_FORTUNE_RESULT_LEARN,  StrId::STR_FORTUNE_RESULT_REST,
};

constexpr std::array<StrId, 8> kFortuneDetails = {
    StrId::STR_FORTUNE_DETAIL_STEADY, StrId::STR_FORTUNE_DETAIL_BEGIN,  StrId::STR_FORTUNE_DETAIL_WAIT,
    StrId::STR_FORTUNE_DETAIL_SHARE,  StrId::STR_FORTUNE_DETAIL_REVIEW, StrId::STR_FORTUNE_DETAIL_TRAVEL,
    StrId::STR_FORTUNE_DETAIL_LEARN,  StrId::STR_FORTUNE_DETAIL_REST,
};

Rect contentRect(const GfxRenderer& renderer) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int height = renderer.getScreenHeight() - top - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  return Rect{metrics.contentSidePadding, top, renderer.getScreenWidth() - metrics.contentSidePadding * 2, height};
}
}  // namespace

void FortuneActivity::onEnter() {
  Activity::onEnter();
  drawAgain();
  requestUpdate();
}

void FortuneActivity::drawAgain() {
  if (mode == Mode::Fortune) {
    selectedFortune = static_cast<int>(random(static_cast<long>(kFortuneTitles.size())));
  } else {
    coinHeads = random(2) == 0;
  }
}

void FortuneActivity::toggleMode() {
  mode = mode == Mode::Fortune ? Mode::Coin : Mode::Fortune;
  drawAgain();
  requestUpdate();
}

const char* FortuneActivity::modeLabel() const {
  return mode == Mode::Fortune ? tr(STR_FORTUNE_MODE_FORTUNE) : tr(STR_FORTUNE_MODE_COIN);
}

const char* FortuneActivity::resultTitle() const {
  if (mode == Mode::Coin) {
    return coinHeads ? tr(STR_FORTUNE_COIN_HEADS) : tr(STR_FORTUNE_COIN_TAILS);
  }
  return I18N.get(kFortuneTitles[selectedFortune]);
}

const char* FortuneActivity::resultDetail() const {
  if (mode == Mode::Coin) {
    return coinHeads ? tr(STR_FORTUNE_COIN_HEADS_DETAIL) : tr(STR_FORTUNE_COIN_TAILS_DETAIL);
  }
  return I18N.get(kFortuneDetails[selectedFortune]);
}

void FortuneActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      mappedInput.suppressTouchButtonFallback();
      const Rect rect = contentRect(renderer);
      if (TouchHitTest::pointInRect(touchEvent.x, touchEvent.y, rect)) {
        drawAgain();
        requestUpdate();
      }
      return;
    }
    if (!buttonHintTap && (TouchHitTest::isForwardSwipe(touchEvent) || TouchHitTest::isBackwardSwipe(touchEvent))) {
      mappedInput.suppressTouchButtonFallback();
      toggleMode();
      return;
    }
    if (!buttonHintTap) {
      mappedInput.suppressTouchButtonFallback();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    drawAgain();
    requestUpdate();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
      mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    toggleMode();
  }
}

void FortuneActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const Rect rect = contentRect(renderer);

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FORTUNE_TITLE),
                 modeLabel());

  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 1, 8, true);
  renderer.drawCenteredText(UI_10_FONT_ID, rect.y + 24, modeLabel(), true, EpdFontFamily::BOLD);

  const int centerY = renderer.getTextYForCentering(rect.y, rect.height, UI_12_FONT_ID);
  renderer.drawCenteredText(UI_12_FONT_ID, centerY - 24, resultTitle(), true, EpdFontFamily::BOLD);

  const std::string detail = renderer.truncatedText(UI_10_FONT_ID, resultDetail(), rect.width - 32);
  renderer.drawCenteredText(UI_10_FONT_ID, centerY + 24, detail.c_str());
  renderer.drawCenteredText(SMALL_FONT_ID, rect.y + rect.height - 38, tr(STR_FORTUNE_TAP_HINT));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_FORTUNE_DRAW), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
