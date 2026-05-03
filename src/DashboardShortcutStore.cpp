#include "DashboardShortcutStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace {
constexpr char kDashboardShortcutDir[] = "/.mofei/dashboard";
constexpr char kDashboardShortcutFile[] = "/.mofei/dashboard/shortcuts.json";

constexpr std::array<DashboardShortcutDefinition, static_cast<size_t>(DashboardShortcutId::Count)> kDefinitions = {{
    {DashboardShortcutId::RecentReading, StrId::STR_CONTINUE_READING, StrId::STR_DASHBOARD_SUBTITLE_RECENT_READING},
    {DashboardShortcutId::ReadingHub, StrId::STR_DASHBOARD_READING, StrId::STR_DASHBOARD_SUBTITLE_READING},
    {DashboardShortcutId::StudyHub, StrId::STR_DASHBOARD_STUDY, StrId::STR_DASHBOARD_SUBTITLE_STUDY},
    {DashboardShortcutId::DesktopHub, StrId::STR_DASHBOARD_DESKTOP, StrId::STR_DASHBOARD_SUBTITLE_DESKTOP},
    {DashboardShortcutId::ArcadeHub, StrId::STR_DASHBOARD_ARCADE, StrId::STR_DASHBOARD_SUBTITLE_ARCADE},
    {DashboardShortcutId::ImportSync, StrId::STR_DASHBOARD_IMPORT, StrId::STR_DASHBOARD_SUBTITLE_IMPORT},
    {DashboardShortcutId::FileBrowser, StrId::STR_DASHBOARD_FILES, StrId::STR_DASHBOARD_SUBTITLE_FILES},
    {DashboardShortcutId::Settings, StrId::STR_SETTINGS_TITLE, StrId::STR_DASHBOARD_SUBTITLE_SETTINGS},
    {DashboardShortcutId::WeatherClock, StrId::STR_DASHBOARD_WEATHER, StrId::STR_DASHBOARD_SUBTITLE_WEATHER},
    {DashboardShortcutId::Today, StrId::STR_DASHBOARD_TODAY, StrId::STR_DASHBOARD_SUBTITLE_TODAY},
    {DashboardShortcutId::StudyToday, StrId::STR_DASHBOARD_STUDY_TODAY, StrId::STR_DASHBOARD_SUBTITLE_STUDY_TODAY},
}};

constexpr std::array<const char*, static_cast<size_t>(DashboardShortcutId::Count)> kShortcutKeys = {
    "recent_reading", "reading",  "study",   "desktop", "arcade",     "import_sync",
    "file_browser",   "settings", "weather", "today",   "study_today"};

const DashboardShortcutStore::ShortcutList kDefaultShortcuts = {
    DashboardShortcutId::RecentReading, DashboardShortcutId::ReadingHub, DashboardShortcutId::StudyHub,
    DashboardShortcutId::DesktopHub,    DashboardShortcutId::ArcadeHub,  DashboardShortcutId::ImportSync,
    DashboardShortcutId::FileBrowser,   DashboardShortcutId::Settings,   DashboardShortcutId::WeatherClock,
    DashboardShortcutId::Today,         DashboardShortcutId::StudyToday,
};

bool containsShortcut(const DashboardShortcutStore::ShortcutList& list, DashboardShortcutId id) {
  return std::find(list.begin(), list.end(), id) != list.end();
}

bool containsShortcutExcept(const DashboardShortcutStore::ShortcutList& list, DashboardShortcutId id,
                            const size_t exceptIndex) {
  for (size_t i = 0; i < list.size(); i++) {
    if (i != exceptIndex && list[i] == id) {
      return true;
    }
  }
  return false;
}

bool isValidShortcutList(const DashboardShortcutStore::ShortcutList& list) {
  std::array<bool, static_cast<size_t>(DashboardShortcutId::Count)> used = {};
  for (DashboardShortcutId id : list) {
    if (!DashboardShortcutStore::isValid(id) || used[static_cast<size_t>(id)]) {
      return false;
    }
    used[static_cast<size_t>(id)] = true;
  }
  return containsShortcut(list, DashboardShortcutId::FileBrowser) &&
         containsShortcut(list, DashboardShortcutId::Settings);
}

DashboardShortcutId firstUnusedShortcut(const std::array<bool, static_cast<size_t>(DashboardShortcutId::Count)>& used) {
  for (size_t i = 0; i < static_cast<size_t>(DashboardShortcutId::Count); i++) {
    const auto candidate = static_cast<DashboardShortcutId>(i);
    if (!used[i] && !DashboardShortcutStore::isProtected(candidate)) {
      return candidate;
    }
  }
  return DashboardShortcutId::RecentReading;
}

void ensureRequiredShortcut(DashboardShortcutStore::ShortcutList& list, const DashboardShortcutId id,
                            const size_t preferredIndex, bool& repaired) {
  if (containsShortcut(list, id)) {
    return;
  }

  const DashboardShortcutId otherProtected =
      id == DashboardShortcutId::FileBrowser ? DashboardShortcutId::Settings : DashboardShortcutId::FileBrowser;
  size_t targetIndex = preferredIndex;
  if (list[targetIndex] == otherProtected && !containsShortcutExcept(list, otherProtected, targetIndex)) {
    for (size_t i = 0; i < list.size(); i++) {
      if (!DashboardShortcutStore::isProtected(list[i])) {
        targetIndex = i;
        break;
      }
    }
  }

  list[targetIndex] = id;
  repaired = true;
}

DashboardShortcutId nextShortcutId(const DashboardShortcutId id, const int direction) {
  const int count = static_cast<int>(DashboardShortcutId::Count);
  int next = static_cast<int>(id) + (direction >= 0 ? 1 : -1);
  if (next < 0) {
    next = count - 1;
  } else if (next >= count) {
    next = 0;
  }
  return static_cast<DashboardShortcutId>(next);
}
}  // namespace

DashboardShortcutStore DashboardShortcutStore::instance;

const DashboardShortcutStore::ShortcutList& DashboardShortcutStore::defaultShortcuts() { return kDefaultShortcuts; }

bool DashboardShortcutStore::isValid(DashboardShortcutId id) {
  return static_cast<size_t>(id) < static_cast<size_t>(DashboardShortcutId::Count);
}

bool DashboardShortcutStore::isProtected(const DashboardShortcutId id) {
  return id == DashboardShortcutId::FileBrowser || id == DashboardShortcutId::Settings;
}

const DashboardShortcutDefinition* DashboardShortcutStore::definitionFor(DashboardShortcutId id) {
  if (!isValid(id)) {
    return nullptr;
  }
  return &kDefinitions[static_cast<size_t>(id)];
}

const char* DashboardShortcutStore::keyFor(DashboardShortcutId id) {
  if (!isValid(id)) {
    return "";
  }
  return kShortcutKeys[static_cast<size_t>(id)];
}

bool DashboardShortcutStore::idFromKey(const char* key, DashboardShortcutId* outId) {
  if (key == nullptr || outId == nullptr) {
    return false;
  }

  for (size_t i = 0; i < kShortcutKeys.size(); i++) {
    if (strcmp(key, kShortcutKeys[i]) == 0) {
      *outId = static_cast<DashboardShortcutId>(i);
      return true;
    }
  }
  return false;
}

DashboardShortcutId DashboardShortcutStore::getShortcut(size_t index) const {
  if (index >= shortcuts.size()) {
    return DashboardShortcutId::RecentReading;
  }
  return shortcuts[index];
}

void DashboardShortcutStore::resetToDefaults() {
  shortcuts = kDefaultShortcuts;
  saveToFile();
}

bool DashboardShortcutStore::moveSlot(size_t fromIndex, size_t toIndex) {
  if (fromIndex >= shortcuts.size() || toIndex >= shortcuts.size() || fromIndex == toIndex) {
    return false;
  }

  const DashboardShortcutId moved = shortcuts[fromIndex];
  if (fromIndex < toIndex) {
    for (size_t i = fromIndex; i < toIndex; i++) {
      shortcuts[i] = shortcuts[i + 1];
    }
  } else {
    for (size_t i = fromIndex; i > toIndex; i--) {
      shortcuts[i] = shortcuts[i - 1];
    }
  }
  shortcuts[toIndex] = moved;
  return saveToFile();
}

bool DashboardShortcutStore::cycleSlot(const size_t index, const int direction) {
  if (index >= shortcuts.size() || direction == 0) {
    return false;
  }
  normalizeAndRepair();
  if (isProtected(shortcuts[index])) {
    return false;
  }

  DashboardShortcutId candidate = shortcuts[index];
  for (size_t attempts = 0; attempts < static_cast<size_t>(DashboardShortcutId::Count); attempts++) {
    candidate = nextShortcutId(candidate, direction);
    if (isProtected(candidate) || containsShortcutExcept(shortcuts, candidate, index)) {
      continue;
    }
    shortcuts[index] = candidate;
    return saveToFile();
  }
  return false;
}

bool DashboardShortcutStore::loadFromFile() {
  shortcuts = kDefaultShortcuts;

  bool shouldSave = false;
  if (!Storage.exists(kDashboardShortcutFile)) {
    shouldSave = true;
  } else {
    const String json = Storage.readFile(kDashboardShortcutFile);
    JsonDocument doc;
    if (json.isEmpty() || deserializeJson(doc, json)) {
      LOG_ERR("DSH", "Dashboard shortcut config is corrupt; restoring defaults");
      shouldSave = true;
    } else {
      JsonArrayConst arr = doc["shortcuts"].as<JsonArrayConst>();
      if (arr.isNull() || arr.size() != shortcuts.size()) {
        shouldSave = true;
      } else {
        ShortcutList parsed = kDefaultShortcuts;
        size_t index = 0;
        bool configValid = true;
        for (JsonVariantConst item : arr) {
          DashboardShortcutId id = DashboardShortcutId::RecentReading;
          if (item.is<const char*>()) {
            const char* key = item.as<const char*>();
            if (!idFromKey(key, &id)) {
              configValid = false;
              continue;
            }
          } else if (item.is<int>()) {
            const int numericId = item.as<int>();
            if (numericId < 0 || numericId >= static_cast<int>(DashboardShortcutId::Count)) {
              configValid = false;
              continue;
            }
            id = static_cast<DashboardShortcutId>(numericId);
          } else {
            configValid = false;
            continue;
          }
          parsed[index++] = id;
        }
        if (configValid && isValidShortcutList(parsed)) {
          shortcuts = parsed;
        } else {
          LOG_ERR("DSH", "Dashboard shortcut config is invalid; restoring defaults");
          shouldSave = true;
        }
      }
    }
  }
  if (shouldSave) {
    saveToFile();
  }
  return !shouldSave;
}

bool DashboardShortcutStore::saveToFile() const {
  Storage.mkdir("/.mofei");
  Storage.mkdir(kDashboardShortcutDir);

  JsonDocument doc;
  JsonArray arr = doc["shortcuts"].to<JsonArray>();
  for (DashboardShortcutId id : shortcuts) {
    arr.add(keyFor(id));
  }

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(kDashboardShortcutFile, json);
}

bool DashboardShortcutStore::normalizeAndRepair() {
  bool repaired = false;

  for (size_t i = 0; i < shortcuts.size(); i++) {
    if (!isValid(shortcuts[i])) {
      shortcuts[i] = kDefaultShortcuts[i];
      repaired = true;
    }
  }

  std::array<bool, static_cast<size_t>(DashboardShortcutId::Count)> used = {};
  used.fill(false);
  for (size_t i = 0; i < shortcuts.size(); i++) {
    DashboardShortcutId id = shortcuts[i];
    if (!isValid(id) || used[static_cast<size_t>(id)]) {
      shortcuts[i] = firstUnusedShortcut(used);
      repaired = true;
    }
    used[static_cast<size_t>(shortcuts[i])] = true;
  }

  ensureRequiredShortcut(shortcuts, DashboardShortcutId::FileBrowser, shortcuts.size() - 2, repaired);
  ensureRequiredShortcut(shortcuts, DashboardShortcutId::Settings, shortcuts.size() - 1, repaired);

  return repaired;
}
