#include "LearningReportActivity.h"

#include <I18n.h>

#include <algorithm>

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

  const int total = state.correctToday + state.wrongToday;
  const int accuracy = total > 0 ? (state.correctToday * 100 / total) : 0;
  const int completion = state.dueToday > 0 ? (state.completedToday * 100 / state.dueToday) : 0;
  const int wrongRate = total > 0 ? (state.wrongToday * 100 / total) : 0;
  const int masteryScore = std::clamp(accuracy + (completion / 2) - (wrongRate / 3), 0, 100);

  char line1[48];
  snprintf(line1, sizeof(line1), "Done %u / %u (%d%%)", state.completedToday, state.dueToday, completion);
  char line2[48];
  snprintf(line2, sizeof(line2), "Accuracy %d%% · Wrong %u", accuracy, state.wrongToday);
  char line3[48];
  snprintf(line3, sizeof(line3), "Mastery score %d · Streak %u", masteryScore, state.streakDays);

  const int startY = metrics.topPadding + metrics.headerHeight + 24;
  renderer.drawCenteredText(UI_12_FONT_ID, startY, "Today", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, startY + 42, line1);
  renderer.drawCenteredText(UI_10_FONT_ID, startY + 78, line2);
  renderer.drawCenteredText(UI_10_FONT_ID, startY + 114, line3);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - metrics.buttonHintsHeight - 14, "Companion app shows full charts");

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Done", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
