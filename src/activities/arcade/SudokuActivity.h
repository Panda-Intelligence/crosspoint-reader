#pragma once

#include <array>
#include <cstdint>

#include "../Activity.h"

class SudokuActivity final : public Activity {
 public:
  static constexpr int kSize = 9;
  using Board = std::array<std::array<uint8_t, kSize>, kSize>;

 private:
  Board given{};
  Board board{};
  int cursorRow = 0;
  int cursorCol = 0;
  bool hasConflicts = false;
  bool completed = false;

  void loadPuzzle();
  bool isEditable(int row, int col) const;
  bool isConflictAt(int row, int col) const;
  void refreshStatus();
  void moveCursor(int rowDelta, int colDelta);
  void cycleCellValue();

 public:
  explicit SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Sudoku", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
