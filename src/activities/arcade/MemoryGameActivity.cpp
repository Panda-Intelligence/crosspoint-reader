#include "MemoryGameActivity.h"

#include <Arduino.h>
#include <I18n.h>

#include <algorithm>
#include <array>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Symbol labels for each pair value (1..kPairs)
constexpr const char* kSymbols[MemoryGameActivity::kPairs + 1] = {
    "",   // index 0 unused
    "A",  // pair 1
    "B",  // pair 2
    "C",  // pair 3
    "D",  // pair 4
    "E",  // pair 5
    "F",  // pair 6
};

constexpr uint32_t kFlipFailShowMs = 1200;  // ms to show a failed pair before hiding
}  // namespace

void MemoryGameActivity::initCards() {
  // Fill cards: two of each pair value
  for (int i = 0; i < kPairs; i++) {
    cards[i * 2] = static_cast<uint8_t>(i + 1);
    cards[i * 2 + 1] = static_cast<uint8_t>(i + 1);
  }
  // Fisher-Yates shuffle using Arduino random()
  for (int i = kCols * kRows - 1; i > 0; i--) {
    const int j = static_cast<int>(random(i + 1));
    std::swap(cards[i], cards[j]);
  }
  states.fill(CardState::Hidden);
  cursorIndex = 0;
  firstFlipped = -1;
  matchedPairs = 0;
  moveCount = 0;
  awaitingFlipReset = false;
  flipFailMs = 0;
}

bool MemoryGameActivity::isFlippable(int index) const {
  return states[index] == CardState::Hidden && !awaitingFlipReset;
}

void MemoryGameActivity::tryFlip(int index) {
  if (!isFlippable(index)) return;

  states[index] = CardState::Flipped;

  if (firstFlipped == -1) {
    // First card of this turn
    firstFlipped = index;
  } else {
    // Second card: check for match
    moveCount++;
    if (cards[index] == cards[firstFlipped]) {
      // Match!
      states[index] = CardState::Matched;
      states[firstFlipped] = CardState::Matched;
      matchedPairs++;
      firstFlipped = -1;
    } else {
      // No match: schedule a hide after kFlipFailShowMs
      flipFailMs = millis();
      awaitingFlipReset = true;
    }
  }
  requestUpdate();
}

void MemoryGameActivity::syncFlipReset() {
  if (!awaitingFlipReset) return;
  if (millis() - flipFailMs < kFlipFailShowMs) return;

  // Hide the two un-matched flipped cards
  for (int i = 0; i < kCols * kRows; i++) {
    if (states[i] == CardState::Flipped) {
      states[i] = CardState::Hidden;
    }
  }
  firstFlipped = -1;
  awaitingFlipReset = false;
  requestUpdate();
}

void MemoryGameActivity::onEnter() {
  Activity::onEnter();
  initCards();
  requestUpdate();
}

void MemoryGameActivity::loop() {
  syncFlipReset();

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (matchedPairs == kPairs) {
      // Game complete — restart
      initCards();
      requestUpdate();
      return;
    }
    tryFlip(cursorIndex);
    return;
  }

  if (awaitingFlipReset) return;  // block navigation while showing failed pair

  const int total = kCols * kRows;

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    cursorIndex = (cursorIndex - 1 + total) % total;
    requestUpdate();
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    cursorIndex = (cursorIndex + 1) % total;
    requestUpdate();
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    cursorIndex = (cursorIndex - kCols + total) % total;
    requestUpdate();
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    cursorIndex = (cursorIndex + kCols) % total;
    requestUpdate();
  }

  buttonNavigator.onNextRelease([this, total] {
    cursorIndex = (cursorIndex + 1) % total;
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, total] {
    cursorIndex = (cursorIndex - 1 + total) % total;
    requestUpdate();
  });
}

void MemoryGameActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Memory");

  const int gridTop = metrics.topPadding + metrics.headerHeight + 20;
  const int gridBottom = pageHeight - metrics.buttonHintsHeight - 20;
  const int gridHeight = gridBottom - gridTop;
  const int cellH = gridHeight / kRows;
  const int cellW = std::min((pageWidth - 60) / kCols, cellH);
  const int gridWidth = cellW * kCols;
  const int startX = (pageWidth - gridWidth) / 2;

  for (int row = 0; row < kRows; row++) {
    for (int col = 0; col < kCols; col++) {
      const int idx = row * kCols + col;
      const int x = startX + col * cellW;
      const int y = gridTop + row * cellH;
      const bool isCursor = (idx == cursorIndex);
      const CardState st = states[idx];

      // Outer border — double if cursor
      renderer.drawRect(x + 2, y + 2, cellW - 4, cellH - 4, true);
      if (isCursor) {
        renderer.drawRect(x + 4, y + 4, cellW - 8, cellH - 8, true);
      }

      if (st == CardState::Hidden) {
        // Draw a small cross-hatch to indicate face-down
        renderer.drawLine(x + cellW / 4, y + cellH / 4, x + 3 * cellW / 4, y + 3 * cellH / 4, 1, true);
        renderer.drawLine(x + 3 * cellW / 4, y + cellH / 4, x + cellW / 4, y + 3 * cellH / 4, 1, true);
      } else if (st == CardState::Matched) {
        // Solid fill to show matched
        renderer.fillRect(x + 6, y + 6, cellW - 12, cellH - 12, true);
        // Symbol in inverted (white on black)
        const char* sym = kSymbols[cards[idx]];
        renderer.drawCenteredText(UI_12_FONT_ID, y + cellH / 2 - 12, sym, false, EpdFontFamily::BOLD);
      } else {
        // Flipped: show symbol
        const char* sym = kSymbols[cards[idx]];
        renderer.drawCenteredText(UI_12_FONT_ID, y + cellH / 2 - 12, sym, true, EpdFontFamily::BOLD);
      }
    }
  }

  // Status line
  if (matchedPairs == kPairs) {
    char buf[48];
    snprintf(buf, sizeof(buf), "All matched in %d moves! Confirm to restart", moveCount);
    renderer.drawCenteredText(SMALL_FONT_ID, gridBottom + 4, buf);
  } else {
    char buf[48];
    snprintf(buf, sizeof(buf), "Pairs: %d/%d  Moves: %d", matchedPairs, kPairs, moveCount);
    renderer.drawCenteredText(UI_10_FONT_ID, gridBottom + 4, buf);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), matchedPairs == kPairs ? "Restart" : "Flip",
                                            tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
