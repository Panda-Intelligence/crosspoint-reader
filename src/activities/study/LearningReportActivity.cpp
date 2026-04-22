#include "LearningReportActivity.h"

#include <I18n.h>

#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

void LearningReportActivity::onEnter() {
  Activity::onEnter();
  STUDY_STATE.loadFromFile();
  requestUpdate();
}

void LearningReportActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    finish();
  }
}

void LearningReportActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& state = STUDY_STATE.getState();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Learning Report");

  char accuracyBuffer[32];
  const int total = state.correctToday + state.wrongToday;
  const int accuracy = total > 0 ? (state.correctToday * 100 / total) : 0;
  snprintf(accuracyBuffer, sizeof(accuracyBuffer), "Accuracy: %d%%", accuracy);

  char summaryBuffer[48];
  snprintf(summaryBuffer, sizeof(summaryBuffer), "Done %u of %u · streak %u", state.completedToday, state.dueToday,
           state.streakDays);

  renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 50, accuracyBuffer, true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 5, summaryBuffer);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 35, "Detailed analytics come later");

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Done", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
