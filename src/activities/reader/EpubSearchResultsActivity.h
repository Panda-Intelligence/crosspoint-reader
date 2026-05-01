#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

struct EpubSearchResult {
  uint16_t page = 0;
  std::string snippet;
};

class EpubSearchResultsActivity final : public Activity {
 public:
  explicit EpubSearchResultsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                     std::vector<EpubSearchResult> results)
      : Activity("EpubSearchResults", renderer, mappedInput), results(std::move(results)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }

 private:
  std::vector<EpubSearchResult> results;
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  int getPageItems() const;
  void openSelectedResult();
  std::string labelForResult(const EpubSearchResult& result) const;
};
