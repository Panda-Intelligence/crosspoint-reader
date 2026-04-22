#include "StudyStateStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>

namespace {
constexpr char STUDY_STATE_DIR[] = "/.mofei/study";
constexpr char STUDY_STATE_FILE[] = "/.mofei/study/state.json";
}  // namespace

StudyStateStore StudyStateStore::instance;

bool StudyStateStore::loadFromFile() {
  if (!Storage.exists(STUDY_STATE_FILE)) {
    return false;
  }

  const String json = Storage.readFile(STUDY_STATE_FILE);
  if (json.isEmpty()) {
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return false;
  }

  state.dueToday = doc["dueToday"] | state.dueToday;
  state.completedToday = doc["completedToday"] | static_cast<uint16_t>(0);
  state.correctToday = doc["correctToday"] | static_cast<uint16_t>(0);
  state.wrongToday = doc["wrongToday"] | static_cast<uint16_t>(0);
  state.streakDays = doc["streakDays"] | static_cast<uint16_t>(1);
  return true;
}

bool StudyStateStore::saveToFile() const {
  Storage.mkdir("/.mofei");
  Storage.mkdir(STUDY_STATE_DIR);

  JsonDocument doc;
  doc["dueToday"] = state.dueToday;
  doc["completedToday"] = state.completedToday;
  doc["correctToday"] = state.correctToday;
  doc["wrongToday"] = state.wrongToday;
  doc["streakDays"] = state.streakDays;

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(STUDY_STATE_FILE, json);
}

void StudyStateStore::resetSession() {
  state.completedToday = 0;
  state.correctToday = 0;
  state.wrongToday = 0;
  saveToFile();
}

void StudyStateStore::recordReviewResult(bool correct) {
  if (state.completedToday < state.dueToday) {
    state.completedToday++;
  }

  if (correct) {
    state.correctToday++;
  } else {
    state.wrongToday++;
  }

  saveToFile();
}
