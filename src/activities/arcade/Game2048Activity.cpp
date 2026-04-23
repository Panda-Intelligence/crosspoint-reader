#include "Game2048Activity.h"

#include <Arduino.h>
#include <I18n.h>

#include <algorithm>
#include <array>

#include "ArcadeProgressStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

void Game2048Activity::resetGame() {
  for (auto& row : grid) {
    row.fill(0);
  }
  score = 0;
  reached2048 = false;
  gameOver = false;
  placeRandomTile();
  placeRandomTile();
}

bool Game2048Activity::placeRandomTile() {
  std::array<std::pair<int, int>, kSize * kSize> empties{};
  int emptyCount = 0;
  for (int r = 0; r < kSize; r++) {
    for (int c = 0; c < kSize; c++) {
      if (grid[r][c] == 0) {
        empties[emptyCount++] = {r, c};
      }
    }
  }

  if (emptyCount == 0) {
    return false;
  }

  const int selected = static_cast<int>(random(emptyCount));
  const auto [row, col] = empties[selected];
  const bool spawnFour = (random(10) == 0);
  grid[row][col] = spawnFour ? 4 : 2;
  return true;
}

bool Game2048Activity::slideAndMergeLine(std::array<uint16_t, kSize>& line, uint32_t& gainedScore) {
  std::array<uint16_t, kSize> original = line;

  std::array<uint16_t, kSize> compact{};
  int compactIndex = 0;
  for (int i = 0; i < kSize; i++) {
    if (line[i] != 0) {
      compact[compactIndex++] = line[i];
    }
  }

  std::array<uint16_t, kSize> merged{};
  int mergedIndex = 0;
  for (int i = 0; i < compactIndex; i++) {
    if (i + 1 < compactIndex && compact[i] == compact[i + 1]) {
      merged[mergedIndex] = static_cast<uint16_t>(compact[i] * 2);
      gainedScore += merged[mergedIndex];
      i++;
    } else {
      merged[mergedIndex] = compact[i];
    }
    mergedIndex++;
  }

  line = merged;
  return line != original;
}

bool Game2048Activity::move(const Direction direction) {
  bool changed = false;
  uint32_t gainedScore = 0;

  for (int fixed = 0; fixed < kSize; fixed++) {
    std::array<uint16_t, kSize> line{};
    for (int var = 0; var < kSize; var++) {
      switch (direction) {
        case Direction::Left:
          line[var] = grid[fixed][var];
          break;
        case Direction::Right:
          line[var] = grid[fixed][kSize - 1 - var];
          break;
        case Direction::Up:
          line[var] = grid[var][fixed];
          break;
        case Direction::Down:
          line[var] = grid[kSize - 1 - var][fixed];
          break;
      }
    }

    if (slideAndMergeLine(line, gainedScore)) {
      changed = true;
    }

    for (int var = 0; var < kSize; var++) {
      switch (direction) {
        case Direction::Left:
          grid[fixed][var] = line[var];
          break;
        case Direction::Right:
          grid[fixed][kSize - 1 - var] = line[var];
          break;
        case Direction::Up:
          grid[var][fixed] = line[var];
          break;
        case Direction::Down:
          grid[kSize - 1 - var][fixed] = line[var];
          break;
      }
    }
  }

  if (changed) {
    score += gainedScore;
    for (int r = 0; r < kSize; r++) {
      for (int c = 0; c < kSize; c++) {
        ARCADE_PROGRESS.record2048Tile(grid[r][c]);
        if (grid[r][c] >= 2048) {
          reached2048 = true;
        }
      }
    }
    placeRandomTile();
    gameOver = !canMove();
  }

  return changed;
}

bool Game2048Activity::canMove() const {
  for (int r = 0; r < kSize; r++) {
    for (int c = 0; c < kSize; c++) {
      if (grid[r][c] == 0) {
        return true;
      }
      if (r + 1 < kSize && grid[r][c] == grid[r + 1][c]) {
        return true;
      }
      if (c + 1 < kSize && grid[r][c] == grid[r][c + 1]) {
        return true;
      }
    }
  }
  return false;
}

void Game2048Activity::onEnter() {
  Activity::onEnter();
  ARCADE_PROGRESS.loadFromFile();
  ARCADE_PROGRESS.recordSessionStart();
  resetGame();
  requestUpdate();
}

void Game2048Activity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    resetGame();
    requestUpdate();
    return;
  }

  bool moved = false;
  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    moved = move(Direction::Left);
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    moved = move(Direction::Right);
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    moved = move(Direction::Up);
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    moved = move(Direction::Down);
  }

  if (moved) {
    requestUpdate();
  }
}

void Game2048Activity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "2048");

  const int gridTop = metrics.topPadding + metrics.headerHeight + 20;
  const int gridBottom = pageHeight - metrics.buttonHintsHeight - 24;
  const int boardSize = std::min(pageWidth - 60, gridBottom - gridTop);
  const int cell = boardSize / kSize;
  const int startX = (pageWidth - boardSize) / 2;
  const int startY = gridTop;

  // Rounded outer board border
  renderer.drawRoundedRect(startX - 2, startY - 2, boardSize + 4, boardSize + 4, 2, 6, true);

  for (int r = 0; r < kSize; r++) {
    for (int c = 0; c < kSize; c++) {
      const int x = startX + c * cell;
      const int y = startY + r * cell;
      const uint16_t value = grid[r][c];
      constexpr int kPad = 4;
      constexpr int kRadius = 8;
      const int tileX = x + kPad;
      const int tileY = y + kPad;
      const int tileW = cell - kPad * 2;
      const int tileH = cell - kPad * 2;

      if (value == 0) {
        // Empty tile: faint outline only
        renderer.drawRoundedRect(tileX, tileY, tileW, tileH, 1, kRadius, true);
      } else {
        // Gray hierarchy by value: low=LightGray, mid=DarkGray, high=Black+white text
        Color fillColor;
        bool whiteText;
        if (value <= 4) {
          fillColor = Color::LightGray;
          whiteText = false;
        } else if (value <= 32) {
          fillColor = Color::DarkGray;
          whiteText = false;
        } else {
          fillColor = Color::Black;
          whiteText = true;
        }
        renderer.fillRoundedRect(tileX, tileY, tileW, tileH, kRadius, fillColor);
        renderer.drawRoundedRect(tileX, tileY, tileW, tileH, 1, kRadius, true);

        char buffer[8];
        snprintf(buffer, sizeof(buffer), "%u", value);
        const int textH = renderer.getTextHeight(UI_10_FONT_ID);
        renderer.drawCenteredText(UI_10_FONT_ID, tileY + (tileH - textH) / 2, buffer, !whiteText,
                                  EpdFontFamily::BOLD);
      }
    }
  }

  // Score below board
  char scoreBuffer[32];
  snprintf(scoreBuffer, sizeof(scoreBuffer), "Score  %lu", static_cast<unsigned long>(score));
  renderer.drawCenteredText(UI_10_FONT_ID, startY + boardSize + 8, scoreBuffer, true, EpdFontFamily::BOLD);

  const int statusY = startY + boardSize + 34;
  if (gameOver) {
    renderer.drawCenteredText(UI_10_FONT_ID, statusY, "Game over - Confirm to restart");
  } else if (reached2048) {
    renderer.drawCenteredText(UI_10_FONT_ID, statusY, "2048 reached! Keep going");
  } else {
    renderer.drawCenteredText(SMALL_FONT_ID, statusY, "Arrows to slide - Confirm to restart");
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Restart", "Left/Up", "Right/Down");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
