#pragma once

#include <string>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class DeckImportStatusActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool hasStudyStateFile = false;

  void refreshDeckEntries();
  std::string sizeLabel(size_t bytes) const;

 public:
  explicit DeckImportStatusActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DeckImportStatus", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
