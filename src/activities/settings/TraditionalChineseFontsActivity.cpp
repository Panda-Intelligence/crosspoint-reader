#include "TraditionalChineseFontsActivity.h"

#include <HalStorage.h>
#include <I18n.h>

#include <string>

#include "StorageFontRegistry.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
size_t reloadRowIndex() { return StorageFontRegistry::getTraditionalChineseFontPacks().size(); }

std::string loadedSummary() {
  const auto total = StorageFontRegistry::getTraditionalChineseFontPacks().size();
  const auto loaded = StorageFontRegistry::countLoadedTraditionalChineseFonts();
  return std::to_string(loaded) + "/" + std::to_string(total);
}

std::string packStatus(const TraditionalChineseFontPackInfo& pack) {
  if (StorageFontRegistry::isTraditionalChineseFontLoaded(pack.size)) {
    return tr(STR_LOADED);
  }
  if (Storage.exists(pack.path)) {
    return tr(STR_INSTALLED);
  }
  return tr(STR_MISSING);
}
}  // namespace

void TraditionalChineseFontsActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void TraditionalChineseFontsActivity::onExit() { Activity::onExit(); }

void TraditionalChineseFontsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  const auto itemCount = static_cast<int>(reloadRowIndex() + 1);
  buttonNavigator.onNextRelease([this, itemCount] {
    selectedIndex = static_cast<size_t>(ButtonNavigator::nextIndex(static_cast<int>(selectedIndex), itemCount));
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, itemCount] {
    selectedIndex = static_cast<size_t>(ButtonNavigator::previousIndex(static_cast<int>(selectedIndex), itemCount));
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, itemCount] {
    selectedIndex = static_cast<size_t>(ButtonNavigator::nextIndex(static_cast<int>(selectedIndex), itemCount));
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, itemCount] {
    selectedIndex = static_cast<size_t>(ButtonNavigator::previousIndex(static_cast<int>(selectedIndex), itemCount));
    requestUpdate();
  });
}

void TraditionalChineseFontsActivity::handleSelection() {
  if (selectedIndex != reloadRowIndex()) {
    return;
  }

  StorageFontRegistry::loadTraditionalChineseFonts(renderer);
  requestUpdate();
}

void TraditionalChineseFontsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& packs = StorageFontRegistry::getTraditionalChineseFontPacks();
  const auto itemCount = static_cast<int>(packs.size() + 1);
  const std::string summary = loadedSummary();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TC_FONT_PACKS),
                 summary.c_str());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, itemCount, static_cast<int>(selectedIndex),
      [&packs](int index) {
        if (index < static_cast<int>(packs.size())) {
          return std::string(packs[static_cast<size_t>(index)].name);
        }
        return std::string(tr(STR_RELOAD_PACKS));
      },
      [&packs](int index) {
        if (index < static_cast<int>(packs.size())) {
          return std::string(packs[static_cast<size_t>(index)].path);
        }
        return std::string("/.mofei/fonts");
      },
      nullptr,
      [&packs](int index) {
        if (index < static_cast<int>(packs.size())) {
          return packStatus(packs[static_cast<size_t>(index)]);
        }
        return loadedSummary();
      },
      true);

  const char* confirmLabel = (selectedIndex == reloadRowIndex()) ? tr(STR_RELOAD_PACKS) : tr(STR_SELECT);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
