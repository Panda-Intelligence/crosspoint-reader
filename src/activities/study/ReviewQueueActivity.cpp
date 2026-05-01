#include "ReviewQueueActivity.h"

#include <I18n.h>

#include <algorithm>

#include "StudyReviewQueueStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kQueueCount = 3;

StudyQueueKind queueKind(const int index) {
  switch (index) {
    case 1:
      return StudyQueueKind::Later;
    case 2:
      return StudyQueueKind::Saved;
    case 0:
    default:
      return StudyQueueKind::Again;
  }
}

StrId queueTitleId(const int index) {
  switch (index) {
    case 1:
      return StrId::STR_STUDY_QUEUE_LATER;
    case 2:
      return StrId::STR_STUDY_QUEUE_SAVED;
    case 0:
    default:
      return StrId::STR_STUDY_QUEUE_AGAIN;
  }
}

StrId queueHintId(const int index) {
  switch (index) {
    case 1:
      return StrId::STR_STUDY_CARDS_PARKED_FOR_LATER;
    case 2:
      return StrId::STR_STUDY_SAVED_CARDS_READ_ONLY;
    case 0:
    default:
      return StrId::STR_STUDY_CARDS_MARKED_WRONG;
  }
}
}  // namespace

int ReviewQueueActivity::currentQueueSize() const {
  return static_cast<int>(STUDY_REVIEW_QUEUE.getCards(queueKind(queueIndex)).size());
}

void ReviewQueueActivity::switchQueue(const int delta) {
  queueIndex = (queueIndex + delta + kQueueCount) % kQueueCount;
  cardIndex = 0;
  showingBack = false;
  confirmingClear = false;
  requestUpdate();
}

void ReviewQueueActivity::moveCard(const int delta) {
  const int size = currentQueueSize();
  if (size <= 0) {
    return;
  }
  cardIndex = (cardIndex + delta + size) % size;
  showingBack = false;
  confirmingClear = false;
  requestUpdate();
}

void ReviewQueueActivity::onEnter() {
  Activity::onEnter();
  STUDY_REVIEW_QUEUE.loadFromFile();
  queueIndex = 0;
  cardIndex = 0;
  showingBack = false;
  confirmingClear = false;
  requestUpdate();
}

void ReviewQueueActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (confirmingClear) {
      if (!buttonHintTap && touchEvent.isTap()) {
        mappedInput.suppressTouchButtonFallback();
        if (touchEvent.x >= renderer.getScreenWidth() / 2) {
          STUDY_REVIEW_QUEUE.clearQueue(queueKind(queueIndex));
          cardIndex = 0;
        }
        confirmingClear = false;
        requestUpdate();
        return;
      }
      if (!buttonHintTap && touchEvent.type == InputTouchEvent::Type::SwipeLeft) {
        mappedInput.suppressTouchButtonFallback();
        STUDY_REVIEW_QUEUE.clearQueue(queueKind(queueIndex));
        cardIndex = 0;
        confirmingClear = false;
        requestUpdate();
        return;
      }
      if (!buttonHintTap && touchEvent.type == InputTouchEvent::Type::SwipeRight) {
        mappedInput.suppressTouchButtonFallback();
        confirmingClear = false;
        requestUpdate();
        return;
      }
    } else if (!buttonHintTap && touchEvent.isTap()) {
      mappedInput.suppressTouchButtonFallback();
      if (!showingBack && currentQueueSize() > 0) {
        const auto& metrics = UITheme::getInstance().getMetrics();
        const int contentBottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - 8;
        if (touchEvent.y >= contentBottom - 56) {
          confirmingClear = true;
        } else {
          showingBack = true;
        }
        requestUpdate();
        return;
      }
      if (showingBack) {
        const StudyQueueKind kind = queueKind(queueIndex);
        if (kind == StudyQueueKind::Saved) {
          showingBack = false;
          requestUpdate();
          return;
        }
        if (STUDY_REVIEW_QUEUE.removeAt(kind, cardIndex)) {
          const int size = currentQueueSize();
          cardIndex = size <= 0 ? 0 : std::min(cardIndex, size - 1);
          showingBack = false;
          requestUpdate();
          return;
        }
      }
    } else if (!buttonHintTap && (touchEvent.type == InputTouchEvent::Type::SwipeLeft ||
                                  touchEvent.type == InputTouchEvent::Type::SwipeRight)) {
      mappedInput.suppressTouchButtonFallback();
      switchQueue(touchEvent.type == InputTouchEvent::Type::SwipeLeft ? 1 : -1);
      return;
    } else if (!buttonHintTap && TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      moveCard(1);
      return;
    } else if (!buttonHintTap && TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      moveCard(-1);
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (confirmingClear) {
      confirmingClear = false;
      requestUpdate();
      return;
    }
    if (showingBack) {
      showingBack = false;
      requestUpdate();
      return;
    }
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    if (confirmingClear) {
      confirmingClear = false;
      requestUpdate();
      return;
    }
    switchQueue(-1);
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    if (confirmingClear) {
      STUDY_REVIEW_QUEUE.clearQueue(queueKind(queueIndex));
      cardIndex = 0;
      confirmingClear = false;
      requestUpdate();
      return;
    }
    switchQueue(1);
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Down) && !showingBack && currentQueueSize() > 0) {
    confirmingClear = true;
    requestUpdate();
    return;
  }

  buttonNavigator.onNextRelease([this] { moveCard(1); });
  buttonNavigator.onPreviousRelease([this] { moveCard(-1); });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (confirmingClear) {
      confirmingClear = false;
      requestUpdate();
      return;
    }
    const StudyQueueKind kind = queueKind(queueIndex);
    if (!showingBack) {
      if (currentQueueSize() > 0) {
        showingBack = true;
        requestUpdate();
      }
      return;
    }

    if (kind == StudyQueueKind::Saved) {
      showingBack = false;
      requestUpdate();
      return;
    }

    if (STUDY_REVIEW_QUEUE.removeAt(kind, cardIndex)) {
      const int size = currentQueueSize();
      if (size <= 0) {
        cardIndex = 0;
      } else {
        cardIndex = std::min(cardIndex, size - 1);
      }
      showingBack = false;
      requestUpdate();
    }
  }
}

void ReviewQueueActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int pad = metrics.contentSidePadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - 8;

  char header[64];
  snprintf(header, sizeof(header), tr(STR_STUDY_REVIEW_QUEUE_HEADER_FORMAT),
           I18n::getInstance().get(queueTitleId(queueIndex)));
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header);

  const auto& cards = STUDY_REVIEW_QUEUE.getCards(queueKind(queueIndex));
  if (cards.empty()) {
    const int cardH = 132;
    const int cardY = (pageHeight - cardH) / 2 - 18;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 18, tr(STR_STUDY_QUEUE_EMPTY), true, EpdFontFamily::BOLD);
    renderer.drawLine(pad + 18, cardY + 48, pageWidth - pad - 18, cardY + 48, true);
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 66, I18n::getInstance().get(queueHintId(queueIndex)));
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 94, tr(STR_STUDY_USE_LEFT_RIGHT_SWITCH_QUEUES));
  } else {
    if (confirmingClear) {
      const int cardH = 132;
      const int cardY = (pageHeight - cardH) / 2 - 18;
      renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
      renderer.drawCenteredText(UI_10_FONT_ID, cardY + 18, tr(STR_STUDY_CLEAR_CURRENT_QUEUE), true,
                                EpdFontFamily::BOLD);
      renderer.drawLine(pad + 18, cardY + 48, pageWidth - pad - 18, cardY + 48, true);
      renderer.drawCenteredText(SMALL_FONT_ID, cardY + 66, I18n::getInstance().get(queueTitleId(queueIndex)));
      renderer.drawCenteredText(SMALL_FONT_ID, cardY + 94, tr(STR_STUDY_RIGHT_CONFIRM_LEFT_CANCEL));
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_CANCEL), tr(STR_CLEAR_BUTTON));
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      renderer.displayBuffer();
      return;
    }

    const StudyQueuedCard& card = cards[std::clamp(cardIndex, 0, static_cast<int>(cards.size()) - 1)];
    char meta[64];
    snprintf(meta, sizeof(meta), tr(STR_STUDY_INDEX_COUNT_FORMAT), cardIndex + 1, static_cast<int>(cards.size()),
             card.count);
    renderer.drawText(SMALL_FONT_ID, pad, contentTop, meta);
    const std::string deckLabel = renderer.truncatedText(SMALL_FONT_ID, card.deckName.c_str(), pageWidth / 2);
    const int deckW = renderer.getTextWidth(SMALL_FONT_ID, deckLabel.c_str());
    renderer.drawText(SMALL_FONT_ID, pageWidth - pad - deckW, contentTop, deckLabel.c_str());

    const int cardY = contentTop + 24;
    const int cardH = contentBottom - cardY - 38;
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
      const char* action = queueKind(queueIndex) == StudyQueueKind::Saved ? tr(STR_STUDY_SAVED_CONFIRM_CLOSES)
                                                                          : tr(STR_STUDY_CONFIRM_MARKS_DONE);
      renderer.drawCenteredText(UI_10_FONT_ID, contentBottom - 30, action);
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, contentBottom - 30, tr(STR_STUDY_CONFIRM_REVEAL_DOWN_CLEAR));
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), showingBack ? tr(STR_DONE) : tr(STR_STUDY_FLIP),
                                            tr(STR_STUDY_QUEUE_PREV), tr(STR_STUDY_QUEUE_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
