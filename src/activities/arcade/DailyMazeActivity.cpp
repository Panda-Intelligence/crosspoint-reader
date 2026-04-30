#include "DailyMazeActivity.h"

#include <I18n.h>

#include <array>

#include "ArcadeProgressStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr std::array<const char*, DailyMazeActivity::kRows> kMazeTemplate = {
    "#########", "#S#.....#", "#.#.###.#", "#.#...#.#", "#.###.#.#", "#...#.#.#", "###.#.#.#", "#....#..E", "#########",
};
}  // namespace

void DailyMazeActivity::loadMaze() {
  mazeRows.clear();
  mazeRows.reserve(kRows);

  playerPos = {1, 1};
  exitPos = {kRows - 2, kCols - 2};
  stepCount = 0;
  completed = false;

  for (int row = 0; row < kRows; row++) {
    std::string line = kMazeTemplate[row];
    for (int col = 0; col < kCols; col++) {
      if (line[col] == 'S') {
        playerPos = {row, col};
        line[col] = '.';
      } else if (line[col] == 'E') {
        exitPos = {row, col};
        line[col] = '.';
      }
    }
    mazeRows.push_back(line);
  }
}

bool DailyMazeActivity::canMoveTo(const int row, const int col) const {
  if (row < 0 || row >= kRows || col < 0 || col >= kCols) {
    return false;
  }
  if (mazeRows.empty()) {
    return false;
  }
  return mazeRows[row][col] != '#';
}

void DailyMazeActivity::movePlayer(const int rowDelta, const int colDelta) {
  if (completed) {
    return;
  }

  const int nextRow = playerPos.row + rowDelta;
  const int nextCol = playerPos.col + colDelta;
  if (!canMoveTo(nextRow, nextCol)) {
    return;
  }

  playerPos = {nextRow, nextCol};
  stepCount++;
  if (playerPos.row == exitPos.row && playerPos.col == exitPos.col) {
    completed = true;
    ARCADE_PROGRESS.recordWin(ArcadeGameId::DailyMaze);
  }
  requestUpdate();
}

void DailyMazeActivity::onEnter() {
  Activity::onEnter();
  ARCADE_PROGRESS.loadFromFile();
  ARCADE_PROGRESS.recordSessionStart();
  loadMaze();
  requestUpdate();
}

void DailyMazeActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      mappedInput.suppressTouchButtonFallback();
      if (completed) {
        loadMaze();
        requestUpdate();
        return;
      }
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int pageWidth = renderer.getScreenWidth();
      const int pageHeight = renderer.getScreenHeight();
      const int gridTop = metrics.topPadding + metrics.headerHeight + 16;
      const int gridBottom = pageHeight - metrics.buttonHintsHeight - 18;
      const int cell = std::min((gridBottom - gridTop) / kRows, (pageWidth - 80) / kCols);
      const int startX = (pageWidth - cell * kCols) / 2;
      int row = 0;
      int col = 0;
      if (TouchHitTest::gridCellAt(Rect{startX, gridTop, cell * kCols, cell * kRows}, kRows, kCols, touchEvent.x,
                                   touchEvent.y, &row, &col)) {
        const int rowDelta = row - playerPos.row;
        const int colDelta = col - playerPos.col;
        if (std::abs(rowDelta) > std::abs(colDelta)) {
          movePlayer(rowDelta > 0 ? 1 : -1, 0);
        } else if (colDelta != 0) {
          movePlayer(0, colDelta > 0 ? 1 : -1);
        }
        return;
      }
    } else if (!buttonHintTap) {
      mappedInput.suppressTouchButtonFallback();
      if (touchEvent.type == InputTouchEvent::Type::SwipeLeft) {
        movePlayer(0, -1);
      } else if (touchEvent.type == InputTouchEvent::Type::SwipeRight) {
        movePlayer(0, 1);
      } else if (touchEvent.type == InputTouchEvent::Type::SwipeUp) {
        movePlayer(-1, 0);
      } else if (touchEvent.type == InputTouchEvent::Type::SwipeDown) {
        movePlayer(1, 0);
      }
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    loadMaze();
    requestUpdate();
    return;
  }

  buttonNavigator.onPreviousRelease([this] { movePlayer(-1, 0); });
  buttonNavigator.onNextRelease([this] { movePlayer(1, 0); });

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    movePlayer(0, -1);
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    movePlayer(0, 1);
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    movePlayer(-1, 0);
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    movePlayer(1, 0);
  }
}

void DailyMazeActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Daily Maze");

  const int gridTop = metrics.topPadding + metrics.headerHeight + 16;
  const int gridBottom = pageHeight - metrics.buttonHintsHeight - 18;
  const int gridHeight = gridBottom - gridTop;
  const int cell = std::min(gridHeight / kRows, (pageWidth - 80) / kCols);
  const int gridWidth = cell * kCols;
  const int startX = (pageWidth - gridWidth) / 2;

  constexpr int kWallRadius = 3;

  for (int row = 0; row < kRows; row++) {
    for (int col = 0; col < kCols; col++) {
      const int x = startX + col * cell;
      const int y = gridTop + row * cell;

      if (mazeRows[row][col] == '#') {
        // Wall: solid rounded rect
        renderer.fillRoundedRect(x + 1, y + 1, cell - 2, cell - 2, kWallRadius, Color::Black);
      } else {
        // Floor: faint outline
        renderer.drawRoundedRect(x + 1, y + 1, cell - 2, cell - 2, 1, kWallRadius, true);
      }

      if (row == exitPos.row && col == exitPos.col) {
        // Exit: bold filled square marker
        const int m = std::max(3, cell / 4);
        renderer.fillRoundedRect(x + (cell - m) / 2, y + (cell - m) / 2, m, m, 2, Color::Black);
      }

      if (row == playerPos.row && col == playerPos.col) {
        // Player: filled circle (white on black floor)
        const int pr = (cell - 6) / 2;
        renderer.fillRoundedRect(x + cell / 2 - pr, y + cell / 2 - pr, pr * 2, pr * 2, pr, Color::Black);
        // White inner dot so player stands out against dark walls in debug views
        renderer.fillRoundedRect(x + cell / 2 - pr / 2, y + cell / 2 - pr / 2, pr, pr, pr / 2, Color::White);
      }
    }
  }

  char status[48];
  snprintf(status, sizeof(status), "Steps: %d", stepCount);
  renderer.drawCenteredText(UI_10_FONT_ID, gridBottom + 2, status);

  if (completed) {
    renderer.drawCenteredText(UI_10_FONT_ID, gridBottom + 30, "Maze cleared! Press Confirm to restart");
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, gridBottom + 30, "Use arrows to move to the exit");
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Restart", "Left/Up", "Right/Down");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
