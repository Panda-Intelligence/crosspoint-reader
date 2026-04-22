#pragma once

#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class SokobanActivity final : public Activity {
 public:
  static constexpr int kRows = 8;
  static constexpr int kCols = 8;

 private:
  struct Pos {
    int row = 0;
    int col = 0;
  };

  enum class Tile {
    Wall = '#',
    Floor = ' ',
    Target = '.',
    Box = '$',
    BoxOnTarget = '*',
    Player = '@',
    PlayerOnTarget = '+'
  };

  std::vector<std::vector<Tile>> board;
  Pos playerPos;
  int moveCount = 0;
  bool completed = false;
  ButtonNavigator buttonNavigator;

  void loadLevel();
  bool isWall(int row, int col) const;
  bool isBox(int row, int col) const;
  bool isTarget(int row, int col) const;
  void movePlayer(int rowDelta, int colDelta);
  void checkWin();

 public:
  explicit SokobanActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Sokoban", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
