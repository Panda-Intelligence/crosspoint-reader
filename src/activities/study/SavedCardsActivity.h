#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class SavedCardsActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool showingBack = false;

  int itemCount() const;

 public:
  explicit SavedCardsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SavedCards", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
