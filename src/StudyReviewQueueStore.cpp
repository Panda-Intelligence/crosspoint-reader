#include "StudyReviewQueueStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>

#include <algorithm>

namespace {
constexpr char STUDY_REVIEW_DIR[] = "/.mofei/study";
constexpr char STUDY_REVIEW_FILE[] = "/.mofei/study/review_queue.json";

bool sameCard(const StudyQueuedCard& item, const StudyCard& card) {
  return item.deckName == card.deckName && item.front == card.front && item.back == card.back;
}

void loadQueue(JsonArrayConst source, std::vector<StudyQueuedCard>& target) {
  target.clear();
  for (JsonVariantConst variant : source) {
    if (!variant.is<JsonObjectConst>()) {
      continue;
    }

    JsonObjectConst object = variant.as<JsonObjectConst>();
    StudyQueuedCard card;
    card.front = object["front"] | "";
    card.back = object["back"] | "";
    card.deckName = object["deckName"] | "";
    card.count = object["count"] | static_cast<uint16_t>(1);

    if (!card.front.empty() && !card.back.empty()) {
      target.push_back(card);
    }
  }
}

void saveQueue(JsonObject doc, const char* key, const std::vector<StudyQueuedCard>& source) {
  JsonArray array = doc[key].to<JsonArray>();
  for (const auto& item : source) {
    JsonObject object = array.add<JsonObject>();
    object["front"] = item.front;
    object["back"] = item.back;
    object["deckName"] = item.deckName;
    object["count"] = item.count;
  }
}
}  // namespace

StudyReviewQueueStore StudyReviewQueueStore::instance;

StudyQueuedCard StudyReviewQueueStore::toQueuedCard(const StudyCard& card) {
  StudyQueuedCard queued;
  queued.front = card.front;
  queued.back = card.back;
  queued.deckName = card.deckName;
  queued.count = 1;
  return queued;
}

void StudyReviewQueueStore::upsert(std::vector<StudyQueuedCard>& queue, const StudyCard& card) {
  const auto existing = std::find_if(queue.begin(), queue.end(),
                                    [&card](const StudyQueuedCard& item) { return sameCard(item, card); });
  if (existing != queue.end()) {
    if (existing->count < UINT16_MAX) {
      existing->count++;
    }
    return;
  }

  if (static_cast<int>(queue.size()) >= kMaxItemsPerQueue) {
    queue.erase(queue.begin());
  }
  queue.push_back(toQueuedCard(card));
}

std::vector<StudyQueuedCard>& StudyReviewQueueStore::mutableQueue(const StudyQueueKind kind) {
  switch (kind) {
    case StudyQueueKind::Later:
      return laterCards;
    case StudyQueueKind::Saved:
      return savedCards;
    case StudyQueueKind::Again:
    default:
      return againCards;
  }
}

const std::vector<StudyQueuedCard>& StudyReviewQueueStore::queue(const StudyQueueKind kind) const {
  switch (kind) {
    case StudyQueueKind::Later:
      return laterCards;
    case StudyQueueKind::Saved:
      return savedCards;
    case StudyQueueKind::Again:
    default:
      return againCards;
  }
}

bool StudyReviewQueueStore::loadFromFile() {
  if (!Storage.exists(STUDY_REVIEW_FILE)) {
    againCards.clear();
    laterCards.clear();
    savedCards.clear();
    return false;
  }

  const String json = Storage.readFile(STUDY_REVIEW_FILE);
  if (json.isEmpty()) {
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return false;
  }

  loadQueue(doc["again"].as<JsonArrayConst>(), againCards);
  loadQueue(doc["later"].as<JsonArrayConst>(), laterCards);
  loadQueue(doc["saved"].as<JsonArrayConst>(), savedCards);
  return true;
}

bool StudyReviewQueueStore::saveToFile() const {
  Storage.mkdir("/.mofei");
  Storage.mkdir(STUDY_REVIEW_DIR);

  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  saveQueue(root, "again", againCards);
  saveQueue(root, "later", laterCards);
  saveQueue(root, "saved", savedCards);

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(STUDY_REVIEW_FILE, json);
}

void StudyReviewQueueStore::recordAgain(const StudyCard& card) {
  upsert(againCards, card);
  saveToFile();
}

void StudyReviewQueueStore::recordLater(const StudyCard& card) {
  upsert(laterCards, card);
  saveToFile();
}

void StudyReviewQueueStore::recordSaved(const StudyCard& card) {
  upsert(savedCards, card);
  saveToFile();
}

bool StudyReviewQueueStore::removeAt(const StudyQueueKind kind, const int index) {
  auto& target = mutableQueue(kind);
  if (index < 0 || index >= static_cast<int>(target.size())) {
    return false;
  }

  target.erase(target.begin() + index);
  saveToFile();
  return true;
}

void StudyReviewQueueStore::clearQueue(const StudyQueueKind kind) {
  auto& target = mutableQueue(kind);
  target.clear();
  saveToFile();
}
