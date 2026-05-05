#pragma once

#include <I18n.h>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class DictionaryActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool showingDetail = false;
  bool hasQueryWord = false;
  StrId statusMessage = StrId::STR_DICTIONARY_SAVE_HINT;
  std::string queryWord;
  void saveSelectedWord();

 public:
  explicit DictionaryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string word = "")
      : Activity("Dictionary", renderer, mappedInput), queryWord(std::move(word)), hasQueryWord(!word.empty()) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
