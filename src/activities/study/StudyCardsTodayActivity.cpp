#include "StudyCardsTodayActivity.h"

#include <I18n.h>

#include <algorithm>

#include "LearningReportActivity.h"
#include "SavedCardsActivity.h"
#include "StudyDeckStore.h"
#include "StudyLaterActivity.h"
#include "StudyRecoveryActivity.h"
#include "StudyReviewQueueStore.h"
#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kActionCount = 4;
constexpr StrId kActionIds[kActionCount] = {StrId::STR_STUDY_ACTION_KNOW, StrId::STR_STUDY_ACTION_AGAIN,
                                            StrId::STR_STUDY_ACTION_LATER, StrId::STR_STUDY_ACTION_SAVE};

void drawProgressBar(const GfxRenderer& renderer, int x, int y, int width, int height, int percent) {
  const int r = height / 2;
  renderer.drawRoundedRect(x, y, width, height, 1, r, true);
  const int fillWidth = (width - 2) * std::clamp(percent, 0, 100) / 100;
  if (fillWidth > 2) {
    renderer.fillRoundedRect(x + 1, y + 1, fillWidth, height - 2, r, Color::Black);
  }
}

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

std::string truncatedLine(const GfxRenderer& renderer, int fontId, const std::string& text, int maxWidth,
                          EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  return renderer.truncatedText(fontId, text.c_str(), maxWidth, style);
}
}  // namespace

void StudyCardsTodayActivity::startSession() {
  inSession = true;
  showingBack = false;
  sessionComplete = false;
  cardIndex = 0;
  actionIndex = 0;
  reviewedInSession = 0;
  sessionAgainCount = 0;
  sessionLaterCount = 0;
  savedCount = 0;
}

StudyCardsTodayActivity::NextStep StudyCardsTodayActivity::recommendedNextStep() const {
  if (STUDY_REVIEW_QUEUE.getAgainCount() > 0) {
    return NextStep::Recovery;
  }
  if (STUDY_REVIEW_QUEUE.getLaterCount() > 0) {
    return NextStep::Later;
  }
  if (STUDY_REVIEW_QUEUE.getSavedCount() > 0) {
    return NextStep::Saved;
  }
  return NextStep::Report;
}

StrId StudyCardsTodayActivity::nextStepLabelId() const {
  switch (recommendedNextStep()) {
    case NextStep::Recovery:
      return StrId::STR_STUDY_NEXT_RECOVERY;
    case NextStep::Later:
      return StrId::STR_STUDY_NEXT_LATER_CARDS;
    case NextStep::Saved:
      return StrId::STR_STUDY_NEXT_SAVED_CARDS;
    case NextStep::Report:
    default:
      return StrId::STR_STUDY_NEXT_LEARNING_REPORT;
  }
}

StrId StudyCardsTodayActivity::nextStepHintId() const {
  switch (recommendedNextStep()) {
    case NextStep::Recovery:
      return StrId::STR_STUDY_HINT_FIX_MISSED_FIRST;
    case NextStep::Later:
      return StrId::STR_STUDY_HINT_CLEAR_POSTPONED_NEXT;
    case NextStep::Saved:
      return StrId::STR_STUDY_HINT_REVIEW_SAVED_CARDS;
    case NextStep::Report:
    default:
      return StrId::STR_STUDY_HINT_CHECK_MASTERY;
  }
}

void StudyCardsTodayActivity::openRecommendedNextStep() {
  switch (recommendedNextStep()) {
    case NextStep::Recovery:
      activityManager.replaceActivity(std::make_unique<StudyRecoveryActivity>(renderer, mappedInput));
      break;
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

void StudyCardsTodayActivity::advanceCard(const bool recordResult, const bool correct, const bool saved) {
  const auto& cards = STUDY_DECKS.getCards();
  if (!cards.empty()) {
    const StudyCard& card = cards[std::clamp(cardIndex, 0, static_cast<int>(cards.size()) - 1)];
    if (recordResult && !correct) {
      STUDY_REVIEW_QUEUE.recordAgain(card);
      sessionAgainCount++;
    } else if (!recordResult && !saved) {
      STUDY_REVIEW_QUEUE.recordLater(card);
      sessionLaterCount++;
    } else if (saved) {
      STUDY_REVIEW_QUEUE.recordSaved(card);
    }
  }

  if (recordResult) {
    STUDY_STATE.recordReviewResult(correct);
  }
  if (saved) {
    savedCount++;
  }

  reviewedInSession++;
  if (cards.empty() || reviewedInSession >= static_cast<int>(cards.size())) {
    inSession = false;
    showingBack = false;
    sessionComplete = true;
    requestUpdate();
    return;
  }

  cardIndex = (cardIndex + 1) % static_cast<int>(cards.size());
  showingBack = false;
  actionIndex = 0;
  requestUpdate();
}

void StudyCardsTodayActivity::applySelectedAction() {
  switch (actionIndex) {
    case 0:
      advanceCard(true, true, false);
      break;
    case 1:
      advanceCard(true, false, false);
      break;
    case 2:
      advanceCard(false, false, false);
      break;
    case 3:
    default:
      advanceCard(false, false, true);
      break;
  }
}

void StudyCardsTodayActivity::onEnter() {
  Activity::onEnter();
  STUDY_STATE.loadFromFile();
  STUDY_DECKS.refresh();
  STUDY_REVIEW_QUEUE.loadFromFile();
  inSession = false;
  showingBack = false;
  sessionComplete = false;
  cardIndex = 0;
  actionIndex = 0;
  reviewedInSession = 0;
  sessionAgainCount = 0;
  sessionLaterCount = 0;
  savedCount = 0;
  requestUpdate();
}

void StudyCardsTodayActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      mappedInput.suppressTouchButtonFallback();
      if (!inSession) {
        if (sessionComplete) {
          openRecommendedNextStep();
        } else if (STUDY_DECKS.hasCards()) {
          startSession();
          requestUpdate();
        } else {
          STUDY_DECKS.refresh();
          requestUpdate();
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
      const int btnTop = contentBottom - 74;
      for (int i = 0; i < kActionCount; i++) {
        const Rect buttonRect{pad + (i % 2) * (btnW + 18), btnTop + (i / 2) * (btnH + 8), btnW, btnH};
        if (TouchHitTest::pointInRect(touchEvent.x, touchEvent.y, buttonRect)) {
          actionIndex = i;
          applySelectedAction();
          return;
        }
      }
      applySelectedAction();
      return;
    }
    if (!buttonHintTap && inSession && showingBack && TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      actionIndex = ButtonNavigator::nextIndex(actionIndex, kActionCount);
      requestUpdate();
      return;
    }
    if (!buttonHintTap && inSession && showingBack && TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      actionIndex = ButtonNavigator::previousIndex(actionIndex, kActionCount);
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (inSession && showingBack) {
      showingBack = false;
      requestUpdate();
      return;
    }
    finish();
    return;
  }

  if (!inSession) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) && sessionComplete) {
      openRecommendedNextStep();
    } else if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) && STUDY_DECKS.hasCards()) {
      startSession();
      requestUpdate();
    } else if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      STUDY_DECKS.refresh();
      requestUpdate();
    }
    return;
  }

  if (!showingBack) {
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

void StudyCardsTodayActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const auto& state = STUDY_STATE.getState();
  const auto& cards = STUDY_DECKS.getCards();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_STUDY_CARDS));

  const int pad = metrics.contentSidePadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - 8;
  const int totalReviewed = static_cast<int>(state.correctToday) + static_cast<int>(state.wrongToday);
  const int accuracy = totalReviewed > 0 ? static_cast<int>(state.correctToday) * 100 / totalReviewed : 0;

  if (cards.empty()) {
    const int cardH = 144;
    const int cardY = (pageHeight - cardH) / 2 - 20;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 16, tr(STR_STUDY_NO_STUDY_CARDS_FOUND), true, EpdFontFamily::BOLD);
    renderer.drawLine(pad + 16, cardY + 48, pageWidth - pad - 16, cardY + 48, true);
    renderer.drawText(SMALL_FONT_ID, pad + 18, cardY + 62, tr(STR_STUDY_COPY_DECK_JSON_TO));
    renderer.drawText(UI_10_FONT_ID, pad + 18, cardY + 84, "/.mofei/study", true, EpdFontFamily::BOLD);
    renderer.drawText(SMALL_FONT_ID, pad + 18, cardY + 112, tr(STR_STUDY_ACCEPTED_DECK_SCHEMA));

    char status[64];
    snprintf(status, sizeof(status), tr(STR_STUDY_DECKS_ERRORS_COMPACT_FORMAT), STUDY_DECKS.getDeckCount(),
             STUDY_DECKS.getErrorCount());
    renderer.drawCenteredText(SMALL_FONT_ID, contentBottom - 16, status);
  } else if (!inSession && sessionComplete) {
    const int cardH = 178;
    const int cardY = contentTop + (contentBottom - contentTop - cardH) / 2 - 22;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 16, tr(STR_STUDY_COMPLETE), true, EpdFontFamily::BOLD);
    renderer.drawLine(pad + 24, cardY + 46, pageWidth - pad - 24, cardY + 46, true);

    char reviewed[64];
    snprintf(reviewed, sizeof(reviewed), tr(STR_STUDY_REVIEWED_SAVED_FORMAT), reviewedInSession, savedCount);
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 62, reviewed);

    char queues[64];
    snprintf(queues, sizeof(queues), tr(STR_STUDY_AGAIN_LATER_FORMAT), sessionAgainCount, sessionLaterCount);
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 86, queues);

    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 116, I18n::getInstance().get(nextStepLabelId()), true,
                              EpdFontFamily::BOLD);
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 144, I18n::getInstance().get(nextStepHintId()));
  } else if (!inSession) {
    const int due = std::max(0, static_cast<int>(state.dueToday) - static_cast<int>(state.completedToday));
    const int cardH = 178;
    const int cardY = contentTop + (contentBottom - contentTop - cardH) / 2 - 22;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);

    char countStr[32];
    snprintf(countStr, sizeof(countStr), "%d", static_cast<int>(cards.size()));
    renderer.drawCenteredText(UI_12_FONT_ID, cardY + 18, countStr, true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 66, tr(STR_STUDY_LOADED_CARDS));
    renderer.drawLine(pad + 24, cardY + 94, pageWidth - pad - 24, cardY + 94, true);

    char deckStr[64];
    snprintf(deckStr, sizeof(deckStr), tr(STR_STUDY_DECKS_IMPORTED_FORMAT), STUDY_DECKS.getDeckCount());
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 108, deckStr);

    char dueStr[80];
    snprintf(dueStr, sizeof(dueStr), tr(STR_STUDY_DUE_ACCURACY_FORMAT), due, accuracy);
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 132, dueStr);
    drawProgressBar(renderer, pad + 22, cardY + 154, pageWidth - (pad + 22) * 2, 8,
                    state.dueToday > 0 ? static_cast<int>(state.completedToday) * 100 / state.dueToday : 0);
  } else {
    const StudyCard& card = cards[std::clamp(cardIndex, 0, static_cast<int>(cards.size()) - 1)];
    const int total = static_cast<int>(cards.size());
    const int pct = total > 0 ? std::clamp(reviewedInSession * 100 / total, 0, 100) : 0;

    char meta[80];
    snprintf(meta, sizeof(meta), tr(STR_STUDY_CARD_SAVED_FORMAT), reviewedInSession + 1, total, savedCount);
    renderer.drawText(SMALL_FONT_ID, pad, contentTop, meta);
    const std::string deckName = truncatedLine(renderer, SMALL_FONT_ID, card.deckName, pageWidth / 2);
    renderer.drawText(SMALL_FONT_ID, pageWidth - pad - renderer.getTextWidth(SMALL_FONT_ID, deckName.c_str()),
                      contentTop, deckName.c_str());

    const int cardY = contentTop + 24;
    const int actionArea = showingBack ? 92 : 38;
    const int cardH = contentBottom - cardY - actionArea;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawText(SMALL_FONT_ID, pad + 14, cardY + 14,
                      showingBack ? tr(STR_STUDY_CARD_BACK) : tr(STR_STUDY_CARD_FRONT));

    const std::string primary = showingBack ? card.back : card.front;
    const std::string secondary = showingBack ? card.example : "";
    const int textMaxWidth = pageWidth - pad * 2 - 32;
    const auto primaryLines =
        renderer.wrappedText(UI_12_FONT_ID, primary.c_str(), textMaxWidth, 3, EpdFontFamily::BOLD);
    const int firstLineY = cardY + 56;
    for (size_t i = 0; i < primaryLines.size(); i++) {
      renderer.drawCenteredText(UI_12_FONT_ID, firstLineY + static_cast<int>(i) * 28, primaryLines[i].c_str(), true,
                                EpdFontFamily::BOLD);
    }

    if (!secondary.empty()) {
      const auto secondaryLines = renderer.wrappedText(SMALL_FONT_ID, secondary.c_str(), textMaxWidth, 2);
      const int secondaryY = cardY + cardH - 54;
      for (size_t i = 0; i < secondaryLines.size(); i++) {
        renderer.drawCenteredText(SMALL_FONT_ID, secondaryY + static_cast<int>(i) * 18, secondaryLines[i].c_str());
      }
    }

    drawProgressBar(renderer, pad + 14, cardY + cardH - 18, pageWidth - (pad + 14) * 2, 8, pct);

    if (showingBack) {
      const int btnW = (pageWidth - pad * 2 - 18) / 2;
      const int btnH = 34;
      const int btnTop = contentBottom - 74;
      for (int i = 0; i < kActionCount; i++) {
        const int x = pad + (i % 2) * (btnW + 18);
        const int y = btnTop + (i / 2) * (btnH + 8);
        drawActionButton(renderer, x, y, btnW, btnH, I18n::getInstance().get(kActionIds[i]), i == actionIndex);
      }
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, contentBottom - 32, tr(STR_STUDY_PRESS_CONFIRM_REVEAL_ANSWER));
    }
  }

  const char* confirmLabel = inSession ? (showingBack ? tr(STR_STUDY_APPLY) : tr(STR_STUDY_FLIP))
                                       : (sessionComplete ? tr(STR_OPEN) : tr(STR_START));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
