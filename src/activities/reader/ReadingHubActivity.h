#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class ReadingHubActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  void openCurrentSelection();

 public:
  explicit ReadingHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Reading", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
