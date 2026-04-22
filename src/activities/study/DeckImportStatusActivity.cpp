#include "DeckImportStatusActivity.h"

#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char kStudyDir[] = "/.mofei/study";
constexpr char kStudyStateFile[] = "/.mofei/study/state.json";

bool hasJsonSuffix(const std::string& name) {
  constexpr const char* suffix = ".json";
  constexpr size_t suffixLen = 5;
  return name.size() >= suffixLen && name.compare(name.size() - suffixLen, suffixLen, suffix) == 0;
}
}  // namespace

void DeckImportStatusActivity::refreshDeckEntries() {
  deckEntries.clear();
  hasStudyStateFile = Storage.exists(kStudyStateFile);

  const auto files = Storage.listFiles(kStudyDir);
  for (const auto& file : files) {
    const std::string filename = file.c_str();
    if (!hasJsonSuffix(filename) || filename == "state.json") {
      continue;
    }

    size_t bytes = 0;
    FsFile deckFile;
    const std::string path = std::string(kStudyDir) + "/" + filename;
    if (Storage.openFileForRead("SDS", path.c_str(), deckFile)) {
      bytes = deckFile.size();
      deckFile.close();
    }

    deckEntries.push_back({filename, bytes});
  }

  std::sort(deckEntries.begin(), deckEntries.end(),
            [](const DeckEntry& a, const DeckEntry& b) { return a.filename < b.filename; });
}

std::string DeckImportStatusActivity::sizeLabel(const size_t bytes) const {
  if (bytes < 1024) {
    return std::to_string(bytes) + " B";
  }
  if (bytes < 1024 * 1024) {
    return std::to_string(bytes / 1024) + " KB";
  }
  return std::to_string(bytes / (1024 * 1024)) + " MB";
}

void DeckImportStatusActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  refreshDeckEntries();
  requestUpdate();
}

void DeckImportStatusActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (!deckEntries.empty()) {
    buttonNavigator.onNextRelease([this] {
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(deckEntries.size()));
      requestUpdate();
    });

    buttonNavigator.onPreviousRelease([this] {
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(deckEntries.size()));
      requestUpdate();
    });
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    refreshDeckEntries();
    if (!deckEntries.empty()) {
      selectedIndex = std::min(selectedIndex, static_cast<int>(deckEntries.size()) - 1);
    } else {
      selectedIndex = 0;
    }
    requestUpdate();
  }
}

void DeckImportStatusActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Deck Import Status");

  if (deckEntries.empty()) {
    const int centerY = pageHeight / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - 36, "No imported deck file");
    renderer.drawCenteredText(UI_10_FONT_ID, centerY, "Copy .json decks to /.mofei/study");
    if (hasStudyStateFile) {
      renderer.drawCenteredText(SMALL_FONT_ID, centerY + 30, "Study state exists");
    } else {
      renderer.drawCenteredText(SMALL_FONT_ID, centerY + 30, "state.json not found");
    }
  } else {
    GUI.drawList(
        renderer,
        Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
             pageHeight -
                 (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)},
        static_cast<int>(deckEntries.size()), selectedIndex, [this](int index) { return deckEntries[index].filename; },
        [this](int index) { return "Size: " + sizeLabel(deckEntries[index].bytes); });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Refresh", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
