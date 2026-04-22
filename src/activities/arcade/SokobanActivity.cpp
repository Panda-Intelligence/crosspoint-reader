#include "SokobanActivity.h"

#include <I18n.h>

#include <algorithm>
#include <array>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr const char* kLevelTemplate[SokobanActivity::kRows] = {
    "  ####  ",
    "###  ###",
    "#     $#",
    "# #  #$#",
    "# . .@ #",
    "########",
    "        ",
    "        ",
};
}  // namespace

void SokobanActivity::loadLevel() {
  board.assign(kRows, std::vector<Tile>(kCols, Tile::Floor));
  moveCount = 0;
  completed = false;

  for (int r = 0; r < kRows; r++) {
    const std::string line = kLevelTemplate[r];
    for (int c = 0; c < kCols; c++) {
      const char ch = line[c];
      board[r][c] = static_cast<Tile>(ch);
      if (ch == '@' || ch == '+') {
        playerPos = {r, c};
      }
    }
  }
}

bool SokobanActivity::isWall(int r, int c) const {
  if (r < 0 || r >= kRows || c < 0 || c >= kCols) return true;
  return board[r][c] == Tile::Wall;
}

bool SokobanActivity::isBox(int r, int c) const {
  if (r < 0 || r >= kRows || c < 0 || c >= kCols) return false;
  return board[r][c] == Tile::Box || board[r][c] == Tile::BoxOnTarget;
}

bool SokobanActivity::isTarget(int r, int c) const {
  if (r < 0 || r >= kRows || c < 0 || c >= kCols) return false;
  return board[r][c] == Tile::Target || board[r][c] == Tile::BoxOnTarget || board[r][c] == Tile::PlayerOnTarget;
}

void SokobanActivity::movePlayer(int dr, int dc) {
  if (completed) return;

  const int nr = playerPos.row + dr;
  const int nc = playerPos.col + dc;

  if (isWall(nr, nc)) return;

  if (isBox(nr, nc)) {
    const int br = nr + dr;
    const int bc = nc + dc;
    if (isWall(br, bc) || isBox(br, bc)) return;

    // Move box
    board[br][bc] = isTarget(br, bc) ? Tile::BoxOnTarget : Tile::Box;
  }

  // Move player
  board[playerPos.row][playerPos.col] = isTarget(playerPos.row, playerPos.col) ? Tile::Target : Tile::Floor;
  playerPos = {nr, nc};
  board[nr][nc] = isTarget(nr, nc) ? Tile::PlayerOnTarget : Tile::Player;

  moveCount++;
  checkWin();
  requestUpdate();
}

void SokobanActivity::checkWin() {
  for (int r = 0; r < kRows; r++) {
    for (int c = 0; c < kCols; c++) {
      if (board[r][c] == Tile::Box) {
        return;
      }
    }
  }
  completed = true;
}

void SokobanActivity::onEnter() {
  Activity::onEnter();
  loadLevel();
  requestUpdate();
}

void SokobanActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    loadLevel();
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

void SokobanActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Sokoban");

  const int gridTop = metrics.topPadding + metrics.headerHeight + 20;
  const int gridBottom = pageHeight - metrics.buttonHintsHeight - 20;
  const int gridHeight = gridBottom - gridTop;
  const int cell = std::min(gridHeight / kRows, (pageWidth - 80) / kCols);
  const int gridWidth = cell * kCols;
  const int startX = (pageWidth - gridWidth) / 2;

  for (int r = 0; r < kRows; r++) {
    for (int c = 0; c < kCols; c++) {
      const int x = startX + c * cell;
      const int y = gridTop + r * cell;

      switch (board[r][c]) {
        case Tile::Wall:
          renderer.fillRect(x, y, cell, cell, true);
          break;
        case Tile::Target:
          renderer.drawRect(x, y, cell, cell, true);
          renderer.fillRect(x + cell / 3, y + cell / 3, cell / 3, cell / 3, true);
          break;
        case Tile::Box:
          renderer.drawRect(x + 2, y + 2, cell - 4, cell - 4, 2, true);
          renderer.drawLine(x + 2, y + 2, x + cell - 2, y + cell - 2, 1, true);
          renderer.drawLine(x + cell - 2, y + 2, x + 2, y + cell - 2, 1, true);
          break;
        case Tile::BoxOnTarget:
          renderer.fillRect(x + 2, y + 2, cell - 4, cell - 4, true);
          renderer.drawRect(x + 4, y + 4, cell - 8, cell - 8, false);
          break;
        case Tile::Player:
        case Tile::PlayerOnTarget:
          renderer.drawRect(x, y, cell, cell, true);
          renderer.fillRect(x + 4, y + 4, cell - 8, cell - 8, true);
          if (board[r][c] == Tile::PlayerOnTarget) {
            renderer.drawRect(x + 6, y + 6, cell - 12, cell - 12, false);
          }
          break;
        case Tile::Floor:
        default:
          // Just empty space
          break;
      }
    }
  }

  char status[48];
  snprintf(status, sizeof(status), "Moves: %d", moveCount);
  renderer.drawCenteredText(UI_10_FONT_ID, gridBottom + 4, status);

  if (completed) {
    renderer.drawCenteredText(UI_10_FONT_ID, gridBottom + 32, "All boxes placed! Press Confirm to restart");
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, gridBottom + 32, "Push all $ to . positions");
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Restart", "Left/Up", "Right/Down");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
