#include "StudyCardsTodayActivity.h"

#include <I18n.h>

#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

void StudyCardsTodayActivity::onEnter() {
  Activity::onEnter();
  STUDY_STATE.loadFromFile();
  inSession = false;
  actionIndex = 0;
  requestUpdate();
}

void StudyCardsTodayActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (!inSession) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      inSession = true;
      actionIndex = 0;
      requestUpdate();
    }
    return;
  }

  buttonNavigator.onNextRelease([this] {
    actionIndex = ButtonNavigator::nextIndex(actionIndex, 2);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    actionIndex = ButtonNavigator::previousIndex(actionIndex, 2);
    requestUpdate();
  });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    STUDY_STATE.recordReviewResult(actionIndex == 0);
    if (STUDY_STATE.getState().completedToday >= STUDY_STATE.getState().dueToday) {
      inSession = false;
    }
    requestUpdate();
  }
}

void StudyCardsTodayActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& state = STUDY_STATE.getState();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Study Cards");

  if (!inSession) {
    char dueBuffer[32];
    snprintf(dueBuffer, sizeof(dueBuffer), "%u due today", state.dueToday);
    char doneBuffer[32];
    snprintf(doneBuffer, sizeof(doneBuffer), "%u done · streak %u", state.completedToday, state.streakDays);
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 40, dueBuffer, true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, doneBuffer);
  } else {
    const int remaining = static_cast<int>(state.dueToday) - static_cast<int>(state.completedToday);
    char remainingBuffer[32];
    snprintf(remainingBuffer, sizeof(remainingBuffer), "%d cards remaining", remaining);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 60, remainingBuffer, true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, "Did you remember this card?");

    const char* left = actionIndex == 0 ? "[ Correct ]" : "  Correct  ";
    const char* right = actionIndex == 1 ? "[ Again ]" : "  Again  ";
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 30, left);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 65, right);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), inSession ? "Grade" : "Start", tr(STR_DIR_UP),
                                            tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
