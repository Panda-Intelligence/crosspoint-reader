#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class StickyNotesActivity final : public Activity {
 public:
  explicit StickyNotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("StickyNotes", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool showingDetail = false;

  int itemCount() const;
  void addNote();
  void editSelected();
  void toggleSelected();
  void deleteCompleted();
  void handleKeyboardResult(const ActivityResult& result, int editIndex);
  void clampSelection();
};
