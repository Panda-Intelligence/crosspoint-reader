#include "EpubBookmarkStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <functional>

namespace {
constexpr char kBookmarkRootDir[] = "/.mofei";
constexpr char kBookmarkDir[] = "/.mofei/bookmarks";

bool isValidBookmark(const EpubBookmark& bookmark) {
  return bookmark.spineIndex >= 0 && bookmark.page >= 0 && bookmark.pageCount >= 0;
}
}  // namespace

std::string EpubBookmarkStore::normalizeTitle(const std::string& title) {
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

std::string EpubBookmarkStore::pathForBook(const std::string& epubPath) {
  return std::string(kBookmarkDir) + "/epub_" + std::to_string(std::hash<std::string>{}(epubPath)) + ".json";
}

int EpubBookmarkStore::findBookmarkIndex(const int spineIndex, const int page) const {
  for (size_t i = 0; i < bookmarks.size(); i++) {
    if (bookmarks[i].spineIndex == spineIndex && bookmarks[i].page == page) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool EpubBookmarkStore::loadForBook(const std::string& epubPath) {
  bookmarks.clear();
  storagePath = pathForBook(epubPath);

  Storage.mkdir(kBookmarkRootDir);
  Storage.mkdir(kBookmarkDir);

  if (!Storage.exists(storagePath.c_str())) {
    return saveToFile();
  }

  const String json = Storage.readFile(storagePath.c_str());
  JsonDocument doc;
  if (json.isEmpty() || deserializeJson(doc, json)) {
    LOG_ERR("BMK", "Bookmark file is corrupt; restoring empty list");
    return saveToFile();
  }

  JsonArrayConst arr = doc["bookmarks"].as<JsonArrayConst>();
  if (arr.isNull()) {
    return true;
  }

  for (JsonVariantConst variant : arr) {
    if (bookmarks.size() >= kMaxBookmarks) {
      break;
    }
    if (!variant.is<JsonObjectConst>()) {
      continue;
    }

    JsonObjectConst item = variant.as<JsonObjectConst>();
    EpubBookmark bookmark;
    bookmark.spineIndex = item["spineIndex"] | -1;
    bookmark.page = item["page"] | -1;
    bookmark.pageCount = item["pageCount"] | 0;
    bookmark.title = normalizeTitle(item["title"] | "");
    if (!isValidBookmark(bookmark) || findBookmarkIndex(bookmark.spineIndex, bookmark.page) >= 0) {
      continue;
    }
    bookmarks.push_back(bookmark);
  }

  std::sort(bookmarks.begin(), bookmarks.end(), [](const EpubBookmark& lhs, const EpubBookmark& rhs) {
    if (lhs.spineIndex != rhs.spineIndex) {
      return lhs.spineIndex < rhs.spineIndex;
    }
    return lhs.page < rhs.page;
  });

  return true;
}

bool EpubBookmarkStore::saveToFile() const {
  if (storagePath.empty()) {
    return false;
  }

  Storage.mkdir(kBookmarkRootDir);
  Storage.mkdir(kBookmarkDir);

  JsonDocument doc;
  JsonArray arr = doc["bookmarks"].to<JsonArray>();
  for (const auto& bookmark : bookmarks) {
    JsonObject item = arr.add<JsonObject>();
    item["spineIndex"] = bookmark.spineIndex;
    item["page"] = bookmark.page;
    item["pageCount"] = bookmark.pageCount;
    item["title"] = bookmark.title;
  }

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(storagePath.c_str(), json);
}

bool EpubBookmarkStore::addBookmark(EpubBookmark bookmark) {
  if (!isValidBookmark(bookmark) || storagePath.empty()) {
    return false;
  }
  bookmark.title = normalizeTitle(bookmark.title);

  const int existingIndex = findBookmarkIndex(bookmark.spineIndex, bookmark.page);
  if (existingIndex >= 0) {
    bookmarks[existingIndex] = bookmark;
    return saveToFile();
  }

  if (bookmarks.size() >= kMaxBookmarks) {
    bookmarks.erase(bookmarks.begin());
  }

  auto insertPos = std::lower_bound(bookmarks.begin(), bookmarks.end(), bookmark,
                                    [](const EpubBookmark& lhs, const EpubBookmark& rhs) {
                                      if (lhs.spineIndex != rhs.spineIndex) {
                                        return lhs.spineIndex < rhs.spineIndex;
                                      }
                                      return lhs.page < rhs.page;
                                    });
  bookmarks.insert(insertPos, bookmark);
  return saveToFile();
}

bool EpubBookmarkStore::removeBookmark(const int spineIndex, const int page) {
  const int index = findBookmarkIndex(spineIndex, page);
  if (index < 0) {
    return false;
  }
  bookmarks.erase(bookmarks.begin() + index);
  return saveToFile();
}

bool EpubBookmarkStore::toggleBookmark(const EpubBookmark& bookmark) {
  if (isBookmarked(bookmark.spineIndex, bookmark.page)) {
    return removeBookmark(bookmark.spineIndex, bookmark.page);
  }
  return addBookmark(bookmark);
}

bool EpubBookmarkStore::isBookmarked(const int spineIndex, const int page) const {
  return findBookmarkIndex(spineIndex, page) >= 0;
}

void EpubBookmarkStore::clear() {
  bookmarks.clear();
  saveToFile();
}
