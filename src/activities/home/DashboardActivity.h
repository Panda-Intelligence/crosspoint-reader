#pragma once

#include <array>

#include "../Activity.h"
#include "DashboardShortcutStore.h"
#include "components/themes/BaseTheme.h"

class DashboardActivity final : public Activity {
 public:
  static constexpr int kGridCols = 3;
  static constexpr int kGridCellCount = static_cast<int>(DashboardShortcutStore::SLOT_COUNT);
  static constexpr int kItemCount = kGridCellCount + 1;

  explicit DashboardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Dashboard", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int selectedIndex = 0;
  // Cached layout rects. cellRects[0..itemCount-1] = grid cells (row-major); cellRects[itemCount] = Customize row.
  // Recomputed by layoutCells(); render() and loop() must call layoutCells() before use.
  std::array<Rect, kItemCount> cellRects;

  int gridRowCount() const;
  int itemCount() const { return static_cast<int>(DASHBOARD_SHORTCUTS.getShortcuts().size()); }
  int customizeIndex() const { return itemCount(); }
  int selectionCount() const { return itemCount() + 1; }
  bool isGridIndex(int index) const;
  std::string subtitleForShortcut(DashboardShortcutId id) const;
  void openCurrentSelection();
  void layoutCells();
  void layoutGridCells();
  void layoutListCells();
  void moveSelectionHorizontally(int delta);
  void moveSelectionVertically(int delta);
};
