#include "SleepWallpaperActivity.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "SleepWallpaperStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kClearSelectionRow = 0;
}  // namespace

void SleepWallpaperActivity::onEnter() {
  Activity::onEnter();
  SLEEP_WALLPAPER.loadFromFile();
  loadWallpapers();
  selectedIndex = 0;

  const auto& selected = SLEEP_WALLPAPER.getSelectedPath();
  if (!selected.empty()) {
    for (size_t i = 0; i < entries.size(); i++) {
      if (entries[i].path == selected) {
        selectedIndex = static_cast<int>(i) + 1;
        break;
      }
    }
  }
  requestUpdate();
}

void SleepWallpaperActivity::onExit() { Activity::onExit(); }

int SleepWallpaperActivity::itemCount() const { return static_cast<int>(entries.size()) + 1; }

void SleepWallpaperActivity::appendBmpFiles(const char* dirPath) {
  auto dir = Storage.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    return;
  }

  char name[500];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) {
      continue;
    }
    file.getName(name, sizeof(name));
    std::string filename{name};
    if (filename.empty() || filename[0] == '.' || !FsHelpers::hasBmpExtension(filename)) {
      continue;
    }
    WallpaperEntry entry;
    entry.path = std::string(dirPath) + "/" + filename;
    entry.label = filename;
    entries.push_back(std::move(entry));
  }
}

void SleepWallpaperActivity::loadWallpapers() {
  entries.clear();
  appendBmpFiles("/.sleep");
  appendBmpFiles("/sleep");

  if (Storage.exists("/sleep.bmp")) {
    entries.push_back({"/sleep.bmp", "sleep.bmp"});
  }

  std::sort(entries.begin(), entries.end(), [](const WallpaperEntry& a, const WallpaperEntry& b) {
    return a.label < b.label;
  });
  entries.erase(std::unique(entries.begin(), entries.end(),
                            [](const WallpaperEntry& a, const WallpaperEntry& b) { return a.path == b.path; }),
                entries.end());
}

void SleepWallpaperActivity::applySelection() {
  if (selectedIndex == kClearSelectionRow) {
    clearSelection();
    return;
  }

  const int entryIndex = selectedIndex - 1;
  if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
    return;
  }

  SLEEP_WALLPAPER.setSelectedPath(entries[static_cast<size_t>(entryIndex)].path);
  SLEEP_WALLPAPER.saveToFile();
  finish();
}

void SleepWallpaperActivity::clearSelection() {
  SLEEP_WALLPAPER.clearSelectedPath();
  SLEEP_WALLPAPER.saveToFile();
  finish();
}

void SleepWallpaperActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
      const int contentHeight =
          renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
      const Rect listRect{0, contentTop, renderer.getScreenWidth(), contentHeight};
      const int clickedIndex = TouchHitTest::listItemAt(listRect, metrics.listWithSubtitleRowHeight, selectedIndex,
                                                        itemCount(), touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        applySelection();
        return;
      }
      mappedInput.suppressTouchButtonFallback();
      return;
    }

    const auto gestureAction = TouchHitTest::listGestureActionForTouch(touchEvent);
    if (!buttonHintTap && gestureAction == TouchHitTest::ListGestureAction::NextItem) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount());
      requestUpdate();
      return;
    }
    if (!buttonHintTap && gestureAction == TouchHitTest::ListGestureAction::PreviousItem) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount());
      requestUpdate();
      return;
    }
    if (!buttonHintTap) {
      mappedInput.suppressTouchButtonFallback();
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount());
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount());
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    applySelection();
  }
}

void SleepWallpaperActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const auto& selectedPath = SLEEP_WALLPAPER.getSelectedPath();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_SLEEP_WALLPAPER_PICKER));

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, itemCount(), selectedIndex,
      [this](int index) {
        if (index == kClearSelectionRow) {
          return std::string(tr(STR_SLEEP_WALLPAPER_RANDOM));
        }
        const int entryIndex = index - 1;
        return entryIndex >= 0 && entryIndex < static_cast<int>(entries.size())
                   ? entries[static_cast<size_t>(entryIndex)].label
                   : std::string();
      },
      [this](int index) {
        if (index == kClearSelectionRow) {
          return std::string(tr(STR_SLEEP_WALLPAPER_RANDOM_SUBTITLE));
        }
        const int entryIndex = index - 1;
        return entryIndex >= 0 && entryIndex < static_cast<int>(entries.size())
                   ? entries[static_cast<size_t>(entryIndex)].path
                   : std::string();
      },
      nullptr,
      [this, &selectedPath](int index) {
        if (index == kClearSelectionRow) {
          return selectedPath.empty() ? tr(STR_SELECTED) : "";
        }
        const int entryIndex = index - 1;
        if (entryIndex >= 0 && entryIndex < static_cast<int>(entries.size()) &&
            entries[static_cast<size_t>(entryIndex)].path == selectedPath) {
          return tr(STR_SELECTED);
        }
        return "";
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
