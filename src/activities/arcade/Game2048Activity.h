#pragma once

#include <array>
#include <cstdint>

#include "../Activity.h"

class Game2048Activity final : public Activity {
  enum class Direction { Left, Right, Up, Down };

  static constexpr int kSize = 4;
  using Grid = std::array<std::array<uint16_t, kSize>, kSize>;

  Grid grid{};
  uint32_t score = 0;
  bool reached2048 = false;
  bool gameOver = false;

  void resetGame();
  bool move(Direction direction);
  bool placeRandomTile();
  bool canMove() const;

  static bool slideAndMergeLine(std::array<uint16_t, kSize>& line, uint32_t& gainedScore);

 public:
  explicit Game2048Activity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Game2048", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
