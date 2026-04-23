#pragma once

#include <array>
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class WordPuzzleActivity final : public Activity {
 public:
  static constexpr int kSize = 4;
  static constexpr int kWordCount = 4;
  static constexpr int kPuzzleCount = 3;
  using Grid = std::array<std::array<char, kSize>, kSize>;

 private:
  struct CellPos {
    int row = 0;
    int col = 0;
  };

  Grid board{};
  std::array<std::string, kWordCount> targetWords{};
  std::array<bool, kWordCount> foundWords{};
  std::vector<CellPos> selectedPath;
  std::string currentWord;
  std::string statusMessage;
  CellPos cursor;
  int puzzleIndex = 0;
  int foundCount = 0;
  bool completed = false;
  ButtonNavigator buttonNavigator;

  void loadPuzzle(int index);
  void moveCursor(int rowDelta, int colDelta);
  bool isSelected(int row, int col) const;
  bool isAdjacentToCurrentPath(int row, int col) const;
  bool isPrefixOfRemainingWord(const std::string& value) const;
  int matchedWordIndex(const std::string& value) const;
  void clearSelection();
  void selectCurrentCell();

 public:
  explicit WordPuzzleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("WordPuzzle", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
