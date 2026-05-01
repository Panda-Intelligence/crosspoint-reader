#include "ArcadeHubActivity.h"

#include <I18n.h>

#include <algorithm>
#include <array>
#include <memory>

#include "ArcadeChallengesActivity.h"
#include "DailyMazeActivity.h"
#include "FortuneActivity.h"
#include "Game2048Activity.h"
#include "MemoryGameActivity.h"
#include "SokobanActivity.h"
#include "SudokuActivity.h"
#include "WordPuzzleActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
using GameEntry = ArcadeHubActivity::GameEntry;

constexpr std::array<GameEntry, 8> kEntries{{
    {StrId::STR_ARCADE_GAME_2048, StrId::STR_ARCADE_GAME_2048_DESC, StrId::STR_ARCADE_GAME_2048_CONTROLS},
    {StrId::STR_ARCADE_GAME_SUDOKU, StrId::STR_ARCADE_GAME_SUDOKU_DESC, StrId::STR_ARCADE_GAME_SUDOKU_CONTROLS},
    {StrId::STR_ARCADE_GAME_SOKOBAN, StrId::STR_ARCADE_GAME_SOKOBAN_DESC, StrId::STR_ARCADE_GAME_SOKOBAN_CONTROLS},
    {StrId::STR_ARCADE_GAME_MEMORY, StrId::STR_ARCADE_GAME_MEMORY_DESC, StrId::STR_ARCADE_GAME_MEMORY_CONTROLS},
    {StrId::STR_ARCADE_GAME_WORD, StrId::STR_ARCADE_GAME_WORD_DESC, StrId::STR_ARCADE_GAME_WORD_CONTROLS},
    {StrId::STR_ARCADE_GAME_MAZE, StrId::STR_ARCADE_GAME_MAZE_DESC, StrId::STR_ARCADE_GAME_MAZE_CONTROLS},
    {StrId::STR_ARCADE_GAME_CHALLENGES, StrId::STR_ARCADE_GAME_CHALLENGES_DESC,
     StrId::STR_ARCADE_GAME_CHALLENGES_CONTROLS},
    {StrId::STR_FORTUNE_TITLE, StrId::STR_ARCADE_GAME_FORTUNE_DESC, StrId::STR_ARCADE_GAME_FORTUNE_CONTROLS},
}};
}  // namespace

const ArcadeHubActivity::GameEntry& ArcadeHubActivity::entry(const int index) {
  return kEntries[std::clamp(index, 0, static_cast<int>(kEntries.size()) - 1)];
}

int ArcadeHubActivity::itemCount() { return static_cast<int>(kEntries.size()); }

void ArcadeHubActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  showingDetail = false;
  requestUpdate();
}

void ArcadeHubActivity::openCurrentSelection() {
  if (selectedIndex == 0) {
    activityManager.replaceActivity(std::make_unique<Game2048Activity>(renderer, mappedInput));
  } else if (selectedIndex == 1) {
    activityManager.replaceActivity(std::make_unique<SudokuActivity>(renderer, mappedInput));
  } else if (selectedIndex == 2) {
    activityManager.replaceActivity(std::make_unique<SokobanActivity>(renderer, mappedInput));
  } else if (selectedIndex == 3) {
    activityManager.replaceActivity(std::make_unique<MemoryGameActivity>(renderer, mappedInput));
  } else if (selectedIndex == 4) {
    activityManager.replaceActivity(std::make_unique<WordPuzzleActivity>(renderer, mappedInput));
  } else if (selectedIndex == 5) {
    activityManager.replaceActivity(std::make_unique<DailyMazeActivity>(renderer, mappedInput));
  } else if (selectedIndex == 6) {
    activityManager.replaceActivity(std::make_unique<ArcadeChallengesActivity>(renderer, mappedInput));
  } else if (selectedIndex == 7) {
    activityManager.replaceActivity(std::make_unique<FortuneActivity>(renderer, mappedInput));
  } else {
    showingDetail = true;
    requestUpdate();
  }
}

void ArcadeHubActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (showingDetail) {
      if (touchEvent.isTap()) {
        mappedInput.suppressTouchButtonFallback();
        showingDetail = false;
        requestUpdate();
        return;
      }
      return;
    }

    if (touchEvent.isTap()) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const Rect listRect{
          0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, renderer.getScreenWidth(),
          renderer.getScreenHeight() -
              (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)};
      const int clickedIndex = TouchHitTest::listItemAt(listRect, metrics.listWithSubtitleRowHeight, selectedIndex,
                                                        itemCount(), touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        openCurrentSelection();
        return;
      }
    } else {
      const auto gestureAction = TouchHitTest::listGestureActionForTouch(touchEvent);
      if (gestureAction == TouchHitTest::ListGestureAction::NextItem) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount());
        requestUpdate();
        return;
      }
      if (gestureAction == TouchHitTest::ListGestureAction::PreviousItem) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount());
        requestUpdate();
        return;
      }
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (showingDetail) {
      showingDetail = false;
      requestUpdate();
      return;
    }
    finish();
    return;
  }

  if (showingDetail) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      showingDetail = false;
      requestUpdate();
    }
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount());
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount());
    requestUpdate();
  });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    openCurrentSelection();
  }
}

void ArcadeHubActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Arcade");

  if (showingDetail) {
    const auto& selected = entry(selectedIndex);
    const int startY = metrics.topPadding + metrics.headerHeight + 24;
    renderer.drawCenteredText(UI_12_FONT_ID, startY, I18N.get(selected.title), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, startY + 42, I18N.get(selected.description));
    renderer.drawCenteredText(UI_10_FONT_ID, startY + 78, I18N.get(selected.controls));
    renderer.drawCenteredText(SMALL_FONT_ID, startY + 122, tr(STR_ARCADE_DETAIL_HINT));
  } else {
    GUI.drawList(
        renderer,
        Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
             pageHeight -
                 (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)},
        itemCount(), selectedIndex, [](int index) { return std::string(I18N.get(kEntries[index].title)); },
        [](int index) { return std::string(I18N.get(kEntries[index].description)); });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), showingDetail ? tr(STR_BACK) : tr(STR_OPEN), tr(STR_DIR_UP),
                                            tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
