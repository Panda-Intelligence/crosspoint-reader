#include "StudyRecoveryActivity.h"

#include <I18n.h>

#include <algorithm>

#include "LearningReportActivity.h"
#include "SavedCardsActivity.h"
#include "StudyLaterActivity.h"
#include "StudyReviewQueueStore.h"
#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kActionCount = 2;
constexpr StrId kActionIds[kActionCount] = {StrId::STR_STUDY_ACTION_KNOW, StrId::STR_STUDY_ACTION_AGAIN};

void drawActionButton(const GfxRenderer& renderer, int x, int y, int w, int h, const char* label, bool selected) {
  if (selected) {
    renderer.fillRoundedRect(x, y, w, h, 8, Color::Black);
  } else {
    renderer.drawRoundedRect(x, y, w, h, 1, 8, true);
  }

  const int tw = renderer.getTextWidth(SMALL_FONT_ID, label, selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  const int th = renderer.getTextHeight(SMALL_FONT_ID);
  renderer.drawText(SMALL_FONT_ID, x + (w - tw) / 2, y + (h - th) / 2, label, !selected,
                    selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
}
}  // namespace

StudyRecoveryActivity::NextStep StudyRecoveryActivity::recommendedNextStep() const {
  if (STUDY_REVIEW_QUEUE.getLaterCount() > 0) {
    return NextStep::Later;
  }
  if (STUDY_REVIEW_QUEUE.getSavedCount() > 0) {
    return NextStep::Saved;
  }
  return NextStep::Report;
}

StrId StudyRecoveryActivity::nextStepLabelId() const {
  switch (recommendedNextStep()) {
    case NextStep::Later:
      return StrId::STR_STUDY_NEXT_LATER_CARDS;
    case NextStep::Saved:
      return StrId::STR_STUDY_NEXT_SAVED_CARDS;
    case NextStep::Report:
    default:
      return StrId::STR_STUDY_NEXT_LEARNING_REPORT;
  }
}

StrId StudyRecoveryActivity::nextStepHintId() const {
  switch (recommendedNextStep()) {
    case NextStep::Later:
      return StrId::STR_STUDY_HINT_CONVERT_POSTPONED_NEXT;
    case NextStep::Saved:
      return StrId::STR_STUDY_HINT_REVIEW_SAVED_CARDS;
    case NextStep::Report:
    default:
      return StrId::STR_STUDY_HINT_CHECK_MASTERY;
  }
}

void StudyRecoveryActivity::openRecommendedNextStep() {
  switch (recommendedNextStep()) {
    case NextStep::Later:
      activityManager.replaceActivity(std::make_unique<StudyLaterActivity>(renderer, mappedInput));
      break;
    case NextStep::Saved:
      activityManager.replaceActivity(std::make_unique<SavedCardsActivity>(renderer, mappedInput));
      break;
    case NextStep::Report:
    default:
      activityManager.replaceActivity(std::make_unique<LearningReportActivity>(renderer, mappedInput));
      break;
  }
}

void StudyRecoveryActivity::applySelectedAction() {
  const auto& cards = STUDY_REVIEW_QUEUE.getCards(StudyQueueKind::Again);
  if (cards.empty()) {
    return;
  }
  const auto card = cards[std::clamp(selectedIndex, 0, static_cast<int>(cards.size()) - 1)];
  if (actionIndex == 0) {
    STUDY_STATE.recordReviewResult(true);
    STUDY_REVIEW_QUEUE.removeAt(StudyQueueKind::Again, selectedIndex);
    completedCount++;
  } else {
    STUDY_STATE.recordReviewResult(false);
    STUDY_REVIEW_QUEUE.recordAgain({card.front, card.back, "", card.deckName});
  }

  STUDY_REVIEW_QUEUE.loadFromFile();
  const int count = static_cast<int>(STUDY_REVIEW_QUEUE.getCards(StudyQueueKind::Again).size());
  if (count <= 0) {
    selectedIndex = 0;
    sessionComplete = true;
  } else {
    selectedIndex = std::min(selectedIndex, count - 1);
  }
  showingBack = false;
  requestUpdate();
}

void StudyRecoveryActivity::onEnter() {
  Activity::onEnter();
  STUDY_REVIEW_QUEUE.loadFromFile();
  STUDY_STATE.loadFromFile();
  showingBack = false;
  sessionComplete = false;
  selectedIndex = 0;
  actionIndex = 0;
  completedCount = 0;
  requestUpdate();
}

void StudyRecoveryActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    const auto& cards = STUDY_REVIEW_QUEUE.getCards(StudyQueueKind::Again);
    if (!buttonHintTap && touchEvent.isTap()) {
      mappedInput.suppressTouchButtonFallback();
      if (cards.empty()) {
        if (sessionComplete) {
          openRecommendedNextStep();
        }
        return;
      }
      if (!showingBack) {
        showingBack = true;
        actionIndex = 0;
        requestUpdate();
        return;
      }

      const auto& metrics = UITheme::getInstance().getMetrics();
      const int pageWidth = renderer.getScreenWidth();
      const int pageHeight = renderer.getScreenHeight();
      const int pad = metrics.contentSidePadding;
      const int contentBottom = pageHeight - metrics.buttonHintsHeight - 8;
      const int btnW = (pageWidth - pad * 2 - 18) / 2;
      const int btnH = 34;
      const int btnTop = contentBottom - 54;
      for (int i = 0; i < kActionCount; i++) {
        const Rect buttonRect{pad + i * (btnW + 18), btnTop, btnW, btnH};
        if (TouchHitTest::pointInRect(touchEvent.x, touchEvent.y, buttonRect)) {
          actionIndex = i;
          applySelectedAction();
          return;
        }
      }
      applySelectedAction();
      return;
    }
    if (!buttonHintTap && !showingBack && !cards.empty() && TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(cards.size()));
      requestUpdate();
      return;
    }
    if (!buttonHintTap && !showingBack && !cards.empty() && TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(cards.size()));
      requestUpdate();
      return;
    }
    if (!buttonHintTap && showingBack && TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      actionIndex = ButtonNavigator::nextIndex(actionIndex, kActionCount);
      requestUpdate();
      return;
    }
    if (!buttonHintTap && showingBack && TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      actionIndex = ButtonNavigator::previousIndex(actionIndex, kActionCount);
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (showingBack) {
      showingBack = false;
      requestUpdate();
      return;
    }
    finish();
    return;
  }

  const auto& cards = STUDY_REVIEW_QUEUE.getCards(StudyQueueKind::Again);
  if (cards.empty()) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) && sessionComplete) {
      openRecommendedNextStep();
    }
    return;
  }

  if (!showingBack) {
    buttonNavigator.onNextRelease([this, &cards] {
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(cards.size()));
      requestUpdate();
    });

    buttonNavigator.onPreviousRelease([this, &cards] {
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(cards.size()));
      requestUpdate();
    });

    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      showingBack = true;
      actionIndex = 0;
      requestUpdate();
    }
    return;
  }

  buttonNavigator.onNextRelease([this] {
    actionIndex = ButtonNavigator::nextIndex(actionIndex, kActionCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    actionIndex = ButtonNavigator::previousIndex(actionIndex, kActionCount);
    requestUpdate();
  });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    applySelectedAction();
  }
}

void StudyRecoveryActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int pad = metrics.contentSidePadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - 8;

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_STUDY_RECOVERY));

  const auto& cards = STUDY_REVIEW_QUEUE.getCards(StudyQueueKind::Again);
  if (cards.empty()) {
    const int cardH = sessionComplete ? 156 : 132;
    const int cardY = (pageHeight - cardH) / 2 - 18;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 18,
                              sessionComplete ? tr(STR_STUDY_RECOVERY_COMPLETE) : tr(STR_STUDY_NO_RECOVERY_CARDS), true,
                              EpdFontFamily::BOLD);
    renderer.drawLine(pad + 18, cardY + 48, pageWidth - pad - 18, cardY + 48, true);
    if (sessionComplete) {
      char done[64];
      snprintf(done, sizeof(done), tr(STR_STUDY_CLEARED_RECOVERY_FORMAT), completedCount);
      renderer.drawCenteredText(SMALL_FONT_ID, cardY + 66, done);
      renderer.drawCenteredText(UI_10_FONT_ID, cardY + 94, I18n::getInstance().get(nextStepLabelId()), true,
                                EpdFontFamily::BOLD);
      renderer.drawCenteredText(SMALL_FONT_ID, cardY + 122, I18n::getInstance().get(nextStepHintId()));
    } else {
      renderer.drawCenteredText(SMALL_FONT_ID, cardY + 66, tr(STR_STUDY_WRONG_CARDS_APPEAR_HERE));
      renderer.drawCenteredText(SMALL_FONT_ID, cardY + 94, tr(STR_STUDY_MISS_CARD_HINT));
    }
  } else {
    const auto& card = cards[std::clamp(selectedIndex, 0, static_cast<int>(cards.size()) - 1)];
    char meta[64];
    snprintf(meta, sizeof(meta), tr(STR_STUDY_INDEX_COUNT_FORMAT), selectedIndex + 1, static_cast<int>(cards.size()),
             card.count);
    renderer.drawText(SMALL_FONT_ID, pad, contentTop, meta);
    const std::string deckLabel = renderer.truncatedText(SMALL_FONT_ID, card.deckName.c_str(), pageWidth / 2);
    renderer.drawText(SMALL_FONT_ID, pageWidth - pad - renderer.getTextWidth(SMALL_FONT_ID, deckLabel.c_str()),
                      contentTop, deckLabel.c_str());

    const int cardY = contentTop + 24;
    const int actionArea = showingBack ? 72 : 38;
    const int cardH = contentBottom - cardY - actionArea;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawText(SMALL_FONT_ID, pad + 14, cardY + 14,
                      showingBack ? tr(STR_STUDY_CARD_BACK) : tr(STR_STUDY_CARD_FRONT));

    const std::string text = showingBack ? card.back : card.front;
    const auto lines =
        renderer.wrappedText(UI_12_FONT_ID, text.c_str(), pageWidth - pad * 2 - 32, 4, EpdFontFamily::BOLD);
    const int firstLineY = cardY + 58;
    for (size_t i = 0; i < lines.size(); i++) {
      renderer.drawCenteredText(UI_12_FONT_ID, firstLineY + static_cast<int>(i) * 30, lines[i].c_str(), true,
                                EpdFontFamily::BOLD);
    }

    if (showingBack) {
      const int btnW = (pageWidth - pad * 2 - 18) / 2;
      const int btnH = 34;
      const int btnTop = contentBottom - 54;
      for (int i = 0; i < kActionCount; i++) {
        const int x = pad + i * (btnW + 18);
        drawActionButton(renderer, x, btnTop, btnW, btnH, I18n::getInstance().get(kActionIds[i]), i == actionIndex);
      }
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, contentBottom - 30, tr(STR_STUDY_CONFIRM_TO_REVEAL));
    }
  }

  const auto labels = mappedInput.mapLabels(
      tr(STR_BACK), sessionComplete ? tr(STR_OPEN) : (showingBack ? tr(STR_STUDY_APPLY) : tr(STR_STUDY_FLIP)),
      tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
