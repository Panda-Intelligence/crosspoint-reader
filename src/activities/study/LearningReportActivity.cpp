#include "LearningReportActivity.h"

#include <I18n.h>

#include <algorithm>
#include <ctime>

#include "StudyReviewQueueStore.h"
#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
StrId weekdayShortId(int weekday) {
  static constexpr StrId kLabels[] = {StrId::STR_WEEKDAY_SHORT_SUN, StrId::STR_WEEKDAY_SHORT_MON,
                                      StrId::STR_WEEKDAY_SHORT_TUE, StrId::STR_WEEKDAY_SHORT_WED,
                                      StrId::STR_WEEKDAY_SHORT_THU, StrId::STR_WEEKDAY_SHORT_FRI,
                                      StrId::STR_WEEKDAY_SHORT_SAT};
  return kLabels[std::clamp(weekday, 0, 6)];
}
}  // namespace

void LearningReportActivity::onEnter() {
  Activity::onEnter();
  STUDY_STATE.loadFromFile();
  STUDY_REVIEW_QUEUE.loadFromFile();
  requestUpdate();
}

void LearningReportActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (touchEvent.isTap() || TouchHitTest::isForwardSwipe(touchEvent) || TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      finish();
      return;
    }
  }

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

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_STUDY_LEARNING_REPORT));

  const int pad = metrics.contentSidePadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + 10;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - 8;

  // Compute stats
  const int total = static_cast<int>(state.correctToday) + static_cast<int>(state.wrongToday);
  const int accuracy = total > 0 ? (static_cast<int>(state.correctToday) * 100 / total) : 0;
  const int completion =
      state.dueToday > 0 ? (static_cast<int>(state.completedToday) * 100 / static_cast<int>(state.dueToday)) : 0;
  const int wrongRate = total > 0 ? (static_cast<int>(state.wrongToday) * 100 / total) : 0;
  const int masteryScore = std::clamp(accuracy + (completion / 2) - (wrongRate / 3), 0, 100);
  const int againCount = STUDY_REVIEW_QUEUE.getAgainCount();
  const int laterCount = STUDY_REVIEW_QUEUE.getLaterCount();
  const int savedCount = STUDY_REVIEW_QUEUE.getSavedCount();

  const char* weakArea = tr(STR_STUDY_BALANCED_PROGRESS);
  const char* nextFocus = tr(STR_STUDY_KEEP_DAILY_REVIEW_STEADY);
  if (againCount >= laterCount && againCount >= savedCount && againCount > 0) {
    weakArea = tr(STR_STUDY_RECOVERY_BACKLOG_HIGH);
    nextFocus = tr(STR_STUDY_PRIORITISE_WRONG_CARDS_FIRST);
  } else if (laterCount >= againCount && laterCount >= savedCount && laterCount > 0) {
    weakArea = tr(STR_STUDY_LATER_QUEUE_GROWING);
    nextFocus = tr(STR_STUDY_TURN_POSTPONED_INTO_WINS);
  } else if (savedCount > 0) {
    weakArea = tr(STR_STUDY_SAVED_SET_EXPANDING);
    nextFocus = tr(STR_STUDY_REVISIT_SAVED_THIS_WEEK);
  }

  // ── Today label ──────────────────────────────────────────────────────
  renderer.drawText(UI_10_FONT_ID, pad, contentTop, tr(STR_STUDY_TODAY), true, EpdFontFamily::BOLD);

  // ── Vertical bar chart (7 mock days, today is the last bar) ──────────
  // Mock accuracy history ending with today's real accuracy
  const int kBars = 7;
  const int barValues[kBars] = {44, 56, 72, 68, 82, 78, accuracy};

  time_t now = time(nullptr);
  struct tm localTm = {};
  localtime_r(&now, &localTm);

  const int chartTop = contentTop + 28;
  const int chartH = 80;
  const int chartBottom = chartTop + chartH;
  const int totalBarWidth = pageWidth - pad * 2;
  const int barSlot = totalBarWidth / kBars;
  const int barW = barSlot - 6;

  for (int i = 0; i < kBars; i++) {
    const int barH = std::max(4, barValues[i] * chartH / 100);
    const int bx = pad + i * barSlot;
    const int by = chartBottom - barH;
    const bool isToday = (i == kBars - 1);

    if (isToday) {
      renderer.fillRoundedRect(bx, by, barW, barH, 4, Color::Black);
    } else {
      renderer.fillRoundedRect(bx, by, barW, barH, 4, Color::LightGray);
      renderer.drawRoundedRect(bx, by, barW, barH, 1, 4, true);
    }

    // Day label below bar
    const int weekday = (localTm.tm_wday + i - (kBars - 1) + 7) % 7;
    const char* dayLabel = I18N.get(weekdayShortId(weekday));
    const int lw = renderer.getTextWidth(SMALL_FONT_ID, dayLabel);
    renderer.drawText(SMALL_FONT_ID, bx + (barW - lw) / 2, chartBottom + 4, dayLabel, true,
                      isToday ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  }

  renderer.drawText(SMALL_FONT_ID, pad, chartBottom + 20, tr(STR_STUDY_WEEKLY_ACCURACY));
  char accPct[16];
  snprintf(accPct, sizeof(accPct), "%d%%", accuracy);
  const int apW = renderer.getTextWidth(SMALL_FONT_ID, accPct);
  renderer.drawText(SMALL_FONT_ID, pageWidth - pad - apW, chartBottom + 20, accPct);

  // ── Divider ───────────────────────────────────────────────────────────
  const int divY = chartBottom + 36;
  renderer.drawLine(pad, divY, pageWidth - pad, divY, true);

  // ── Stat rows ─────────────────────────────────────────────────────────
  struct StatRow {
    const char* label;
    char value[48];
  };

  StatRow rows[3];
  rows[0].label = tr(STR_STUDY_CARDS_DONE);
  snprintf(rows[0].value, sizeof(rows[0].value), "%u / %u  (%d%%)", state.completedToday, state.dueToday, completion);
  rows[1].label = tr(STR_STUDY_ACCURACY);
  snprintf(rows[1].value, sizeof(rows[1].value), "%d%%  (%u wrong)", accuracy, state.wrongToday);
  rows[2].label = tr(STR_STUDY_REVIEW_QUEUE);
  snprintf(rows[2].value, sizeof(rows[2].value), tr(STR_STUDY_QUEUE_COUNTS_SHORT_FORMAT), againCount, laterCount,
           savedCount);

  const int rowH = 28;
  for (int i = 0; i < 3; i++) {
    const int ry = divY + 10 + i * rowH;
    renderer.drawText(SMALL_FONT_ID, pad, ry, rows[i].label);
    const int vw = renderer.getTextWidth(UI_10_FONT_ID, rows[i].value);
    renderer.drawText(UI_10_FONT_ID, pageWidth - pad - vw, ry - 2, rows[i].value, true, EpdFontFamily::BOLD);
  }

  // ── Bottom status row ─────────────────────────────────────────────────
  const int statusY = contentBottom - 14;
  char streakStr[40];
  snprintf(streakStr, sizeof(streakStr), tr(STR_STUDY_STREAK_DAYS_FORMAT), state.streakDays);
  renderer.drawText(SMALL_FONT_ID, pad, statusY, streakStr);
  char masteryStr[40];
  snprintf(masteryStr, sizeof(masteryStr), tr(STR_STUDY_MASTERY_FORMAT), masteryScore);
  renderer.drawText(SMALL_FONT_ID, pageWidth - pad - renderer.getTextWidth(SMALL_FONT_ID, masteryStr), statusY,
                    masteryStr);

  renderer.drawText(SMALL_FONT_ID, pad, statusY - 24, weakArea, true, EpdFontFamily::BOLD);
  renderer.drawText(SMALL_FONT_ID, pageWidth - pad - renderer.getTextWidth(SMALL_FONT_ID, nextFocus), statusY - 24,
                    nextFocus);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_DONE), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
