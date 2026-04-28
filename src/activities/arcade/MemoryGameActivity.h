#pragma once

#include <array>
#include <cstdint>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class MemoryGameActivity final : public Activity {
 public:
  static constexpr int kCols = 4;
  static constexpr int kRows = 3;
  static constexpr int kPairs = (kCols * kRows) / 2;

 private:
  // Card values: 0 = unset, 1..kPairs = pair symbol
  std::array<uint8_t, kCols * kRows> cards{};

  // State per card: Hidden, Flipped (temporarily visible), Matched
  enum class CardState : uint8_t { Hidden, Flipped, Matched };
  std::array<CardState, kCols * kRows> states{};

  int cursorIndex = 0;    // selected card index (0..kCols*kRows-1)
  int firstFlipped = -1;  // index of first card flipped this turn, -1 if none
  int matchedPairs = 0;
  int moveCount = 0;

  // Delay rendering of a failed flip to give the player time to see both cards
  uint32_t flipFailMs = 0;  // millis() timestamp when flip-fail started
  bool awaitingFlipReset = false;

  ButtonNavigator buttonNavigator;

  void initCards();
  bool isFlippable(int index) const;
  void tryFlip(int index);
  void syncFlipReset();

 public:
  explicit MemoryGameActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Memory", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
