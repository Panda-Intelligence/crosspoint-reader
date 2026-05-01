#pragma once

#include "../Activity.h"

class DashboardCustomizeActivity final : public Activity {
 public:
  explicit DashboardCustomizeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DashboardCustomize", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int selectedIndex = 0;
  bool moving = false;

  void selectRelative(int delta);
  void moveSelected(int delta);
  void cycleSelected(int direction);
};
