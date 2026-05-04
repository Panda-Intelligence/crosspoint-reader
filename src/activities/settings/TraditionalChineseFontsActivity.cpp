#include "TraditionalChineseFontsActivity.h"

#include <HalStorage.h>
#include <I18n.h>

#include <string>
#include <vector>

#include "StorageFontRegistry.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
std::vector<const TraditionalChineseFontPackInfo*> loadablePacks() {
  std::vector<const TraditionalChineseFontPackInfo*> packs;
  const auto& allPacks = StorageFontRegistry::getTraditionalChineseFontPacks();
  packs.reserve(allPacks.size());
  for (const auto& pack : allPacks) {
    if (StorageFontRegistry::isTraditionalChineseFontLoadSupportedById(pack.fontId)) {
      packs.push_back(&pack);
    }
  }
  return packs;
}

size_t reloadRowIndex() { return loadablePacks().size(); }

std::string loadedSummary() {
  const auto total = loadablePacks().size();
  const auto loaded = StorageFontRegistry::countLoadedTraditionalChineseFonts();
  return std::to_string(loaded) + "/" + std::to_string(total);
}

std::string packStatus(const TraditionalChineseFontPackInfo& pack) {
  const auto loaded = StorageFontRegistry::countLoadedTraditionalChineseStylesById(pack.fontId);
  if (loaded > 0) {
    return std::to_string(loaded) + "/4";
  }
  const auto installed = StorageFontRegistry::countInstalledTraditionalChineseStylesById(pack.fontId);
  if (installed > 0) {
    return "0/4";
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
  const auto itemCount = static_cast<int>(reloadRowIndex() + 1);
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (touchEvent.isTap()) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
      const int contentHeight =
          renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
      const Rect listRect{0, contentTop, renderer.getScreenWidth(), contentHeight};
      const int clickedIndex =
          TouchHitTest::listItemAt(listRect, metrics.listWithSubtitleRowHeight, static_cast<int>(selectedIndex),
                                   itemCount, touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = static_cast<size_t>(clickedIndex);
        handleSelection();
        requestUpdate();
        return;
      }
    } else if (TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = static_cast<size_t>(ButtonNavigator::nextIndex(static_cast<int>(selectedIndex), itemCount));
      requestUpdate();
      return;
    } else if (TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = static_cast<size_t>(ButtonNavigator::previousIndex(static_cast<int>(selectedIndex), itemCount));
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

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

#include "../../ui_font_manager.h"

void TraditionalChineseFontsActivity::handleSelection() {
  const auto packs = loadablePacks();
  if (selectedIndex < packs.size()) {
    StorageFontRegistry::loadTraditionalChineseFontById(renderer, packs[selectedIndex]->fontId);
    updateUiFontMapping();
    requestUpdate();
    return;
  }

  if (selectedIndex != reloadRowIndex()) {
    return;
  }

  StorageFontRegistry::loadTraditionalChineseFonts(renderer);
  updateUiFontMapping();
  requestUpdate();
}

void TraditionalChineseFontsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto packs = loadablePacks();
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
          return std::string(packs[static_cast<size_t>(index)]->name);
        }
        return std::string(tr(STR_RELOAD_PACKS));
      },
      [&packs](int index) {
        if (index < static_cast<int>(packs.size())) {
          return std::string(packs[static_cast<size_t>(index)]->path);
        }
        return std::string("/.mofei/fonts");
      },
      nullptr,
      [&packs](int index) {
        if (index < static_cast<int>(packs.size())) {
          return packStatus(*packs[static_cast<size_t>(index)]);
        }
        return loadedSummary();
      },
      true);

  const char* confirmLabel = (selectedIndex == reloadRowIndex()) ? tr(STR_RELOAD_PACKS) : tr(STR_SELECT);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
