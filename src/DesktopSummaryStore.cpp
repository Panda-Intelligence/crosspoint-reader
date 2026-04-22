#include "DesktopSummaryStore.h"

#include <Arduino.h>
#include <WiFi.h>

#include "WifiCredentialStore.h"
#include "StudyStateStore.h"

DesktopSummaryStore DesktopSummaryStore::instance;

void DesktopSummaryStore::refresh() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const std::string lastSsid = WIFI_STORE.getLastConnectedSsid();
  state.isOnline = connected;

  if (connected) {
    state.city = lastSsid.empty() ? "Online" : lastSsid;
    state.weatherLine = "Connected · ready for sync";
  } else {
    state.city = lastSsid.empty() ? "Offline" : lastSsid;
    state.weatherLine = "Offline · cached data only";
  }

  state.todayPrimary = "Today";

  state.todaySecondary = connected ? "Sync calendar from companion" : "Calendar available offline";

  // Pull live study counters from StudyStateStore
  const auto& study = STUDY_STATE.getState();
  const int remaining = static_cast<int>(study.dueToday) - static_cast<int>(study.completedToday);
  state.dueCards = remaining > 0 ? remaining : 0;
  state.studyDone = static_cast<int>(study.completedToday);
}
