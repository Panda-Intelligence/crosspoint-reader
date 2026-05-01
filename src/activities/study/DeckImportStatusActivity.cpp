#include "DeckImportStatusActivity.h"

#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "StudyDeckStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr char kStudyStateFile[] = "/.mofei/study/state.json";
}  // namespace

void DeckImportStatusActivity::refreshDeckEntries() {
  hasStudyStateFile = Storage.exists(kStudyStateFile);
  STUDY_DECKS.refresh();
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
  const auto& deckEntries = STUDY_DECKS.getDeckSummaries();
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (deckEntries.empty()) {
      if (touchEvent.isTap()) {
        mappedInput.suppressTouchButtonFallback();
        refreshDeckEntries();
        requestUpdate();
        return;
      }
    } else if (touchEvent.isTap()) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
      const int listH =
          renderer.getScreenHeight() - (contentTop + metrics.buttonHintsHeight + metrics.verticalSpacing + 24);
      const Rect listRect{0, contentTop, renderer.getScreenWidth(), listH};
      const int clickedIndex =
          TouchHitTest::listItemAt(listRect, metrics.listWithSubtitleRowHeight, selectedIndex,
                                   static_cast<int>(deckEntries.size()), touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        refreshDeckEntries();
        const auto& refreshedDeckEntries = STUDY_DECKS.getDeckSummaries();
        selectedIndex = refreshedDeckEntries.empty()
                            ? 0
                            : std::min(selectedIndex, static_cast<int>(refreshedDeckEntries.size()) - 1);
        requestUpdate();
        return;
      }
    } else if (TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(deckEntries.size()));
      requestUpdate();
      return;
    } else if (TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(deckEntries.size()));
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (!deckEntries.empty()) {
    buttonNavigator.onNextRelease([this] {
      selectedIndex =
          ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(STUDY_DECKS.getDeckSummaries().size()));
      requestUpdate();
    });

    buttonNavigator.onPreviousRelease([this] {
      selectedIndex =
          ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(STUDY_DECKS.getDeckSummaries().size()));
      requestUpdate();
    });
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    refreshDeckEntries();
    const auto& refreshedDeckEntries = STUDY_DECKS.getDeckSummaries();
    if (!refreshedDeckEntries.empty()) {
      selectedIndex = std::min(selectedIndex, static_cast<int>(refreshedDeckEntries.size()) - 1);
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

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_STUDY_DECK_IMPORT_STATUS));

  const int pad = metrics.contentSidePadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const auto& deckEntries = STUDY_DECKS.getDeckSummaries();

  if (deckEntries.empty()) {
    // Styled empty-state card
    const int cardH = 130;
    const int cardY = (pageHeight - cardH) / 2 - 16;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);

    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 16, tr(STR_STUDY_NO_DECK_FILES_FOUND), true, EpdFontFamily::BOLD);
    renderer.drawLine(pad + 16, cardY + 46, pageWidth - pad - 16, cardY + 46, true);

    constexpr int bulletR = 3;
    const int bx = pad + 16;
    const int tx = bx + bulletR * 2 + 6;
    renderer.fillRoundedRect(bx, cardY + 58, bulletR * 2, bulletR * 2, bulletR, Color::Black);
    renderer.drawText(SMALL_FONT_ID, tx, cardY + 54, tr(STR_STUDY_COPY_JSON_DECKS_PATH));
    renderer.fillRoundedRect(bx, cardY + 82, bulletR * 2, bulletR * 2, bulletR, Color::Black);
    renderer.drawText(SMALL_FONT_ID, tx, cardY + 78,
                      hasStudyStateFile ? tr(STR_STUDY_STATE_READY) : tr(STR_STUDY_STATE_JSON_NOT_FOUND));
    renderer.fillRoundedRect(bx, cardY + 106, bulletR * 2, bulletR * 2, bulletR, Color::Black);
    renderer.drawText(SMALL_FONT_ID, tx, cardY + 102, tr(STR_STUDY_PRESS_REFRESH_AFTER_COPYING));
  } else {
    const int listH = pageHeight - (contentTop + metrics.buttonHintsHeight + metrics.verticalSpacing + 24);
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, listH}, static_cast<int>(deckEntries.size()), selectedIndex,
        [](int index) {
          const auto& entry = STUDY_DECKS.getDeckSummaries()[index];
          return entry.valid ? entry.title : (entry.filename + "  " + std::string(tr(STR_STUDY_ERROR_MARK)));
        },
        [this](int index) {
          const auto& entry = STUDY_DECKS.getDeckSummaries()[index];
          if (!entry.valid) {
            return std::string(tr(STR_STUDY_INVALID_DECK)) + "  |  " + sizeLabel(entry.bytes);
          }
          char cardsLabel[48];
          snprintf(cardsLabel, sizeof(cardsLabel), tr(STR_STUDY_CARD_COUNT_FORMAT), entry.cardCount);
          return std::string(cardsLabel) + "  |  " + sizeLabel(entry.bytes);
        });

    // Summary below list
    const int badgeY = pageHeight - metrics.buttonHintsHeight - 22;
    char countStr[96];
    snprintf(countStr, sizeof(countStr), tr(STR_STUDY_DECK_SUMMARY_FORMAT), STUDY_DECKS.getDeckCount(),
             hasStudyStateFile ? tr(STR_STUDY_STATE_READY_COMPACT) : tr(STR_STUDY_NO_STATE_FILE_COMPACT));
    renderer.drawCenteredText(SMALL_FONT_ID, badgeY, countStr);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
