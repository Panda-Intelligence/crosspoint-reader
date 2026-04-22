#pragma once

#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class DeckImportStatusActivity final : public Activity {
  struct DeckEntry {
    std::string filename;
    size_t bytes = 0;
  };

  ButtonNavigator buttonNavigator;
  std::vector<DeckEntry> deckEntries;
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
