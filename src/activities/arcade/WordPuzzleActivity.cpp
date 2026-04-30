#include "WordPuzzleActivity.h"

#include <I18n.h>

#include <algorithm>
#include <array>
#include <cstring>

#include "ArcadeProgressStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
struct PuzzleDefinition {
  WordPuzzleActivity::Grid board;
  std::array<const char*, WordPuzzleActivity::kWordCount> words;
};

constexpr std::array<PuzzleDefinition, WordPuzzleActivity::kPuzzleCount> kPuzzles{{
    {{{{'B', 'O', 'O', 'K'}, {'P', 'A', 'G', 'E'}, {'R', 'E', 'A', 'D'}, {'N', 'O', 'T', 'E'}}},
     {"BOOK", "PAGE", "READ", "NOTE"}},
    {{{{'C', 'A', 'R', 'D'}, {'P', 'L', 'A', 'N'}, {'G', 'O', 'A', 'L'}, {'T', 'A', 'S', 'K'}}},
     {"CARD", "PLAN", "GOAL", "TASK"}},
    {{{{'T', 'I', 'M', 'E'}, {'W', 'O', 'R', 'K'}, {'D', 'A', 'Y', 'S'}, {'D', 'E', 'S', 'K'}}},
     {"TIME", "WORK", "DAYS", "DESK"}},
}};
}  // namespace

void WordPuzzleActivity::loadPuzzle(const int index) {
  puzzleIndex = ((index % kPuzzleCount) + kPuzzleCount) % kPuzzleCount;
  board = kPuzzles[puzzleIndex].board;
  for (int i = 0; i < kWordCount; i++) {
    targetWords[i] = kPuzzles[puzzleIndex].words[i];
  }
  foundWords.fill(false);
  foundCount = 0;
  completed = false;
  cursor = {0, 0};
  clearSelection();
  statusMessage = "Select letters to find all words";
}

void WordPuzzleActivity::moveCursor(const int rowDelta, const int colDelta) {
  cursor.row = (cursor.row + rowDelta + kSize) % kSize;
  cursor.col = (cursor.col + colDelta + kSize) % kSize;
  requestUpdate();
}

bool WordPuzzleActivity::isSelected(const int row, const int col) const {
  return std::any_of(selectedPath.begin(), selectedPath.end(),
                     [row, col](const CellPos& pos) { return pos.row == row && pos.col == col; });
}

bool WordPuzzleActivity::isAdjacentToCurrentPath(const int row, const int col) const {
  if (selectedPath.empty()) {
    return true;
  }

  const auto& last = selectedPath.back();
  const int rowDistance = std::abs(last.row - row);
  const int colDistance = std::abs(last.col - col);
  return rowDistance + colDistance == 1;
}

bool WordPuzzleActivity::isPrefixOfRemainingWord(const std::string& value) const {
  for (int i = 0; i < kWordCount; i++) {
    if (foundWords[i]) {
      continue;
    }
    if (targetWords[i].rfind(value, 0) == 0) {
      return true;
    }
  }
  return false;
}

int WordPuzzleActivity::matchedWordIndex(const std::string& value) const {
  for (int i = 0; i < kWordCount; i++) {
    if (!foundWords[i] && targetWords[i] == value) {
      return i;
    }
  }
  return -1;
}

void WordPuzzleActivity::clearSelection() {
  selectedPath.clear();
  currentWord.clear();
}

void WordPuzzleActivity::selectCurrentCell() {
  if (completed) {
    loadPuzzle(puzzleIndex + 1);
    requestUpdate();
    return;
  }

  if (isSelected(cursor.row, cursor.col)) {
    if (!selectedPath.empty() && selectedPath.back().row == cursor.row && selectedPath.back().col == cursor.col) {
      selectedPath.pop_back();
      if (!currentWord.empty()) {
        currentWord.pop_back();
      }
      statusMessage = currentWord.empty() ? "Selection cleared" : currentWord;
      requestUpdate();
    }
    return;
  }

  if (!isAdjacentToCurrentPath(cursor.row, cursor.col)) {
    statusMessage = "Pick a neighboring letter";
    requestUpdate();
    return;
  }

  const char nextChar = board[cursor.row][cursor.col];
  const std::string candidate = currentWord + nextChar;
  if (!isPrefixOfRemainingWord(candidate)) {
    statusMessage = "No target word starts with that path";
    requestUpdate();
    return;
  }

  selectedPath.push_back(cursor);
  currentWord = candidate;

  const int matchedIndex = matchedWordIndex(currentWord);
  if (matchedIndex >= 0) {
    foundWords[matchedIndex] = true;
    foundCount++;
    statusMessage = targetWords[matchedIndex] + " found";
    clearSelection();
    if (foundCount == kWordCount) {
      completed = true;
      ARCADE_PROGRESS.recordWin(ArcadeGameId::WordPuzzle);
      statusMessage = "All words found - Confirm for next board";
    }
  } else {
    statusMessage = currentWord;
  }

  requestUpdate();
}

void WordPuzzleActivity::onEnter() {
  Activity::onEnter();
  ARCADE_PROGRESS.loadFromFile();
  ARCADE_PROGRESS.recordSessionStart();
  loadPuzzle(0);
  requestUpdate();
}

void WordPuzzleActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      mappedInput.suppressTouchButtonFallback();
      if (completed) {
        selectCurrentCell();
        return;
      }
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int pageWidth = renderer.getScreenWidth();
      const int pageHeight = renderer.getScreenHeight();
      const int gridTop = metrics.topPadding + metrics.headerHeight + 20;
      const int gridBottom = pageHeight - metrics.buttonHintsHeight - 92;
      const int cell = std::min((pageWidth - 72) / kSize, (gridBottom - gridTop) / kSize);
      const int startX = (pageWidth - cell * kSize) / 2;
      int row = 0;
      int col = 0;
      if (TouchHitTest::gridCellAt(Rect{startX, gridTop, cell * kSize, cell * kSize}, kSize, kSize, touchEvent.x,
                                   touchEvent.y, &row, &col)) {
        cursor = {row, col};
        selectCurrentCell();
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
    selectCurrentCell();
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

  buttonNavigator.onPreviousRelease([this] { moveCursor(-1, 0); });
  buttonNavigator.onNextRelease([this] { moveCursor(1, 0); });
}

void WordPuzzleActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Word Puzzle");

  const int gridTop = metrics.topPadding + metrics.headerHeight + 20;
  const int gridBottom = pageHeight - metrics.buttonHintsHeight - 92;
  const int cell = std::min((pageWidth - 72) / kSize, (gridBottom - gridTop) / kSize);
  const int boardSize = cell * kSize;
  const int startX = (pageWidth - boardSize) / 2;

  renderer.drawRoundedRect(startX - 2, gridTop - 2, boardSize + 4, boardSize + 4, 2, 6, true);

  for (int row = 0; row < kSize; row++) {
    for (int col = 0; col < kSize; col++) {
      const int x = startX + col * cell;
      const int y = gridTop + row * cell;
      const bool selected = isSelected(row, col);
      const bool cursorSelected = (cursor.row == row && cursor.col == col);

      Color fillColor = Color::White;
      bool blackText = true;
      if (selected) {
        fillColor = Color::Black;
        blackText = false;
      } else if (cursorSelected) {
        fillColor = Color::LightGray;
      }

      renderer.fillRoundedRect(x + 3, y + 3, cell - 6, cell - 6, 6, fillColor);
      renderer.drawRoundedRect(x + 3, y + 3, cell - 6, cell - 6, cursorSelected ? 2 : 1, 6, true);

      char text[2] = {board[row][col], '\0'};
      const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, text, EpdFontFamily::BOLD);
      const int textHeight = renderer.getTextHeight(UI_12_FONT_ID);
      renderer.drawText(UI_12_FONT_ID, x + (cell - textWidth) / 2, y + (cell - textHeight) / 2 - 2, text, blackText,
                        EpdFontFamily::BOLD);
    }
  }

  const int wordTop = gridTop + boardSize + 14;
  const int leftX = 24;
  const int rightX = pageWidth / 2 + 8;
  for (int i = 0; i < kWordCount; i++) {
    const int x = (i < 2) ? leftX : rightX;
    const int y = wordTop + (i % 2) * 26;
    const char* marker = foundWords[i] ? "[x]" : "[ ]";
    renderer.drawText(SMALL_FONT_ID, x, y, marker, true, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, x + 28, y - 2, targetWords[i].c_str(), true,
                      foundWords[i] ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  }

  const int statusY = pageHeight - metrics.buttonHintsHeight - 38;
  renderer.drawCenteredText(UI_10_FONT_ID, statusY, statusMessage.c_str(), true,
                            completed ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);

  char progress[24];
  snprintf(progress, sizeof(progress), "Board %d  %d/%d", puzzleIndex + 1, foundCount, kWordCount);
  renderer.drawCenteredText(SMALL_FONT_ID, statusY + 24, progress);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), completed ? "Next" : "Pick", "Left/Up", "Right/Down");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
