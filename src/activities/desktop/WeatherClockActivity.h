#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class WeatherClockActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int pageIndex = 0;

 public:
  explicit WeatherClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("WeatherClock", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
