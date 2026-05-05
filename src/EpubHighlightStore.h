#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct EpubHighlight {
  int spineIndex = 0;
  int page = 0;
  int pageCount = 0;
  int startBlockIndex = -1;
  int startTokenIndex = -1;
  int endBlockIndex = -1;
  int endTokenIndex = -1;
  std::string title;
  std::string snippet;

  bool hasRangeAnchor() const {
    return startBlockIndex >= 0 && startTokenIndex >= 0 && endBlockIndex >= startBlockIndex && endTokenIndex >= 0;
  }
};

class EpubHighlightStore {
 public:
  static constexpr size_t kMaxHighlights = 128;
  static constexpr size_t kMaxTitleLength = 96;

  bool loadForBook(const std::string& epubPath);
  bool saveToFile() const;
  bool toggleHighlight(const EpubHighlight& highlight);
  bool addHighlight(EpubHighlight highlight);
  bool removeHighlight(int spineIndex, int page);
  bool isHighlighted(int spineIndex, int page) const;
  bool hasHighlights() const { return !highlights.empty(); }
  void clear();

  const std::vector<EpubHighlight>& getHighlights() const { return highlights; }

 private:
  std::string storagePath;
  std::vector<EpubHighlight> highlights;

  static std::string normalizeTitle(const std::string& title);
  static std::string normalizeSnippet(const std::string& snippet);
  static std::string pathForBook(const std::string& epubPath);
  int findHighlightIndex(int spineIndex, int page) const;
  int findHighlightIndex(const EpubHighlight& highlight) const;
};
