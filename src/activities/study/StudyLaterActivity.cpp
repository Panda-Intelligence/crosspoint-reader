#include "StudyLaterActivity.h"

#include <I18n.h>

#include <algorithm>
#include <vector>

#include "StudyReviewQueueStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

int StudyLaterActivity::itemCount() const {
  const auto& cards = STUDY_REVIEW_QUEUE.getCards(StudyQueueKind::Later);
  if (cards.empty()) {
    return 0;
  }

  std::vector<std::string> decks;
  for (const auto& card : cards) {
    if (std::find(decks.begin(), decks.end(), card.deckName) == decks.end()) {
      decks.push_back(card.deckName);
    }
  }
  if (decks.empty()) {
    return 0;
  }

  const std::string activeDeck = decks[std::clamp(deckIndex, 0, static_cast<int>(decks.size()) - 1)];
  int count = 0;
  for (const auto& card : cards) {
    if (card.deckName == activeDeck) {
      count++;
    }
  }
  return count;
}

int StudyLaterActivity::deckCount() const {
  const auto& cards = STUDY_REVIEW_QUEUE.getCards(StudyQueueKind::Later);
  std::vector<std::string> decks;
  for (const auto& card : cards) {
    if (std::find(decks.begin(), decks.end(), card.deckName) == decks.end()) {
      decks.push_back(card.deckName);
    }
  }
  return static_cast<int>(decks.size());
}

int StudyLaterActivity::selectedQueueIndex() const {
  const auto& cards = STUDY_REVIEW_QUEUE.getCards(StudyQueueKind::Later);
  std::vector<std::string> decks;
  for (const auto& card : cards) {
    if (std::find(decks.begin(), decks.end(), card.deckName) == decks.end()) {
      decks.push_back(card.deckName);
    }
  }
  if (decks.empty()) {
    return -1;
  }

  const std::string activeDeck = decks[std::clamp(deckIndex, 0, static_cast<int>(decks.size()) - 1)];
  const int targetIndex = std::clamp(selectedIndex, 0, std::max(0, itemCount() - 1));
  int filteredIndex = 0;
  for (int i = 0; i < static_cast<int>(cards.size()); i++) {
    if (cards[i].deckName != activeDeck) {
      continue;
    }
    if (filteredIndex == targetIndex) {
      return i;
    }
    filteredIndex++;
  }
  return -1;
}

void StudyLaterActivity::onEnter() {
  Activity::onEnter();
  STUDY_REVIEW_QUEUE.loadFromFile();
  deckIndex = 0;
  selectedIndex = 0;
  showingBack = false;
  requestUpdate();
}

void StudyLaterActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (showingBack) {
      showingBack = false;
      requestUpdate();
      return;
    }
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left) && !showingBack && deckCount() > 0) {
    deckIndex = (deckIndex - 1 + deckCount()) % deckCount();
    selectedIndex = 0;
    requestUpdate();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right) && !showingBack && deckCount() > 0) {
    deckIndex = (deckIndex + 1) % deckCount();
    selectedIndex = 0;
    requestUpdate();
    return;
  }

  if (itemCount() > 0 && !showingBack) {
    buttonNavigator.onNextRelease([this] {
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount());
      requestUpdate();
    });

    buttonNavigator.onPreviousRelease([this] {
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount());
      requestUpdate();
    });
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (!showingBack) {
      if (itemCount() > 0) {
        showingBack = true;
        requestUpdate();
      }
      return;
    }

    if (STUDY_REVIEW_QUEUE.removeAt(StudyQueueKind::Later, selectedQueueIndex())) {
      const int count = itemCount();
      if (count <= 0) {
        selectedIndex = 0;
      } else {
        selectedIndex = std::min(selectedIndex, count - 1);
      }
      showingBack = false;
      requestUpdate();
    }
  }
}

void StudyLaterActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int pad = metrics.contentSidePadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - 8;

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Later Cards");

  const auto& cards = STUDY_REVIEW_QUEUE.getCards(StudyQueueKind::Later);
  if (cards.empty()) {
    const int cardH = 132;
    const int cardY = (pageHeight - cardH) / 2 - 18;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 18, "No later cards", true, EpdFontFamily::BOLD);
    renderer.drawLine(pad + 18, cardY + 48, pageWidth - pad - 18, cardY + 48, true);
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 66, "Use Later during Study Cards");
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 94, "Postponed cards stay here");
  } else {
    std::vector<std::string> decks;
    for (const auto& item : cards) {
      if (std::find(decks.begin(), decks.end(), item.deckName) == decks.end()) {
        decks.push_back(item.deckName);
      }
    }
    const std::string activeDeck = decks[std::clamp(deckIndex, 0, static_cast<int>(decks.size()) - 1)];

    std::vector<const StudyQueuedCard*> filteredCards;
    for (const auto& item : cards) {
      if (item.deckName == activeDeck) {
        filteredCards.push_back(&item);
      }
    }

    const auto& card = *filteredCards[std::clamp(selectedIndex, 0, static_cast<int>(filteredCards.size()) - 1)];
    char meta[56];
    snprintf(meta, sizeof(meta), "Deck %d/%d  Card %d/%d",
             std::clamp(deckIndex, 0, static_cast<int>(decks.size()) - 1) + 1, static_cast<int>(decks.size()),
             selectedIndex + 1, static_cast<int>(filteredCards.size()));
    renderer.drawText(SMALL_FONT_ID, pad, contentTop, meta);
    const std::string deckLabel = renderer.truncatedText(SMALL_FONT_ID, activeDeck.c_str(), pageWidth / 2);
    renderer.drawText(SMALL_FONT_ID, pageWidth - pad - renderer.getTextWidth(SMALL_FONT_ID, deckLabel.c_str()),
                      contentTop, deckLabel.c_str());

    const int cardY = contentTop + 24;
    const int cardH = contentBottom - cardY - 38;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawText(SMALL_FONT_ID, pad + 14, cardY + 14, showingBack ? "Back" : "Front");

    const std::string text = showingBack ? card.back : card.front;
    const auto lines =
        renderer.wrappedText(UI_12_FONT_ID, text.c_str(), pageWidth - pad * 2 - 32, 4, EpdFontFamily::BOLD);
    const int firstLineY = cardY + 58;
    for (size_t i = 0; i < lines.size(); i++) {
      renderer.drawCenteredText(UI_12_FONT_ID, firstLineY + static_cast<int>(i) * 30, lines[i].c_str(), true,
                                EpdFontFamily::BOLD);
    }

    if (showingBack) {
      renderer.drawCenteredText(UI_10_FONT_ID, contentBottom - 30, "Confirm marks as handled");
    } else {
      char progress[56];
      snprintf(progress, sizeof(progress), "%d in deck, %d later total", static_cast<int>(filteredCards.size()),
               static_cast<int>(cards.size()));
      renderer.drawCenteredText(UI_10_FONT_ID, contentBottom - 30, progress);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), showingBack ? "Done" : "Flip", "Deck -", "Deck +");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
