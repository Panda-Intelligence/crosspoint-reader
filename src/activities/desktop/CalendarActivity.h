#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class CalendarActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int pageIndex = 0;

 public:
  explicit CalendarActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Calendar", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
