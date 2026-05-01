#pragma once

#include <string>

class SleepWallpaperStore {
 public:
  static constexpr size_t kMaxPathLength = 192;

  static SleepWallpaperStore& getInstance() { return instance; }

  bool loadFromFile();
  bool saveToFile() const;
  bool setSelectedPath(std::string path);
  void clearSelectedPath();

  const std::string& getSelectedPath() const { return selectedPath; }
  bool hasSelectedPath() const { return !selectedPath.empty(); }
  bool selectedPathExists() const;

 private:
  static SleepWallpaperStore instance;

  std::string selectedPath;
};

#define SLEEP_WALLPAPER SleepWallpaperStore::getInstance()
