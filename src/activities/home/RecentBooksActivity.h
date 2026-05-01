#pragma once
#include <I18n.h>

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"
#include "RecentBooksStore.h"
#include "util/ButtonNavigator.h"

struct Rect;

struct RecentBookProgress {
  bool available = false;
  uint8_t percent = 0;
  uint32_t currentPage = 0;
  uint32_t totalPages = 0;
};

struct RecentBookListItem {
  RecentBook book;
  RecentBookProgress progress;
};

class RecentBooksActivity final : public Activity {
 private:
  ButtonNavigator buttonNavigator;

  size_t selectorIndex = 0;

  // Recent tab state
  std::vector<RecentBookListItem> recentBooks;

  // Data loading
  void loadRecentBooks();
  RecentBookProgress loadProgressForBook(const RecentBook& book) const;
  void drawRecentBookRow(const RecentBookListItem& item, int index, Rect rowRect, bool selected);
  bool drawBookCover(const RecentBook& book, int x, int y, int width, int height);
  std::string subtitleForBook(const RecentBook& book) const;

 public:
  explicit RecentBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("RecentBooks", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
