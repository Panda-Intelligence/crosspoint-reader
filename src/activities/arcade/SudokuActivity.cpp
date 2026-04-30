#include "SudokuActivity.h"

#include <I18n.h>

#include <algorithm>

#include "ArcadeProgressStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

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
  hasConflicts = false;
  completed = false;
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
  ARCADE_PROGRESS.loadFromFile();
  ARCADE_PROGRESS.recordSessionStart();
  loadPuzzle();
  requestUpdate();
}

void SudokuActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      mappedInput.suppressTouchButtonFallback();
      if (completed) {
        loadPuzzle();
        requestUpdate();
        return;
      }
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int pageWidth = renderer.getScreenWidth();
      const int pageHeight = renderer.getScreenHeight();
      const int gridTop = metrics.topPadding + metrics.headerHeight + 12;
      const int gridBottom = pageHeight - metrics.buttonHintsHeight - 28;
      const int cell = std::min((gridBottom - gridTop) / kSize, (pageWidth - 60) / kSize);
      const int startX = (pageWidth - cell * kSize) / 2;
      int row = 0;
      int col = 0;
      if (TouchHitTest::gridCellAt(Rect{startX, gridTop, cell * kSize, cell * kSize}, kSize, kSize, touchEvent.x,
                                   touchEvent.y, &row, &col)) {
        cursorRow = row;
        cursorCol = col;
        cycleCellValue();
        if (completed) {
          ARCADE_PROGRESS.recordWin(ArcadeGameId::Sudoku);
        }
        requestUpdate();
        return;
      }
    } else if (!buttonHintTap) {
      mappedInput.suppressTouchButtonFallback();
      if (touchEvent.type == InputTouchEvent::Type::SwipeLeft) {
        moveCursor(0, -1);
      } else if (touchEvent.type == InputTouchEvent::Type::SwipeRight) {
        moveCursor(0, 1);
      } else if (touchEvent.type == InputTouchEvent::Type::SwipeUp) {
        moveCursor(-1, 0);
      } else if (touchEvent.type == InputTouchEvent::Type::SwipeDown) {
        moveCursor(1, 0);
      }
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (completed) {
      loadPuzzle();
    } else {
      cycleCellValue();
      if (completed) {
        ARCADE_PROGRESS.recordWin(ArcadeGameId::Sudoku);
      }
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

  // Rounded outer board border
  renderer.drawRoundedRect(startX - 2, startY - 2, boardSize + 4, boardSize + 4, 2, 6, true);

  for (int r = 0; r < kSize; r++) {
    for (int c = 0; c < kSize; c++) {
      const int x = startX + c * cell;
      const int y = startY + r * cell;
      const bool cursor = (r == cursorRow && c == cursorCol);
      const bool conflict = isConflictAt(r, c);
      const bool editable = isEditable(r, c);
      constexpr int kCellPad = 2;
      constexpr int kCellRadius = 4;
      const int cx = x + kCellPad;
      const int cy = y + kCellPad;
      const int cw = cell - kCellPad * 2;
      const int ch = cell - kCellPad * 2;

      if (conflict) {
        // Conflict: Black fill, white text
        renderer.fillRoundedRect(cx, cy, cw, ch, kCellRadius, Color::Black);
      } else if (cursor) {
        // Cursor: DarkGray fill
        renderer.fillRoundedRect(cx, cy, cw, ch, kCellRadius, Color::DarkGray);
        renderer.drawRoundedRect(cx, cy, cw, ch, 1, kCellRadius, true);
      } else {
        renderer.drawRoundedRect(cx, cy, cw, ch, 1, kCellRadius, true);
      }

      const uint8_t value = board[r][c];
      if (value != 0) {
        const char text[2] = {static_cast<char>('0' + value), '\0'};
        const int textY = y + (cell / 2) - 8;
        if (conflict || cursor) {
          // Inverted text for cursor/conflict
          renderer.drawCenteredText(UI_10_FONT_ID, textY, text, false, EpdFontFamily::BOLD);
        } else {
          renderer.drawCenteredText(UI_10_FONT_ID, textY, text, true,
                                    editable ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD);
        }
      }
    }
  }

  // Thick rounded 3x3 box dividers drawn on top
  for (int i = 3; i < kSize; i += 3) {
    const int vx = startX + i * cell - 1;
    renderer.drawLine(vx, startY, vx, startY + boardSize, 2, true);
    renderer.drawLine(vx + 1, startY, vx + 1, startY + boardSize, 1, true);
    const int hy = startY + i * cell - 1;
    renderer.drawLine(startX, hy, startX + boardSize, hy, 2, true);
    renderer.drawLine(startX, hy + 1, startX + boardSize, hy + 1, 1, true);
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
