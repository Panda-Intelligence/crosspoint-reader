#pragma once

#include <cstdint>
#include <ctime>

enum class ArcadeGameId : uint8_t { Sudoku, Sokoban, Memory, WordPuzzle, DailyMaze };

struct ArcadeProgressState {
  uint32_t activeDateKey = 0;
  uint32_t lastPlayedDateKey = 0;
  uint16_t sessionsToday = 0;
  uint16_t winsToday = 0;
  uint16_t playStreakDays = 0;
  uint16_t totalSessions = 0;
  uint16_t totalWins = 0;
  uint16_t dailyMax2048 = 0;
  uint16_t best2048 = 0;
  uint16_t sudokuClears = 0;
  uint16_t sokobanClears = 0;
  uint16_t memoryClears = 0;
  uint16_t wordPuzzleClears = 0;
  uint16_t dailyMazeClears = 0;
};

class ArcadeProgressStore {
  static ArcadeProgressStore instance;
  ArcadeProgressState state;

  static uint32_t buildDateKey(const tm& localTm);
  uint32_t currentDateKey() const;
  bool isPreviousDay(uint32_t previousDateKey, uint32_t currentDateKey) const;
  bool syncDay();

 public:
  static ArcadeProgressStore& getInstance() { return instance; }

  const ArcadeProgressState& getState() const { return state; }

  bool loadFromFile();
  bool saveToFile() const;
  void syncToCurrentDay();
  void recordSessionStart();
  void recordWin(ArcadeGameId gameId);
  void record2048Tile(uint16_t tile);
};

#define ARCADE_PROGRESS ArcadeProgressStore::getInstance()
