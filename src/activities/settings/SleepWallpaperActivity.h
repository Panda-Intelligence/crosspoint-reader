#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

#include <string>
#include <vector>

class SleepWallpaperActivity final : public Activity {
 public:
  explicit SleepWallpaperActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SleepWallpaper", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct WallpaperEntry {
    std::string path;
    std::string label;
  };

  ButtonNavigator buttonNavigator;
  std::vector<WallpaperEntry> entries;
  int selectedIndex = 0;

  void loadWallpapers();
  void appendBmpFiles(const char* dirPath);
  void applySelection();
  void clearSelection();
  int itemCount() const;
};
