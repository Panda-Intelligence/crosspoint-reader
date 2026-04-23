#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class ReviewQueueActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int queueIndex = 0;
  int cardIndex = 0;
  bool showingBack = false;

  void switchQueue(int delta);
  void moveCard(int delta);
  int currentQueueSize() const;

 public:
  explicit ReviewQueueActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReviewQueue", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
