#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class DashboardActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  void openCurrentSelection();

 public:
  explicit DashboardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Dashboard", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
