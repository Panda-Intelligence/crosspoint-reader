#include "DailyMazeActivity.h"

#include <I18n.h>

#include <array>

#include "components/UITheme.h"
#include "fontIds.h"

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
  }
  requestUpdate();
}

void DailyMazeActivity::onEnter() {
  Activity::onEnter();
  loadMaze();
  requestUpdate();
}

void DailyMazeActivity::loop() {
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

  for (int row = 0; row < kRows; row++) {
    for (int col = 0; col < kCols; col++) {
      const int x = startX + col * cell;
      const int y = gridTop + row * cell;

      if (mazeRows[row][col] == '#') {
        renderer.fillRect(x, y, cell, cell, true);
      } else {
        renderer.drawRect(x, y, cell, cell, true);
      }

      if (row == exitPos.row && col == exitPos.col) {
        renderer.fillRect(x + cell / 3, y + cell / 3, std::max(2, cell / 3), std::max(2, cell / 3), true);
      }

      if (row == playerPos.row && col == playerPos.col) {
        renderer.fillRect(x + 2, y + 2, cell - 4, cell - 4, false);
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
