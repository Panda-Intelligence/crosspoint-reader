#include "StudyHubActivity.h"

#include <HalStorage.h>
#include <I18n.h>

#include "DeckImportStatusActivity.h"
#include "LearningReportActivity.h"
#include "ReviewQueueActivity.h"
#include "SavedCardsActivity.h"
#include "StudyCardsTodayActivity.h"
#include "StudyDeckStore.h"
#include "StudyLaterActivity.h"
#include "StudyQuizActivity.h"
#include "StudyRecoveryActivity.h"
#include "StudyReviewQueueStore.h"
#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kItemCount = 8;
constexpr char kStudyDir[] = "/.mofei/study";
constexpr char kStudyStateFile[] = "/.mofei/study/state.json";

const char* itemLabel(int index) {
  switch (index) {
    case 0:
      return "Study Cards";
    case 1:
      return "Recovery";
    case 2:
      return "Quiz Practice";
    case 3:
      return "Later Cards";
    case 4:
      return "Saved Cards";
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

void StudyHubActivity::openCurrentSelection() {
  switch (selectedIndex) {
    case 0:
      activityManager.replaceActivity(std::make_unique<StudyCardsTodayActivity>(renderer, mappedInput));
      break;
    case 1:
      activityManager.replaceActivity(std::make_unique<StudyRecoveryActivity>(renderer, mappedInput));
      break;
    case 2:
      activityManager.replaceActivity(std::make_unique<StudyQuizActivity>(renderer, mappedInput));
      break;
    case 3:
      activityManager.replaceActivity(std::make_unique<StudyLaterActivity>(renderer, mappedInput));
      break;
    case 4:
      activityManager.replaceActivity(std::make_unique<SavedCardsActivity>(renderer, mappedInput));
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

void StudyHubActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (touchEvent.isTap()) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const Rect listRect{
          0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, renderer.getScreenWidth(),
          renderer.getScreenHeight() -
              (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)};
      const int clickedIndex = TouchHitTest::listItemAt(listRect, metrics.listWithSubtitleRowHeight, selectedIndex,
                                                        kItemCount, touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        openCurrentSelection();
        return;
      }
    } else {
      const auto gestureAction = TouchHitTest::listGestureActionForTouch(touchEvent);
      if (gestureAction == TouchHitTest::ListGestureAction::NextItem) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, kItemCount);
        requestUpdate();
        return;
      }
      if (gestureAction == TouchHitTest::ListGestureAction::PreviousItem) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, kItemCount);
        requestUpdate();
        return;
      }
    }
  }

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
    openCurrentSelection();
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
            return "Due today: " +
                   std::to_string(state.dueToday > state.completedToday ? state.dueToday - state.completedToday : 0);
          case 1:
            return againQueueCount > 0 ? ("Wrong cards: " + std::to_string(againQueueCount))
                                       : std::string("No recovery backlog");
          case 2:
            return importedCardCount > 1 ? std::string("Three drill modes ready")
                                         : std::string("Need at least 2 cards");
          case 3:
            return laterQueueCount > 0 ? ("Later: " + std::to_string(laterQueueCount) + "  Parked for later")
                                       : std::string("No postponed cards");
          case 4:
            return savedQueueCount > 0 ? ("Saved: " + std::to_string(savedQueueCount) + "  Keep key cards")
                                       : std::string("Save cards you want to keep");
          case 5:
            return std::string("Weak area and mastery");
          case 6:
            return "Again: " + std::to_string(againQueueCount) +
                   "  Total: " + std::to_string(againQueueCount + laterQueueCount + savedQueueCount);
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
