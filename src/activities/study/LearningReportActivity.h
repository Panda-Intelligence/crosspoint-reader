#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class LearningReportActivity final : public Activity {
  ButtonNavigator buttonNavigator;

 public:
  explicit LearningReportActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("LearningReport", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
