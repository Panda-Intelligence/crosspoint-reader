#include "StudyHubActivity.h"

#include <HalStorage.h>
#include <I18n.h>

#include "DeckImportStatusActivity.h"
#include "LearningReportActivity.h"
#include "ReviewQueueActivity.h"
#include "SavedCardsActivity.h"
#include "StudyLaterActivity.h"
#include "StudyRecoveryActivity.h"
#include "StudyDeckStore.h"
#include "StudyQuizActivity.h"
#include "StudyReviewQueueStore.h"
#include "StudyCardsTodayActivity.h"
#include "StudyStateStore.h"
#include "components/UITheme.h"

namespace {
constexpr int kItemCount = 8;
constexpr char kStudyDir[] = "/.mofei/study";
constexpr char kStudyStateFile[] = "/.mofei/study/state.json";

const char* itemLabel(int index) {
  switch (index) {
    case 0:
      return "Study Cards";
    case 1:
      return "Quiz Practice";
    case 2:
      return "Saved Cards";
    case 3:
      return "Later Cards";
    case 4:
      return "Recovery";
    case 5:
      return "Learning Report";
    case 6:
      return "Review Queue";
    case 7:
    default:
      return "Deck Import Status";
  }
}
}  // namespace

void StudyHubActivity::refreshImportStatus() {
  hasStudyStateFile = Storage.exists(kStudyStateFile);
  STUDY_DECKS.refresh();
  importedDeckCount = STUDY_DECKS.getDeckCount();
  importedCardCount = static_cast<int>(STUDY_DECKS.getCards().size());
  deckErrorCount = STUDY_DECKS.getErrorCount();
  STUDY_REVIEW_QUEUE.loadFromFile();
  againQueueCount = STUDY_REVIEW_QUEUE.getAgainCount();
  laterQueueCount = STUDY_REVIEW_QUEUE.getLaterCount();
  savedQueueCount = STUDY_REVIEW_QUEUE.getSavedCount();
}

void StudyHubActivity::onEnter() {
  Activity::onEnter();
  STUDY_STATE.loadFromFile();
  refreshImportStatus();
  selectedIndex = 0;
  requestUpdate();
}

void StudyHubActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, kItemCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, kItemCount);
    requestUpdate();
  });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    switch (selectedIndex) {
      case 0:
        activityManager.replaceActivity(std::make_unique<StudyCardsTodayActivity>(renderer, mappedInput));
        break;
      case 1:
        activityManager.replaceActivity(std::make_unique<StudyQuizActivity>(renderer, mappedInput));
        break;
      case 2:
        activityManager.replaceActivity(std::make_unique<SavedCardsActivity>(renderer, mappedInput));
        break;
      case 3:
        activityManager.replaceActivity(std::make_unique<StudyLaterActivity>(renderer, mappedInput));
        break;
      case 4:
        activityManager.replaceActivity(std::make_unique<StudyRecoveryActivity>(renderer, mappedInput));
        break;
      case 5:
        activityManager.replaceActivity(std::make_unique<LearningReportActivity>(renderer, mappedInput));
        break;
      case 6:
        activityManager.replaceActivity(std::make_unique<ReviewQueueActivity>(renderer, mappedInput));
        break;
      case 7:
      default:
        activityManager.replaceActivity(std::make_unique<DeckImportStatusActivity>(renderer, mappedInput));
        break;
    }
  }
}

void StudyHubActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& state = STUDY_STATE.getState();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Study");
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
           pageHeight -
               (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)},
      kItemCount, selectedIndex, [](int index) { return std::string(itemLabel(index)); },
      [this, &state](int index) {
        switch (index) {
          case 0:
            if (importedCardCount <= 0) {
              return std::string("Import deck files to start");
            }
            return "Cards: " + std::to_string(importedCardCount) + "  Again: " + std::to_string(againQueueCount);
          case 1:
            return importedCardCount > 1 ? std::string("Two-choice drills from real cards")
                                         : std::string("Need at least 2 cards");
          case 2:
            return "Saved: " + std::to_string(savedQueueCount) + "  Keep key cards";
          case 3:
            return "Later: " + std::to_string(laterQueueCount) + "  Parked for later";
          case 4:
            return "Again: " + std::to_string(againQueueCount) + "  Recovery drill";
          case 5:
            return "Later: " + std::to_string(laterQueueCount) + "  Saved: " + std::to_string(savedQueueCount);
          case 6:
            return "Again: " + std::to_string(againQueueCount) + "  Total: " +
                   std::to_string(againQueueCount + laterQueueCount + savedQueueCount);
          case 7:
          default:
            if (importedDeckCount > 0) {
              return "Decks: " + std::to_string(importedDeckCount) + "  Errors: " + std::to_string(deckErrorCount);
            }
            return hasStudyStateFile ? std::string("Study state ready") : std::string("No deck imported yet");
        }
      });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Open", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
