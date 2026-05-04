#pragma once

#include <I18n.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "components/themes/BaseTheme.h"

enum class DashboardShortcutId : uint8_t {
  RecentReading = 0,
  ReadingHub = 1,
  StudyHub = 2,
  DesktopHub = 3,
  ArcadeHub = 4,
  ImportSync = 5,
  FileBrowser = 6,
  Settings = 7,
  WeatherClock = 8,
  Today = 9,
  StudyToday = 10,
  Count
};

struct DashboardShortcutDefinition {
  DashboardShortcutId id;
  StrId labelId;
  StrId subtitleId;
  UIIcon icon;
};

class DashboardShortcutStore {
 public:
  static constexpr size_t SLOT_COUNT = 9;
  using ShortcutList = std::array<DashboardShortcutId, SLOT_COUNT>;

  static DashboardShortcutStore& getInstance() { return instance; }

  bool loadFromFile();
  bool saveToFile() const;
  void resetToDefaults();
  bool moveSlot(size_t fromIndex, size_t toIndex);
  bool cycleSlot(size_t index, int direction);

  const ShortcutList& getShortcuts() const { return shortcuts; }
  DashboardShortcutId getShortcut(size_t index) const;

  static const ShortcutList& defaultShortcuts();
  static const DashboardShortcutDefinition* definitionFor(DashboardShortcutId id);
  static const char* keyFor(DashboardShortcutId id);
  static bool idFromKey(const char* key, DashboardShortcutId* outId);
  static bool isValid(DashboardShortcutId id);
  static bool isAvailable(DashboardShortcutId id);
  static bool isProtected(DashboardShortcutId id);

 private:
  static DashboardShortcutStore instance;

  ShortcutList shortcuts = defaultShortcuts();
  bool normalizeAndRepair();
};

#define DASHBOARD_SHORTCUTS DashboardShortcutStore::getInstance()
