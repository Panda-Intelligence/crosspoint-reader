#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class ArcadeChallengesActivity final : public Activity {
  static constexpr int kPageCount = 2;

  ButtonNavigator buttonNavigator;
  int pageIndex = 0;

 public:
  explicit ArcadeChallengesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ArcadeChallenges", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
