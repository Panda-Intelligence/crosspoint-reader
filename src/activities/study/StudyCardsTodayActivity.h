#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class StudyCardsTodayActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  bool inSession = false;
  int actionIndex = 0;

 public:
  explicit StudyCardsTodayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("StudyCardsToday", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
