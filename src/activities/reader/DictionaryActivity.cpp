#include "DictionaryActivity.h"

#include <I18n.h>

#include <array>

#include "StudyDeckStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
struct DictionaryEntry {
  const char* word;
  const char* meaning;
};

constexpr std::array<DictionaryEntry, 5> kEntries = {{
    {"ephemeral", "Lasting for a very short time"},
    {"ubiquitous", "Present everywhere"},
    {"sanguine", "Optimistic in a difficult situation"},
    {"gregarious", "Fond of company; sociable"},
    {"pedagogue", "A strict or formal teacher"},
}};

Rect dictionaryListRect(const GfxRenderer& renderer) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, renderer.getScreenWidth(),
              renderer.getScreenHeight() - (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight +
                                            metrics.verticalSpacing * 2)};
}

void toggleDictionaryDetail(bool& showingDetail, Activity& activity) {
  showingDetail = !showingDetail;
  activity.requestUpdate();
}
}  // namespace

void DictionaryActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  showingDetail = false;
  statusMessage = StrId::STR_DICTIONARY_SAVE_HINT;
  requestUpdate();
}

void DictionaryActivity::saveSelectedWord() {
  const DictionaryEntry& entry = kEntries[std::clamp(selectedIndex, 0, static_cast<int>(kEntries.size()) - 1)];
  const StudyUserWordSaveResult result = STUDY_DECKS.addUserWord(entry.word, entry.meaning);
  switch (result) {
    case StudyUserWordSaveResult::Added:
      statusMessage = StrId::STR_DICTIONARY_SAVED_TO_WORDS;
      break;
    case StudyUserWordSaveResult::AlreadyExists:
      statusMessage = StrId::STR_DICTIONARY_ALREADY_IN_WORDS;
      break;
    case StudyUserWordSaveResult::Failed:
    default:
      statusMessage = StrId::STR_DICTIONARY_SAVE_FAILED;
      break;
  }
  showingDetail = true;
  requestUpdate();
}

void DictionaryActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && !showingDetail && touchEvent.isTap()) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int clickedIndex =
          TouchHitTest::listItemAt(dictionaryListRect(renderer), metrics.listWithSubtitleRowHeight, selectedIndex,
                                   static_cast<int>(kEntries.size()), touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        toggleDictionaryDetail(showingDetail, *this);
        return;
      }
    } else if (!buttonHintTap && !showingDetail && TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(kEntries.size()));
      requestUpdate();
      return;
    } else if (!buttonHintTap && !showingDetail && TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(kEntries.size()));
      requestUpdate();
      return;
    } else if (!buttonHintTap && showingDetail && touchEvent.isTap() &&
               TouchHitTest::pointInRect(
                   touchEvent.x, touchEvent.y,
                   Rect{0, 0, renderer.getScreenWidth(),
                        renderer.getScreenHeight() - UITheme::getInstance().getMetrics().buttonHintsHeight})) {
      mappedInput.suppressTouchButtonFallback();
      saveSelectedWord();
      return;
    } else if (buttonHintTap) {
      uint8_t buttonIndex = 0;
#if MOFEI_DEVICE
      if (gpio.mapMofeiButtonHintTapToButton(touchEvent.sourceX(), touchEvent.sourceY(), &buttonIndex)) {
        if (buttonIndex == HalGPIO::BTN_BACK) {
          if (showingDetail) {
            showingDetail = false;
            statusMessage = StrId::STR_DICTIONARY_SAVE_HINT;
            requestUpdate();
          } else {
            finish();
          }
        } else if (buttonIndex == HalGPIO::BTN_CONFIRM) {
          if (showingDetail) {
            saveSelectedWord();
          } else {
            toggleDictionaryDetail(showingDetail, *this);
          }
        } else if (buttonIndex == HalGPIO::BTN_LEFT) {
          selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(kEntries.size()));
          statusMessage = StrId::STR_DICTIONARY_SAVE_HINT;
          requestUpdate();
        } else if (buttonIndex == HalGPIO::BTN_RIGHT) {
          selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(kEntries.size()));
          statusMessage = StrId::STR_DICTIONARY_SAVE_HINT;
          requestUpdate();
        }
      }
#endif
      mappedInput.suppressTouchButtonFallback();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (showingDetail) {
      showingDetail = false;
      statusMessage = StrId::STR_DICTIONARY_SAVE_HINT;
      requestUpdate();
    } else {
      finish();
    }
    return;
  }

  if (!showingDetail) {
    buttonNavigator.onNextRelease([this] {
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(kEntries.size()));
      statusMessage = StrId::STR_DICTIONARY_SAVE_HINT;
      requestUpdate();
    });

    buttonNavigator.onPreviousRelease([this] {
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(kEntries.size()));
      statusMessage = StrId::STR_DICTIONARY_SAVE_HINT;
      requestUpdate();
    });
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (showingDetail) {
      saveSelectedWord();
    } else {
      toggleDictionaryDetail(showingDetail, *this);
    }
  }
}

void DictionaryActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  std::string headerTitle = tr(STR_DICTIONARY);
  if (hasQueryWord && !queryWord.empty()) {
    headerTitle += ": " + queryWord;
  }
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, headerTitle.c_str());

  if (!showingDetail) {
    GUI.drawList(
        renderer, dictionaryListRect(renderer), static_cast<int>(kEntries.size()), selectedIndex,
        [](int index) { return std::string(kEntries[index].word); },
        [](int index) { return std::string(kEntries[index].meaning); });
  } else {
    const int centerY = renderer.getTextYForCentering(0, pageHeight, UI_12_FONT_ID);
    renderer.drawCenteredText(UI_12_FONT_ID, centerY - 40, kEntries[selectedIndex].word, true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, centerY + 10, kEntries[selectedIndex].meaning);
    renderer.drawCenteredText(SMALL_FONT_ID, centerY + 50, I18N.get(statusMessage));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), showingDetail ? tr(STR_STUDY_ACTION_SAVE) : tr(STR_OPEN),
                                            tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
