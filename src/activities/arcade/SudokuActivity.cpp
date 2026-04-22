#include "SudokuActivity.h"

#include <I18n.h>

#include <algorithm>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr SudokuActivity::Board kPuzzle = {{
    {{5, 3, 0, 0, 7, 0, 0, 0, 0}},
    {{6, 0, 0, 1, 9, 5, 0, 0, 0}},
    {{0, 9, 8, 0, 0, 0, 0, 6, 0}},
    {{8, 0, 0, 0, 6, 0, 0, 0, 3}},
    {{4, 0, 0, 8, 0, 3, 0, 0, 1}},
    {{7, 0, 0, 0, 2, 0, 0, 0, 6}},
    {{0, 6, 0, 0, 0, 0, 2, 8, 0}},
    {{0, 0, 0, 4, 1, 9, 0, 0, 5}},
    {{0, 0, 0, 0, 8, 0, 0, 7, 9}},
}};
}  // namespace

void SudokuActivity::loadPuzzle() {
  given = kPuzzle;
  board = kPuzzle;
  cursorRow = 0;
  cursorCol = 0;
  refreshStatus();
}

bool SudokuActivity::isEditable(const int row, const int col) const { return given[row][col] == 0; }

bool SudokuActivity::isConflictAt(const int row, const int col) const {
  const uint8_t value = board[row][col];
  if (value == 0) {
    return false;
  }

  for (int c = 0; c < kSize; c++) {
    if (c != col && board[row][c] == value) {
      return true;
    }
  }

  for (int r = 0; r < kSize; r++) {
    if (r != row && board[r][col] == value) {
      return true;
    }
  }

  const int boxRow = (row / 3) * 3;
  const int boxCol = (col / 3) * 3;
  for (int r = boxRow; r < boxRow + 3; r++) {
    for (int c = boxCol; c < boxCol + 3; c++) {
      if ((r != row || c != col) && board[r][c] == value) {
        return true;
      }
    }
  }

  return false;
}

void SudokuActivity::refreshStatus() {
  hasConflicts = false;
  completed = true;

  for (int r = 0; r < kSize; r++) {
    for (int c = 0; c < kSize; c++) {
      if (board[r][c] == 0) {
        completed = false;
        continue;
      }
      if (isConflictAt(r, c)) {
        hasConflicts = true;
      }
    }
  }

  if (hasConflicts) {
    completed = false;
  }
}

void SudokuActivity::moveCursor(const int rowDelta, const int colDelta) {
  cursorRow = (cursorRow + rowDelta + kSize) % kSize;
  cursorCol = (cursorCol + colDelta + kSize) % kSize;
  requestUpdate();
}

void SudokuActivity::cycleCellValue() {
  if (!isEditable(cursorRow, cursorCol)) {
    return;
  }

  uint8_t& cell = board[cursorRow][cursorCol];
  cell = (cell >= 9) ? 0 : static_cast<uint8_t>(cell + 1);
  refreshStatus();
  requestUpdate();
}

void SudokuActivity::onEnter() {
  Activity::onEnter();
  loadPuzzle();
  requestUpdate();
}

void SudokuActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (completed) {
      loadPuzzle();
    } else {
      cycleCellValue();
    }
    requestUpdate();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    moveCursor(0, -1);
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    moveCursor(0, 1);
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    moveCursor(-1, 0);
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    moveCursor(1, 0);
  }
}

void SudokuActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Sudoku");

  const int gridTop = metrics.topPadding + metrics.headerHeight + 12;
  const int gridBottom = pageHeight - metrics.buttonHintsHeight - 28;
  const int gridHeight = gridBottom - gridTop;
  const int cell = std::min(gridHeight / kSize, (pageWidth - 60) / kSize);
  const int boardSize = cell * kSize;
  const int startX = (pageWidth - boardSize) / 2;
  const int startY = gridTop;

  renderer.drawRect(startX, startY, boardSize, boardSize, 2, true);

  for (int r = 0; r < kSize; r++) {
    for (int c = 0; c < kSize; c++) {
      const int x = startX + c * cell;
      const int y = startY + r * cell;
      const bool cursor = (r == cursorRow && c == cursorCol);
      const bool conflict = isConflictAt(r, c);

      renderer.drawRect(x, y, cell, cell, true);
      if (cursor) {
        renderer.drawRect(x + 1, y + 1, cell - 2, cell - 2, true);
      }

      const uint8_t value = board[r][c];
      if (value != 0) {
        char text[2] = {static_cast<char>('0' + value), '\0'};
        if (conflict) {
          renderer.fillRect(x + 4, y + 4, cell - 8, cell - 8, true);
          renderer.drawCenteredText(UI_10_FONT_ID, y + (cell / 2) - 8, text, false, EpdFontFamily::BOLD);
        } else {
          renderer.drawCenteredText(UI_10_FONT_ID, y + (cell / 2) - 8, text, true,
                                    isEditable(r, c) ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD);
        }
      }
    }
  }

  for (int i = 3; i < kSize; i += 3) {
    const int pos = startX + i * cell;
    renderer.drawLine(pos, startY, pos, startY + boardSize, 2, true);
    const int rowPos = startY + i * cell;
    renderer.drawLine(startX, rowPos, startX + boardSize, rowPos, 2, true);
  }

  if (completed) {
    renderer.drawCenteredText(UI_10_FONT_ID, gridBottom + 4, "Solved! Press Confirm for new game");
  } else if (hasConflicts) {
    renderer.drawCenteredText(UI_10_FONT_ID, gridBottom + 4, "Conflict exists in highlighted cells");
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, gridBottom + 4, "Move cursor and press Confirm to fill");
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), completed ? "Restart" : "Set", "Left/Up", "Right/Down");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
