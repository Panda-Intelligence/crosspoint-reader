#pragma once

#include <I18n.h>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class DictionaryActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool showingDetail = false;
  StrId statusMessage = StrId::STR_DICTIONARY_SAVE_HINT;
  void saveSelectedWord();

 public:
  explicit DictionaryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Dictionary", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
