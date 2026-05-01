#include "StickyNotesStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>

namespace {
constexpr char kStickyNotesDir[] = "/.mofei/notes";
constexpr char kStickyNotesFile[] = "/.mofei/notes/notes.json";
}  // namespace

StickyNotesStore StickyNotesStore::instance;

std::string StickyNotesStore::normalizeText(const std::string& text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = text.find_last_not_of(" \t\r\n");
  std::string normalized = text.substr(first, last - first + 1);
  if (normalized.size() > kMaxTextLength) {
    normalized.resize(kMaxTextLength);
  }
  return normalized;
}

uint32_t StickyNotesStore::allocateId() { return nextId++; }

size_t StickyNotesStore::getOpenCount() const {
  return static_cast<size_t>(
      std::count_if(notes.begin(), notes.end(), [](const StickyNote& note) { return !note.done; }));
}

size_t StickyNotesStore::getDoneCount() const { return notes.size() - getOpenCount(); }

bool StickyNotesStore::loadFromFile() {
  notes.clear();
  nextId = 1;

  Storage.mkdir("/.mofei");
  Storage.mkdir(kStickyNotesDir);

  if (!Storage.exists(kStickyNotesFile)) {
    return saveToFile();
  }

  const String json = Storage.readFile(kStickyNotesFile);
  JsonDocument doc;
  if (json.isEmpty() || deserializeJson(doc, json)) {
    LOG_ERR("NOTES", "Sticky notes config is corrupt; restoring empty list");
    return saveToFile();
  }

  nextId = doc["nextId"] | 1;
  JsonArrayConst arr = doc["notes"].as<JsonArrayConst>();
  if (!arr.isNull()) {
    for (JsonVariantConst variant : arr) {
      if (notes.size() >= kMaxNotes) {
        break;
      }
      if (!variant.is<JsonObjectConst>()) {
        continue;
      }
      JsonObjectConst item = variant.as<JsonObjectConst>();
      const char* text = item["text"] | "";
      const std::string normalized = normalizeText(text);
      if (normalized.empty()) {
        continue;
      }

      StickyNote note;
      note.id = item["id"] | 0;
      if (note.id == 0) {
        note.id = allocateId();
      }
      note.text = normalized;
      note.done = item["done"] | false;
      notes.push_back(note);
      if (note.id >= nextId) {
        nextId = note.id + 1;
      }
    }
  }

  return true;
}

bool StickyNotesStore::saveToFile() const {
  Storage.mkdir("/.mofei");
  Storage.mkdir(kStickyNotesDir);

  JsonDocument doc;
  doc["nextId"] = nextId;
  JsonArray arr = doc["notes"].to<JsonArray>();
  for (const auto& note : notes) {
    JsonObject item = arr.add<JsonObject>();
    item["id"] = note.id;
    item["text"] = note.text;
    item["done"] = note.done;
  }

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(kStickyNotesFile, json);
}

bool StickyNotesStore::addNote(const std::string& text) {
  if (notes.size() >= kMaxNotes) {
    return false;
  }
  const std::string normalized = normalizeText(text);
  if (normalized.empty()) {
    return false;
  }

  StickyNote note;
  note.id = allocateId();
  note.text = normalized;
  note.done = false;
  notes.insert(notes.begin(), note);
  return saveToFile();
}

bool StickyNotesStore::updateNote(const size_t index, const std::string& text) {
  if (index >= notes.size()) {
    return false;
  }
  const std::string normalized = normalizeText(text);
  if (normalized.empty()) {
    return deleteNote(index);
  }

  notes[index].text = normalized;
  return saveToFile();
}

bool StickyNotesStore::toggleDone(const size_t index) {
  if (index >= notes.size()) {
    return false;
  }
  notes[index].done = !notes[index].done;
  return saveToFile();
}

bool StickyNotesStore::deleteNote(const size_t index) {
  if (index >= notes.size()) {
    return false;
  }
  notes.erase(notes.begin() + static_cast<std::vector<StickyNote>::difference_type>(index));
  return saveToFile();
}

int StickyNotesStore::deleteCompleted() {
  const auto before = notes.size();
  notes.erase(std::remove_if(notes.begin(), notes.end(), [](const StickyNote& note) { return note.done; }),
              notes.end());
  const int deleted = static_cast<int>(before - notes.size());
  if (deleted > 0) {
    saveToFile();
  }
  return deleted;
}

void StickyNotesStore::clear() {
  notes.clear();
  nextId = 1;
  saveToFile();
}
