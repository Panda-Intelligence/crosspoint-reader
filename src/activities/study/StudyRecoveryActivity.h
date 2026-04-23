#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class StudyRecoveryActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  bool showingBack = false;
  int selectedIndex = 0;
  int actionIndex = 0;

 public:
  explicit StudyRecoveryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("StudyRecovery", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
