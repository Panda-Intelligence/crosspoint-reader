#include "StudyHubActivity.h"

#include <HalStorage.h>
#include <I18n.h>

#include <cstdio>

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

StrId itemLabelId(int index) {
  switch (index) {
    case 0:
      return StrId::STR_STUDY_CARDS;
    case 1:
      return StrId::STR_STUDY_RECOVERY;
    case 2:
      return StrId::STR_STUDY_QUIZ_PRACTICE;
    case 3:
      return StrId::STR_STUDY_LATER_CARDS;
    case 4:
      return StrId::STR_STUDY_SAVED_CARDS;
    case 5:
      return StrId::STR_STUDY_LEARNING_REPORT;
    case 6:
      return StrId::STR_STUDY_REVIEW_QUEUE;
    case 7:
    default:
      return StrId::STR_STUDY_DECK_IMPORT_STATUS;
  }
}

std::string formatString(StrId id, int value) {
  char buffer[96];
  snprintf(buffer, sizeof(buffer), I18n::getInstance().get(id), value);
  return std::string(buffer);
}

std::string formatString(StrId id, int first, int second) {
  char buffer[112];
  snprintf(buffer, sizeof(buffer), I18n::getInstance().get(id), first, second);
  return std::string(buffer);
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

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_STUDY));
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, pageWidth,
           pageHeight -
               (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)},
      kItemCount, selectedIndex, [](int index) { return std::string(I18n::getInstance().get(itemLabelId(index))); },
      [this, &state](int index) {
        switch (index) {
          case 0:
            if (importedCardCount <= 0) {
              return std::string(tr(STR_STUDY_IMPORT_DECKS_TO_START));
            }
            return formatString(StrId::STR_STUDY_DUE_TODAY_FORMAT,
                                state.dueToday > state.completedToday ? state.dueToday - state.completedToday : 0);
          case 1:
            return againQueueCount > 0 ? formatString(StrId::STR_STUDY_WRONG_CARDS_FORMAT, againQueueCount)
                                       : std::string(tr(STR_STUDY_NO_RECOVERY_BACKLOG));
          case 2:
            return importedCardCount > 1 ? std::string(tr(STR_STUDY_THREE_DRILL_MODES_READY))
                                         : std::string(tr(STR_STUDY_NEED_AT_LEAST_2_CARDS));
          case 3:
            return laterQueueCount > 0 ? formatString(StrId::STR_STUDY_LATER_PARKED_FORMAT, laterQueueCount)
                                       : std::string(tr(STR_STUDY_NO_POSTPONED_CARDS));
          case 4:
            return savedQueueCount > 0 ? formatString(StrId::STR_STUDY_SAVED_KEEP_FORMAT, savedQueueCount)
                                       : std::string(tr(STR_STUDY_SAVE_CARDS_TO_KEEP));
          case 5:
            return std::string(tr(STR_STUDY_WEAK_AREA_AND_MASTERY));
          case 6:
            return formatString(StrId::STR_STUDY_AGAIN_TOTAL_FORMAT, againQueueCount,
                                againQueueCount + laterQueueCount + savedQueueCount);
          case 7:
          default:
            if (importedDeckCount > 0) {
              return formatString(StrId::STR_STUDY_DECKS_ERRORS_FORMAT, importedDeckCount, deckErrorCount);
            }
            return hasStudyStateFile ? std::string(tr(STR_STUDY_STATE_READY))
                                     : std::string(tr(STR_STUDY_NO_DECK_IMPORTED_YET));
        }
      });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
