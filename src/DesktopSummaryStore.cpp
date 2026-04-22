#include "DesktopSummaryStore.h"

#include <Arduino.h>
#include <WiFi.h>

#include "WifiCredentialStore.h"

DesktopSummaryStore DesktopSummaryStore::instance;

void DesktopSummaryStore::refresh() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const std::string lastSsid = WIFI_STORE.getLastConnectedSsid();

  if (connected) {
    state.city = lastSsid.empty() ? "Online" : lastSsid;
    state.weatherLine = "Connected · ready for sync";
  } else {
    state.city = lastSsid.empty() ? "Offline" : lastSsid;
    state.weatherLine = "Offline · cached data only";
  }

  state.todayPrimary = "Today";

  state.todaySecondary = connected ? "Sync calendar from companion" : "Calendar available offline";

  // Placeholder study counters until the study package lands.
  state.dueCards = 0;
  state.studyDone = 0;
}
