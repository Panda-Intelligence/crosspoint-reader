#include "SleepWallpaperStore.h"

#include <ArduinoJson.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>

#include <utility>

namespace {
constexpr char kSleepWallpaperDir[] = "/.mofei/sleep";
constexpr char kSleepWallpaperFile[] = "/.mofei/sleep/wallpaper.json";
}  // namespace

SleepWallpaperStore SleepWallpaperStore::instance;

bool SleepWallpaperStore::loadFromFile() {
  selectedPath.clear();

  Storage.mkdir("/.mofei");
  Storage.mkdir(kSleepWallpaperDir);

  if (!Storage.exists(kSleepWallpaperFile)) {
    return saveToFile();
  }

  const String json = Storage.readFile(kSleepWallpaperFile);
  JsonDocument doc;
  if (json.isEmpty() || deserializeJson(doc, json)) {
    LOG_ERR("SLPWP", "Sleep wallpaper config is corrupt; restoring empty selection");
    return saveToFile();
  }

  const char* path = doc["selectedPath"] | "";
  return setSelectedPath(path);
}

bool SleepWallpaperStore::saveToFile() const {
  Storage.mkdir("/.mofei");
  Storage.mkdir(kSleepWallpaperDir);

  JsonDocument doc;
  doc["selectedPath"] = selectedPath;

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(kSleepWallpaperFile, json);
}

bool SleepWallpaperStore::setSelectedPath(std::string path) {
  if (path.empty()) {
    clearSelectedPath();
    return true;
  }

  if (path.size() > kMaxPathLength || path[0] != '/' || !FsHelpers::hasBmpExtension(path)) {
    return false;
  }

  selectedPath = std::move(path);
  return true;
}

void SleepWallpaperStore::clearSelectedPath() { selectedPath.clear(); }

bool SleepWallpaperStore::selectedPathExists() const {
  return hasSelectedPath() && Storage.exists(selectedPath.c_str());
}
