#pragma once

#include <cstdint>

struct StudyState {
  uint16_t dueToday = 12;
  uint16_t completedToday = 0;
  uint16_t correctToday = 0;
  uint16_t wrongToday = 0;
  uint16_t streakDays = 1;
};

class StudyStateStore {
  static StudyStateStore instance;
  StudyState state;

 public:
  static StudyStateStore& getInstance() { return instance; }

  const StudyState& getState() const { return state; }

  bool loadFromFile();
  bool saveToFile() const;
  void resetSession();
  void recordReviewResult(bool correct);
};

#define STUDY_STATE StudyStateStore::getInstance()
