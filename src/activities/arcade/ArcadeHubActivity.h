#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class ArcadeHubActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

 public:
  explicit ArcadeHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Arcade", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
