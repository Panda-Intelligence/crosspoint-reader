#pragma once

#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class DailyMazeActivity final : public Activity {
 public:
  static constexpr int kRows = 9;
  static constexpr int kCols = 9;

 private:
  struct CellPos {
    int row = 0;
    int col = 0;
  };

  std::vector<std::string> mazeRows;
  CellPos playerPos;
  CellPos exitPos;
  int stepCount = 0;
  bool completed = false;
  ButtonNavigator buttonNavigator;

  void loadMaze();
  bool canMoveTo(int row, int col) const;
  void movePlayer(int rowDelta, int colDelta);

 public:
  explicit DailyMazeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DailyMaze", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
