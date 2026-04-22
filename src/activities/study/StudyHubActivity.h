#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class StudyHubActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  int importedDeckCount = 0;
  bool hasStudyStateFile = false;

  void refreshImportStatus();

 public:
  explicit StudyHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Study", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
