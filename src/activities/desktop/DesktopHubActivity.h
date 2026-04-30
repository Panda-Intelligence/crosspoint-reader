#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class DesktopHubActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  void openCurrentSelection();

 public:
  explicit DesktopHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Desktop", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
