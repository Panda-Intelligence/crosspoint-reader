#include "ReviewQueueActivity.h"

#include <I18n.h>

#include <algorithm>

#include "StudyReviewQueueStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

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

const char* queueTitle(const int index) {
  switch (index) {
    case 1:
      return "Later";
    case 2:
      return "Saved";
    case 0:
    default:
      return "Again";
  }
}

const char* queueHint(const int index) {
  switch (index) {
    case 1:
      return "Cards parked for later review";
    case 2:
      return "Saved cards are read-only";
    case 0:
    default:
      return "Cards marked wrong";
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
  requestUpdate();
}

void ReviewQueueActivity::moveCard(const int delta) {
  const int size = currentQueueSize();
  if (size <= 0) {
    return;
  }
  cardIndex = (cardIndex + delta + size) % size;
  showingBack = false;
  requestUpdate();
}

void ReviewQueueActivity::onEnter() {
  Activity::onEnter();
  STUDY_REVIEW_QUEUE.loadFromFile();
  queueIndex = 0;
  cardIndex = 0;
  showingBack = false;
  requestUpdate();
}

void ReviewQueueActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (showingBack) {
      showingBack = false;
      requestUpdate();
      return;
    }
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    switchQueue(-1);
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    switchQueue(1);
    return;
  }

  buttonNavigator.onNextRelease([this] { moveCard(1); });
  buttonNavigator.onPreviousRelease([this] { moveCard(-1); });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
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

  char header[32];
  snprintf(header, sizeof(header), "Review Queue: %s", queueTitle(queueIndex));
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header);

  const auto& cards = STUDY_REVIEW_QUEUE.getCards(queueKind(queueIndex));
  if (cards.empty()) {
    const int cardH = 132;
    const int cardY = (pageHeight - cardH) / 2 - 18;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 18, "Queue is empty", true, EpdFontFamily::BOLD);
    renderer.drawLine(pad + 18, cardY + 48, pageWidth - pad - 18, cardY + 48, true);
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 66, queueHint(queueIndex));
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 94, "Use Left/Right to switch queues");
  } else {
    const StudyQueuedCard& card = cards[std::clamp(cardIndex, 0, static_cast<int>(cards.size()) - 1)];
    char meta[48];
    snprintf(meta, sizeof(meta), "%d/%d  Count %u", cardIndex + 1, static_cast<int>(cards.size()), card.count);
    renderer.drawText(SMALL_FONT_ID, pad, contentTop, meta);
    const std::string deckLabel = renderer.truncatedText(SMALL_FONT_ID, card.deckName.c_str(), pageWidth / 2);
    const int deckW = renderer.getTextWidth(SMALL_FONT_ID, deckLabel.c_str());
    renderer.drawText(SMALL_FONT_ID, pageWidth - pad - deckW, contentTop, deckLabel.c_str());

    const int cardY = contentTop + 24;
    const int cardH = contentBottom - cardY - 38;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawText(SMALL_FONT_ID, pad + 14, cardY + 14, showingBack ? "Back" : "Front");

    const std::string text = showingBack ? card.back : card.front;
    const auto lines = renderer.wrappedText(UI_12_FONT_ID, text.c_str(), pageWidth - pad * 2 - 32, 4,
                                            EpdFontFamily::BOLD);
    const int firstLineY = cardY + 58;
    for (size_t i = 0; i < lines.size(); i++) {
      renderer.drawCenteredText(UI_12_FONT_ID, firstLineY + static_cast<int>(i) * 30, lines[i].c_str(), true,
                                EpdFontFamily::BOLD);
    }

    if (showingBack) {
      const char* action = queueKind(queueIndex) == StudyQueueKind::Saved ? "Saved card - Confirm closes"
                                                                          : "Confirm marks Done";
      renderer.drawCenteredText(UI_10_FONT_ID, contentBottom - 30, action);
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, contentBottom - 30, "Confirm to reveal");
    }
  }

  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), showingBack ? "Done" : "Flip", "Queue -", "Queue +");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
