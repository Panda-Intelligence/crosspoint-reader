#pragma once

#include <Epub.h>
#include <EpubBookmarkStore.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class EpubBookmarksActivity final : public Activity {
 public:
  explicit EpubBookmarksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 const std::shared_ptr<Epub>& epub, std::vector<EpubBookmark> bookmarks)
      : Activity("EpubBookmarks", renderer, mappedInput), epub(epub), bookmarks(std::move(bookmarks)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }

 private:
  std::shared_ptr<Epub> epub;
  std::vector<EpubBookmark> bookmarks;
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  int getPageItems() const;
  void openSelectedBookmark();
  std::string labelForBookmark(const EpubBookmark& bookmark) const;
};
