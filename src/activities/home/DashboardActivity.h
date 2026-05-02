#pragma once

#include <array>

#include "../Activity.h"
#include "DashboardShortcutStore.h"
#include "components/themes/BaseTheme.h"
#include "util/ButtonNavigator.h"

class DashboardActivity final : public Activity {
 public:
  static constexpr int kGridRows = 3;
  static constexpr int kGridCols = 3;
  static constexpr int kGridCellCount = kGridRows * kGridCols;
  // Index of the dedicated Customize entry (rendered as a footer row below the grid).
  static constexpr int kCustomizeIndex = kGridCellCount;
  static constexpr int kItemCount = kGridCellCount + 1;

  explicit DashboardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Dashboard", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  // Cached layout rects. cellRects[0..8] = grid cells (row-major); cellRects[9] = Customize row.
  // Recomputed by layoutCells(); render() and loop() must call layoutCells() before use.
  std::array<Rect, kItemCount> cellRects;

  int itemCount() const { return kItemCount; }
  std::string subtitleForShortcut(DashboardShortcutId id) const;
  void openCurrentSelection();
  void layoutCells();
};
