#include "ArcadeChallengesActivity.h"

#include <I18n.h>

#include <algorithm>

#include "ArcadeProgressStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
struct ChallengeRow {
  const char* title;
  const char* detail;
  bool completed;
};

void drawMetricCard(GfxRenderer& renderer, int x, int y, int w, int h, const char* label, const char* value) {
  renderer.drawRoundedRect(x, y, w, h, 1, 8, true);
  renderer.drawText(SMALL_FONT_ID, x + 12, y + 8, label);
  renderer.drawText(UI_10_FONT_ID, x + 12, y + h - 24, value, true, EpdFontFamily::BOLD);
}

void drawChallengeRow(GfxRenderer& renderer, int x, int y, int w, const ChallengeRow& row) {
  renderer.drawRoundedRect(x, y, w, 44, 1, 8, true);
  renderer.drawText(UI_10_FONT_ID, x + 14, y + 8, row.title, true, EpdFontFamily::BOLD);
  renderer.drawText(SMALL_FONT_ID, x + 14, y + 24, row.detail);
  renderer.drawText(UI_10_FONT_ID, x + w - 70, y + 12, row.completed ? "Done" : "Open", true,
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
  if (mappedInput.consumeTouchEvent(&touchEvent)) {
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

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Challenges");

  if (pageIndex == 0) {
    char streakValue[24];
    snprintf(streakValue, sizeof(streakValue), "%u days", state.playStreakDays);
    char sessionsValue[24];
    snprintf(sessionsValue, sizeof(sessionsValue), "%u today", state.sessionsToday);
    char winsValue[24];
    snprintf(winsValue, sizeof(winsValue), "%u today", state.winsToday);
    char tileValue[24];
    snprintf(tileValue, sizeof(tileValue), "%u max", state.dailyMax2048);

    const int cardW = (pageWidth - pad * 2 - 10) / 2;
    drawMetricCard(renderer, pad, contentTop, cardW, 58, "Play streak", streakValue);
    drawMetricCard(renderer, pad + cardW + 10, contentTop, cardW, 58, "Sessions", sessionsValue);
    drawMetricCard(renderer, pad, contentTop + 68, cardW, 58, "Wins", winsValue);
    drawMetricCard(renderer, pad + cardW + 10, contentTop + 68, cardW, 58, "2048 peak", tileValue);

    const ChallengeRow challenges[] = {
        {"Daily opener", "Play 2 arcade sessions today", state.sessionsToday >= 2},
        {"Sharp solver", "Clear any 2 puzzles today", state.winsToday >= 2},
        {"Tile climber", "Reach tile 128 in 2048", state.dailyMax2048 >= 128},
    };

    renderer.drawText(UI_10_FONT_ID, pad, contentTop + 142, "Daily missions", true, EpdFontFamily::BOLD);
    for (int i = 0; i < 3; i++) {
      drawChallengeRow(renderer, pad, contentTop + 166 + i * 52, pageWidth - pad * 2, challenges[i]);
    }

    renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - metrics.buttonHintsHeight - 20,
                              "Refresh after finishing a game");
  } else {
    const int totalClears = static_cast<int>(state.sudokuClears) + static_cast<int>(state.sokobanClears) +
                            static_cast<int>(state.memoryClears) + static_cast<int>(state.wordPuzzleClears) +
                            static_cast<int>(state.dailyMazeClears);

    char totalValue[24];
    snprintf(totalValue, sizeof(totalValue), "%d clears", totalClears);
    char bestValue[24];
    snprintf(bestValue, sizeof(bestValue), "%u best", state.best2048);
    drawMetricCard(renderer, pad, contentTop, (pageWidth - pad * 2 - 10) / 2, 58, "Lifetime", totalValue);
    drawMetricCard(renderer, pageWidth / 2 + 5, contentTop, (pageWidth - pad * 2 - 10) / 2, 58, "2048 record",
                   bestValue);

    const ChallengeRow achievements[] = {
        {"Puzzle starter", "Solve Sudoku once", state.sudokuClears >= 1},
        {"Box mover", "Clear Sokoban once", state.sokobanClears >= 1},
        {"Memory keeper", "Finish Memory once", state.memoryClears >= 1},
        {"Word finder", "Finish Word Puzzle once", state.wordPuzzleClears >= 1},
        {"Maze runner", "Clear Daily Maze once", state.dailyMazeClears >= 1},
        {"Arcade regular", "Win 10 total challenges", state.totalWins >= 10},
    };

    renderer.drawText(UI_10_FONT_ID, pad, contentTop + 74, "Achievements", true, EpdFontFamily::BOLD);
    for (int i = 0; i < 6; i++) {
      drawChallengeRow(renderer, pad, contentTop + 98 + i * 48, pageWidth - pad * 2, achievements[i]);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Refresh", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
