#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class ClockFocusActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int presetIndex = 0;
  uint32_t remainingMs = 0;
  uint32_t lastTickMs = 0;
  bool isRunning = false;
  bool hasFinished = false;

  void resetTimer();
  bool syncCountdown();

 public:
  explicit ClockFocusActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ClockFocus", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
