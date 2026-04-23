#include "ArcadeProgressStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>

#include <ctime>

namespace {
constexpr char ARCADE_PROGRESS_DIR[] = "/.mofei/arcade";
constexpr char ARCADE_PROGRESS_FILE[] = "/.mofei/arcade/progress.json";
}  // namespace

ArcadeProgressStore ArcadeProgressStore::instance;

uint32_t ArcadeProgressStore::buildDateKey(const tm& localTm) {
  const uint32_t year = static_cast<uint32_t>(localTm.tm_year + 1900);
  const uint32_t month = static_cast<uint32_t>(localTm.tm_mon + 1);
  const uint32_t day = static_cast<uint32_t>(localTm.tm_mday);
  return year * 10000 + month * 100 + day;
}

uint32_t ArcadeProgressStore::currentDateKey() const {
  time_t now = time(nullptr);
  tm localTm = {};
  localtime_r(&now, &localTm);
  return buildDateKey(localTm);
}

bool ArcadeProgressStore::isPreviousDay(const uint32_t previousDateKey, const uint32_t currentKey) const {
  if (previousDateKey == 0 || currentKey == 0) {
    return false;
  }

  tm previousTm = {};
  previousTm.tm_year = static_cast<int>(previousDateKey / 10000) - 1900;
  previousTm.tm_mon = static_cast<int>((previousDateKey / 100) % 100) - 1;
  previousTm.tm_mday = static_cast<int>(previousDateKey % 100);
  previousTm.tm_hour = 12;

  tm currentTm = {};
  currentTm.tm_year = static_cast<int>(currentKey / 10000) - 1900;
  currentTm.tm_mon = static_cast<int>((currentKey / 100) % 100) - 1;
  currentTm.tm_mday = static_cast<int>(currentKey % 100);
  currentTm.tm_hour = 12;

  const time_t previous = mktime(&previousTm);
  const time_t current = mktime(&currentTm);
  if (previous <= 0 || current <= 0) {
    return false;
  }

  return (current - previous) >= 86400 && (current - previous) < 172800;
}

bool ArcadeProgressStore::syncDay() {
  const uint32_t todayKey = currentDateKey();
  if (todayKey == 0) {
    return false;
  }

  if (state.activeDateKey == todayKey) {
    return false;
  }

  if (state.activeDateKey != 0) {
    if (state.sessionsToday > 0 && isPreviousDay(state.activeDateKey, todayKey)) {
      state.playStreakDays = state.playStreakDays == 0 ? 1 : static_cast<uint16_t>(state.playStreakDays + 1);
    } else if (state.sessionsToday > 0) {
      state.playStreakDays = 1;
    }
  }

  state.activeDateKey = todayKey;
  state.sessionsToday = 0;
  state.winsToday = 0;
  state.dailyMax2048 = 0;
  saveToFile();
  return true;
}

bool ArcadeProgressStore::loadFromFile() {
  if (!Storage.exists(ARCADE_PROGRESS_FILE)) {
    syncDay();
    return false;
  }

  const String json = Storage.readFile(ARCADE_PROGRESS_FILE);
  if (json.isEmpty()) {
    syncDay();
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    syncDay();
    return false;
  }

  state.activeDateKey = doc["activeDateKey"] | static_cast<uint32_t>(0);
  state.lastPlayedDateKey = doc["lastPlayedDateKey"] | static_cast<uint32_t>(0);
  state.sessionsToday = doc["sessionsToday"] | static_cast<uint16_t>(0);
  state.winsToday = doc["winsToday"] | static_cast<uint16_t>(0);
  state.playStreakDays = doc["playStreakDays"] | static_cast<uint16_t>(0);
  state.totalSessions = doc["totalSessions"] | static_cast<uint16_t>(0);
  state.totalWins = doc["totalWins"] | static_cast<uint16_t>(0);
  state.dailyMax2048 = doc["dailyMax2048"] | static_cast<uint16_t>(0);
  state.best2048 = doc["best2048"] | static_cast<uint16_t>(0);
  state.sudokuClears = doc["sudokuClears"] | static_cast<uint16_t>(0);
  state.sokobanClears = doc["sokobanClears"] | static_cast<uint16_t>(0);
  state.memoryClears = doc["memoryClears"] | static_cast<uint16_t>(0);
  state.wordPuzzleClears = doc["wordPuzzleClears"] | static_cast<uint16_t>(0);
  state.dailyMazeClears = doc["dailyMazeClears"] | static_cast<uint16_t>(0);

  syncDay();
  return true;
}

bool ArcadeProgressStore::saveToFile() const {
  Storage.mkdir("/.mofei");
  Storage.mkdir(ARCADE_PROGRESS_DIR);

  JsonDocument doc;
  doc["activeDateKey"] = state.activeDateKey;
  doc["lastPlayedDateKey"] = state.lastPlayedDateKey;
  doc["sessionsToday"] = state.sessionsToday;
  doc["winsToday"] = state.winsToday;
  doc["playStreakDays"] = state.playStreakDays;
  doc["totalSessions"] = state.totalSessions;
  doc["totalWins"] = state.totalWins;
  doc["dailyMax2048"] = state.dailyMax2048;
  doc["best2048"] = state.best2048;
  doc["sudokuClears"] = state.sudokuClears;
  doc["sokobanClears"] = state.sokobanClears;
  doc["memoryClears"] = state.memoryClears;
  doc["wordPuzzleClears"] = state.wordPuzzleClears;
  doc["dailyMazeClears"] = state.dailyMazeClears;

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(ARCADE_PROGRESS_FILE, json);
}

void ArcadeProgressStore::syncToCurrentDay() { syncDay(); }

void ArcadeProgressStore::recordSessionStart() {
  syncDay();
  state.sessionsToday++;
  state.totalSessions++;
  state.lastPlayedDateKey = state.activeDateKey;
  if (state.playStreakDays == 0) {
    state.playStreakDays = 1;
  }
  saveToFile();
}

void ArcadeProgressStore::recordWin(const ArcadeGameId gameId) {
  syncDay();
  state.winsToday++;
  state.totalWins++;

  switch (gameId) {
    case ArcadeGameId::Sudoku:
      state.sudokuClears++;
      break;
    case ArcadeGameId::Sokoban:
      state.sokobanClears++;
      break;
    case ArcadeGameId::Memory:
      state.memoryClears++;
      break;
    case ArcadeGameId::WordPuzzle:
      state.wordPuzzleClears++;
      break;
    case ArcadeGameId::DailyMaze:
      state.dailyMazeClears++;
      break;
  }

  saveToFile();
}

void ArcadeProgressStore::record2048Tile(const uint16_t tile) {
  syncDay();
  if (tile > state.dailyMax2048) {
    state.dailyMax2048 = tile;
  }
  if (tile > state.best2048) {
    state.best2048 = tile;
  }
  saveToFile();
}
