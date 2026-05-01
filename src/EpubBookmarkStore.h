#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct EpubBookmark {
  int spineIndex = 0;
  int page = 0;
  int pageCount = 0;
  std::string title;
};

class EpubBookmarkStore {
 public:
  static constexpr size_t kMaxBookmarks = 64;
  static constexpr size_t kMaxTitleLength = 96;

  bool loadForBook(const std::string& epubPath);
  bool saveToFile() const;
  bool toggleBookmark(const EpubBookmark& bookmark);
  bool addBookmark(EpubBookmark bookmark);
  bool removeBookmark(int spineIndex, int page);
  bool isBookmarked(int spineIndex, int page) const;
  bool hasBookmarks() const { return !bookmarks.empty(); }
  void clear();

  const std::vector<EpubBookmark>& getBookmarks() const { return bookmarks; }

 private:
  std::string storagePath;
  std::vector<EpubBookmark> bookmarks;

  static std::string normalizeTitle(const std::string& title);
  static std::string pathForBook(const std::string& epubPath);
  int findBookmarkIndex(int spineIndex, int page) const;
};
