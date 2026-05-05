#include "EpubHighlightStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <functional>

namespace {
constexpr char kHighlightRootDir[] = "/.mofei";
constexpr char kHighlightDir[] = "/.mofei/highlights";

bool isValidHighlight(const EpubHighlight& highlight) {
  return highlight.spineIndex >= 0 && highlight.page >= 0 && highlight.pageCount >= 0;
}
}  // namespace

std::string EpubHighlightStore::normalizeTitle(const std::string& title) {
  const auto first = title.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = title.find_last_not_of(" \t\r\n");
  std::string normalized = title.substr(first, last - first + 1);
  if (normalized.size() > kMaxTitleLength) {
    normalized.resize(kMaxTitleLength);
  }
  return normalized;
}

std::string EpubHighlightStore::normalizeSnippet(const std::string& snippet) {
  const auto first = snippet.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = snippet.find_last_not_of(" \t\r\n");
  std::string normalized = snippet.substr(first, last - first + 1);
  if (normalized.size() > kMaxTitleLength * 2) {
    normalized.resize(kMaxTitleLength * 2);
  }
  return normalized;
}

std::string EpubHighlightStore::pathForBook(const std::string& epubPath) {
  return std::string(kHighlightDir) + "/epub_" + std::to_string(std::hash<std::string>{}(epubPath)) + ".json";
}

int EpubHighlightStore::findHighlightIndex(const int spineIndex, const int page) const {
  for (size_t i = 0; i < highlights.size(); i++) {
    if (highlights[i].spineIndex == spineIndex && highlights[i].page == page) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int EpubHighlightStore::findHighlightIndex(const EpubHighlight& highlight) const {
  for (size_t i = 0; i < highlights.size(); i++) {
    const auto& existing = highlights[i];
    if (existing.spineIndex != highlight.spineIndex) {
      continue;
    }
    if (highlight.hasRangeAnchor() && existing.hasRangeAnchor()) {
      if (existing.startBlockIndex == highlight.startBlockIndex && existing.startTokenIndex == highlight.startTokenIndex &&
          existing.endBlockIndex == highlight.endBlockIndex && existing.endTokenIndex == highlight.endTokenIndex) {
        return static_cast<int>(i);
      }
      continue;
    }
    if (existing.page == highlight.page) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool EpubHighlightStore::loadForBook(const std::string& epubPath) {
  highlights.clear();
  storagePath = pathForBook(epubPath);

  Storage.mkdir(kHighlightRootDir);
  Storage.mkdir(kHighlightDir);

  if (!Storage.exists(storagePath.c_str())) {
    return saveToFile();
  }

  const String json = Storage.readFile(storagePath.c_str());
  JsonDocument doc;
  if (json.isEmpty() || deserializeJson(doc, json)) {
    LOG_ERR("HIL", "Highlight file is corrupt; restoring empty list");
    return saveToFile();
  }

  JsonArrayConst arr = doc["highlights"].as<JsonArrayConst>();
  if (arr.isNull()) {
    return true;
  }

  for (JsonVariantConst variant : arr) {
    if (highlights.size() >= kMaxHighlights) {
      break;
    }
    if (!variant.is<JsonObjectConst>()) {
      continue;
    }

    JsonObjectConst item = variant.as<JsonObjectConst>();
    EpubHighlight highlight;
    highlight.spineIndex = item["spineIndex"] | -1;
    highlight.page = item["page"] | -1;
    highlight.pageCount = item["pageCount"] | 0;
    highlight.startBlockIndex = item["startBlockIndex"] | -1;
    highlight.startTokenIndex = item["startTokenIndex"] | -1;
    highlight.endBlockIndex = item["endBlockIndex"] | -1;
    highlight.endTokenIndex = item["endTokenIndex"] | -1;
    highlight.title = normalizeTitle(item["title"] | "");
    highlight.snippet = normalizeSnippet(item["snippet"] | "");
    if (!isValidHighlight(highlight) || findHighlightIndex(highlight) >= 0) {
      continue;
    }
    highlights.push_back(highlight);
  }

  std::sort(highlights.begin(), highlights.end(), [](const EpubHighlight& lhs, const EpubHighlight& rhs) {
    if (lhs.spineIndex != rhs.spineIndex) {
      return lhs.spineIndex < rhs.spineIndex;
    }
    return lhs.page < rhs.page;
  });

  return true;
}

bool EpubHighlightStore::saveToFile() const {
  if (storagePath.empty()) {
    return false;
  }

  Storage.mkdir(kHighlightRootDir);
  Storage.mkdir(kHighlightDir);

  JsonDocument doc;
  JsonArray arr = doc["highlights"].to<JsonArray>();
  for (const auto& highlight : highlights) {
    JsonObject item = arr.add<JsonObject>();
    item["spineIndex"] = highlight.spineIndex;
    item["page"] = highlight.page;
    item["pageCount"] = highlight.pageCount;
    item["title"] = highlight.title;
    item["startBlockIndex"] = highlight.startBlockIndex;
    item["startTokenIndex"] = highlight.startTokenIndex;
    item["endBlockIndex"] = highlight.endBlockIndex;
    item["endTokenIndex"] = highlight.endTokenIndex;
    item["snippet"] = highlight.snippet;
  }

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(storagePath.c_str(), json);
}

bool EpubHighlightStore::addHighlight(EpubHighlight highlight) {
  if (!isValidHighlight(highlight) || storagePath.empty()) {
    return false;
  }
  highlight.title = normalizeTitle(highlight.title);
  highlight.snippet = normalizeSnippet(highlight.snippet);

  const int existingIndex = findHighlightIndex(highlight);
  if (existingIndex >= 0) {
    highlights[existingIndex] = highlight;
    return saveToFile();
  }

  if (highlights.size() >= kMaxHighlights) {
    highlights.erase(highlights.begin());
  }

  auto insertPos = std::lower_bound(highlights.begin(), highlights.end(), highlight,
                                    [](const EpubHighlight& lhs, const EpubHighlight& rhs) {
                                      if (lhs.spineIndex != rhs.spineIndex) {
                                        return lhs.spineIndex < rhs.spineIndex;
                                      }
                                      return lhs.page < rhs.page;
                                    });
  highlights.insert(insertPos, highlight);
  return saveToFile();
}

bool EpubHighlightStore::removeHighlight(const int spineIndex, const int page) {
  const int index = findHighlightIndex(spineIndex, page);
  if (index < 0) {
    return false;
  }
  highlights.erase(highlights.begin() + index);
  return saveToFile();
}

bool EpubHighlightStore::toggleHighlight(const EpubHighlight& highlight) {
  if (isHighlighted(highlight.spineIndex, highlight.page)) {
    return removeHighlight(highlight.spineIndex, highlight.page);
  }
  return addHighlight(highlight);
}

bool EpubHighlightStore::isHighlighted(const int spineIndex, const int page) const {
  return findHighlightIndex(spineIndex, page) >= 0;
}

void EpubHighlightStore::clear() {
  highlights.clear();
  saveToFile();
}
