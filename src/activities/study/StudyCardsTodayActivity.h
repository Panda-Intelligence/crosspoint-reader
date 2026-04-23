#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class StudyCardsTodayActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  bool inSession = false;
  bool showingBack = false;
  int cardIndex = 0;
  int actionIndex = 0;
  int reviewedInSession = 0;
  int savedCount = 0;

  void startSession();
  void advanceCard(bool recordResult, bool correct, bool saved);

 public:
  explicit StudyCardsTodayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("StudyCardsToday", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
