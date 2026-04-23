#include "StudyDeckStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>

#include <algorithm>

namespace {
constexpr char kStudyDir[] = "/.mofei/study";
constexpr char kStudyStateFileName[] = "state.json";

bool hasJsonSuffix(const std::string& name) {
  constexpr const char* suffix = ".json";
  constexpr size_t suffixLen = 5;
  return name.size() >= suffixLen && name.compare(name.size() - suffixLen, suffixLen, suffix) == 0;
}

std::string pickString(JsonObjectConst object, const char* first, const char* second, const char* third = nullptr) {
  const char* value = object[first] | "";
  if (value[0] != '\0') {
    return value;
  }

  value = object[second] | "";
  if (value[0] != '\0') {
    return value;
  }

  if (third != nullptr) {
    value = object[third] | "";
    if (value[0] != '\0') {
      return value;
    }
  }

  return "";
}
}  // namespace

StudyDeckStore StudyDeckStore::instance;

StudyDeckSummary StudyDeckStore::loadDeckFile(const std::string& filename) {
  StudyDeckSummary summary;
  summary.filename = filename;

  const std::string path = std::string(kStudyDir) + "/" + filename;
  FsFile deckFile;
  if (Storage.openFileForRead("SDS", path.c_str(), deckFile)) {
    summary.bytes = deckFile.size();
    deckFile.close();
  }

  const String json = Storage.readFile(path.c_str());
  if (json.isEmpty()) {
    return summary;
  }

  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return summary;
  }

  JsonArrayConst cardArray;
  if (doc.is<JsonArrayConst>()) {
    cardArray = doc.as<JsonArrayConst>();
  } else if (doc["cards"].is<JsonArrayConst>()) {
    cardArray = doc["cards"].as<JsonArrayConst>();
  } else {
    return summary;
  }

  const char* title = doc["title"] | "";
  const std::string deckName = title[0] != '\0' ? std::string(title) : filename;
  summary.title = deckName;
  int parsedCount = 0;

  for (JsonVariantConst item : cardArray) {
    if (cards.size() >= kMaxCards) {
      break;
    }
    if (!item.is<JsonObjectConst>()) {
      continue;
    }

    JsonObjectConst object = item.as<JsonObjectConst>();
    StudyCard card;
    card.front = pickString(object, "front", "term", "question");
    card.back = pickString(object, "back", "definition", "answer");
    card.example = pickString(object, "example", "sentence", "note");
    card.deckName = deckName;

    if (card.front.empty() || card.back.empty()) {
      continue;
    }

    cards.push_back(card);
    parsedCount++;
  }

  summary.cardCount = parsedCount;
  summary.valid = parsedCount > 0;
  return summary;
}

void StudyDeckStore::refresh() {
  cards.clear();
  deckSummaries.clear();
  deckCount = 0;
  errorCount = 0;

  Storage.mkdir("/.mofei");
  Storage.mkdir(kStudyDir);

  const auto files = Storage.listFiles(kStudyDir);
  std::vector<std::string> deckFiles;
  for (const auto& file : files) {
    std::string filename = file.c_str();
    if (!hasJsonSuffix(filename) || filename == kStudyStateFileName) {
      continue;
    }
    deckFiles.push_back(filename);
  }

  std::sort(deckFiles.begin(), deckFiles.end());
  for (const auto& filename : deckFiles) {
    if (cards.size() >= kMaxCards) {
      break;
    }
    deckCount++;
    StudyDeckSummary summary = loadDeckFile(filename);
    if (!summary.valid) {
      errorCount++;
    }
    deckSummaries.push_back(summary);
  }
}
