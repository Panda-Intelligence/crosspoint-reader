#include "ArcadeChallengesActivity.h"

#include <I18n.h>

#include <algorithm>

#include "ArcadeProgressStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
struct ChallengeRow {
  StrId title;
  StrId detail;
  bool completed;
};

void drawMetricCard(const GfxRenderer& renderer, int x, int y, int w, int h, const char* label, const char* value) {
  renderer.drawRoundedRect(x, y, w, h, 1, 8, true);
  renderer.drawText(SMALL_FONT_ID, x + 12, y + 8, label);
  renderer.drawText(UI_10_FONT_ID, x + 12, y + h - 24, value, true, EpdFontFamily::BOLD);
}

void drawChallengeRow(const GfxRenderer& renderer, int x, int y, int w, const ChallengeRow& row) {
  renderer.drawRoundedRect(x, y, w, 44, 1, 8, true);
  renderer.drawText(UI_10_FONT_ID, x + 14, y + 8, I18N.get(row.title), true, EpdFontFamily::BOLD);
  renderer.drawText(SMALL_FONT_ID, x + 14, y + 24, I18N.get(row.detail));
  renderer.drawText(UI_10_FONT_ID, x + w - 70, y + 12, row.completed ? tr(STR_DONE) : tr(STR_OPEN), true,
                    row.completed ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
}
}  // namespace

void ArcadeChallengesActivity::onEnter() {
  Activity::onEnter();
  pageIndex = 0;
  ARCADE_PROGRESS.loadFromFile();
  ARCADE_PROGRESS.syncToCurrentDay();
  requestUpdate();
}

void ArcadeChallengesActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      mappedInput.suppressTouchButtonFallback();
      ARCADE_PROGRESS.loadFromFile();
      ARCADE_PROGRESS.syncToCurrentDay();
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
    ARCADE_PROGRESS.loadFromFile();
    ARCADE_PROGRESS.syncToCurrentDay();
    requestUpdate();
  }
}

void ArcadeChallengesActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int pad = metrics.contentSidePadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + 10;

  ARCADE_PROGRESS.syncToCurrentDay();
  const auto& state = ARCADE_PROGRESS.getState();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_ARCADE_GAME_CHALLENGES));

  if (pageIndex == 0) {
    char streakValue[24];
    snprintf(streakValue, sizeof(streakValue), tr(STR_ARCADE_DAYS_FORMAT), state.playStreakDays);
    char sessionsValue[24];
    snprintf(sessionsValue, sizeof(sessionsValue), tr(STR_ARCADE_TODAY_FORMAT), state.sessionsToday);
    char winsValue[24];
    snprintf(winsValue, sizeof(winsValue), tr(STR_ARCADE_TODAY_FORMAT), state.winsToday);
    char tileValue[24];
    snprintf(tileValue, sizeof(tileValue), tr(STR_ARCADE_MAX_FORMAT), state.dailyMax2048);

    const int cardW = (pageWidth - pad * 2 - 10) / 2;
    drawMetricCard(renderer, pad, contentTop, cardW, 58, tr(STR_ARCADE_PLAY_STREAK), streakValue);
    drawMetricCard(renderer, pad + cardW + 10, contentTop, cardW, 58, tr(STR_ARCADE_SESSIONS), sessionsValue);
    drawMetricCard(renderer, pad, contentTop + 68, cardW, 58, tr(STR_ARCADE_WINS), winsValue);
    drawMetricCard(renderer, pad + cardW + 10, contentTop + 68, cardW, 58, tr(STR_ARCADE_2048_PEAK), tileValue);

    const ChallengeRow challenges[] = {
        {StrId::STR_ARCADE_DAILY_OPENER, StrId::STR_ARCADE_DAILY_OPENER_DESC, state.sessionsToday >= 2},
        {StrId::STR_ARCADE_SHARP_SOLVER, StrId::STR_ARCADE_SHARP_SOLVER_DESC, state.winsToday >= 2},
        {StrId::STR_ARCADE_TILE_CLIMBER, StrId::STR_ARCADE_TILE_CLIMBER_DESC, state.dailyMax2048 >= 128},
    };

    renderer.drawText(UI_10_FONT_ID, pad, contentTop + 142, tr(STR_ARCADE_DAILY_MISSIONS), true, EpdFontFamily::BOLD);
    for (int i = 0; i < 3; i++) {
      drawChallengeRow(renderer, pad, contentTop + 166 + i * 52, pageWidth - pad * 2, challenges[i]);
    }

    renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - metrics.buttonHintsHeight - 20,
                              tr(STR_ARCADE_REFRESH_AFTER_GAME));
  } else {
    const int totalClears = static_cast<int>(state.sudokuClears) + static_cast<int>(state.sokobanClears) +
                            static_cast<int>(state.memoryClears) + static_cast<int>(state.wordPuzzleClears) +
                            static_cast<int>(state.dailyMazeClears);

    char totalValue[24];
    snprintf(totalValue, sizeof(totalValue), tr(STR_ARCADE_CLEARS_FORMAT), totalClears);
    char bestValue[24];
    snprintf(bestValue, sizeof(bestValue), tr(STR_ARCADE_BEST_FORMAT), state.best2048);
    drawMetricCard(renderer, pad, contentTop, (pageWidth - pad * 2 - 10) / 2, 58, tr(STR_ARCADE_LIFETIME), totalValue);
    drawMetricCard(renderer, pageWidth / 2 + 5, contentTop, (pageWidth - pad * 2 - 10) / 2, 58,
                   tr(STR_ARCADE_2048_RECORD), bestValue);

    const ChallengeRow achievements[] = {
        {StrId::STR_ARCADE_PUZZLE_STARTER, StrId::STR_ARCADE_PUZZLE_STARTER_DESC, state.sudokuClears >= 1},
        {StrId::STR_ARCADE_BOX_MOVER, StrId::STR_ARCADE_BOX_MOVER_DESC, state.sokobanClears >= 1},
        {StrId::STR_ARCADE_MEMORY_KEEPER, StrId::STR_ARCADE_MEMORY_KEEPER_DESC, state.memoryClears >= 1},
        {StrId::STR_ARCADE_WORD_FINDER, StrId::STR_ARCADE_WORD_FINDER_DESC, state.wordPuzzleClears >= 1},
        {StrId::STR_ARCADE_MAZE_RUNNER, StrId::STR_ARCADE_MAZE_RUNNER_DESC, state.dailyMazeClears >= 1},
        {StrId::STR_ARCADE_REGULAR, StrId::STR_ARCADE_REGULAR_DESC, state.totalWins >= 10},
    };

    renderer.drawText(UI_10_FONT_ID, pad, contentTop + 74, tr(STR_ARCADE_ACHIEVEMENTS), true, EpdFontFamily::BOLD);
    for (int i = 0; i < 6; i++) {
      drawChallengeRow(renderer, pad, contentTop + 98 + i * 48, pageWidth - pad * 2, achievements[i]);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
