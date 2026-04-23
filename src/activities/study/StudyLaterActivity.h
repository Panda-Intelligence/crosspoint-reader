#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class StudyLaterActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool showingBack = false;

  int itemCount() const;

 public:
  explicit StudyLaterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("StudyLater", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
