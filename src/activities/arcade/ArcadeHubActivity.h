#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class ArcadeHubActivity final : public Activity {
 public:
  struct GameEntry {
    const char* title;
    const char* description;
    const char* controls;
  };

  explicit ArcadeHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Arcade", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool showingDetail = false;

  static const GameEntry& entry(int index);
  static int itemCount();
};
