#pragma once

#include <string>

struct DesktopSummaryState {
  bool isOnline = false;
  std::string city = "Offline";
  std::string weatherLine = "No weather data";
  std::string todayPrimary = "No events";
  std::string todaySecondary = "Use Calendar to plan";
  int dueCards = 0;
  int studyDone = 0;
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
