#pragma once

#include <string>

#include "I18nKeys.h"

struct DesktopSummaryState {
  bool isOnline = false;
  std::string city = "Offline";
  std::string weatherLine = "No weather data";
  StrId weatherConditionId = StrId::STR_WEATHER_OFFLINE;
  int temperatureC = 22;
  std::string todayPrimary = "No events";
  std::string todaySecondary = "Use Calendar to plan";
  int dueCards = 0;
  int studyDone = 0;
  int loadedCards = 0;
  int againCards = 0;
  int laterCards = 0;
  int savedCards = 0;
};

class DesktopSummaryStore {
  static DesktopSummaryStore instance;
  DesktopSummaryState state;

 public:
  static DesktopSummaryStore& getInstance() { return instance; }

  const DesktopSummaryState& getState() const { return state; }

  void refresh();
};

#define DESKTOP_SUMMARY DesktopSummaryStore::getInstance()
