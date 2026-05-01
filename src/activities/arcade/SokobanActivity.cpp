#include "SokobanActivity.h"

#include <I18n.h>

#include <algorithm>
#include <array>

#include "ArcadeProgressStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr const char* kLevelTemplate[SokobanActivity::kRows] = {
    "  ####  ", "###  ###", "#     $#", "# #  #$#", "# . .@ #", "########", "        ", "        ",
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
  ARCADE_PROGRESS.recordWin(ArcadeGameId::Sokoban);
}

void SokobanActivity::onEnter() {
  Activity::onEnter();
  ARCADE_PROGRESS.loadFromFile();
  ARCADE_PROGRESS.recordSessionStart();
  loadLevel();
  requestUpdate();
}

void SokobanActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      mappedInput.suppressTouchButtonFallback();
      if (completed) {
        loadLevel();
        requestUpdate();
        return;
      }
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int pageWidth = renderer.getScreenWidth();
      const int pageHeight = renderer.getScreenHeight();
      const int gridTop = metrics.topPadding + metrics.headerHeight + 20;
      const int gridBottom = pageHeight - metrics.buttonHintsHeight - 20;
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

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_ARCADE_GAME_SOKOBAN));

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
          renderer.fillRoundedRect(x + 1, y + 1, cell - 2, cell - 2, 4, Color::Black);
          break;
        case Tile::Target:
          renderer.drawRoundedRect(x + 2, y + 2, cell - 4, cell - 4, 1, 4, true);
          renderer.fillRoundedRect(x + cell / 2 - 4, y + cell / 2 - 4, 8, 8, 4, Color::DarkGray);
          break;
        case Tile::Box: {
          renderer.fillRoundedRect(x + 3, y + 3, cell - 6, cell - 6, 5, Color::LightGray);
          renderer.drawRoundedRect(x + 3, y + 3, cell - 6, cell - 6, 1, 5, true);
          const int bm = 6;
          renderer.drawLine(x + bm, y + bm, x + cell - bm, y + cell - bm, 1, true);
          renderer.drawLine(x + cell - bm, y + bm, x + bm, y + cell - bm, 1, true);
          break;
        }
        case Tile::BoxOnTarget:
          renderer.fillRoundedRect(x + 3, y + 3, cell - 6, cell - 6, 5, Color::Black);
          renderer.drawRoundedRect(x + 5, y + 5, cell - 10, cell - 10, 1, 4, false);
          break;
        case Tile::Player:
        case Tile::PlayerOnTarget: {
          const int pr = (cell - 8) / 2;
          renderer.fillRoundedRect(x + cell / 2 - pr, y + cell / 2 - pr, pr * 2, pr * 2, pr, Color::Black);
          if (board[r][c] == Tile::PlayerOnTarget) {
            renderer.fillRoundedRect(x + cell / 2 - pr / 2, y + cell / 2 - pr / 2, pr, pr, pr / 2, Color::White);
          }
          break;
        }
        case Tile::Floor:
        default:
          // Just empty space
          break;
      }
    }
  }

  char status[32];
  snprintf(status, sizeof(status), tr(STR_ARCADE_MOVES_FORMAT), moveCount);
  renderer.drawCenteredText(UI_10_FONT_ID, gridBottom + 4, status, true, EpdFontFamily::BOLD);

  if (completed) {
    renderer.drawCenteredText(UI_10_FONT_ID, gridBottom + 30, tr(STR_ARCADE_SOKOBAN_COMPLETE));
  } else {
    renderer.drawCenteredText(SMALL_FONT_ID, gridBottom + 30, tr(STR_ARCADE_SOKOBAN_MOVE_HINT));
  }

  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), tr(STR_ARCADE_RESTART), tr(STR_ARCADE_LEFT_UP), tr(STR_ARCADE_RIGHT_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
