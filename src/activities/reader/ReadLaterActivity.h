#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class ReadLaterActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

 public:
  explicit ReadLaterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadLater", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
