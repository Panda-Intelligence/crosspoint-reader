#include "StudyRecoveryActivity.h"

#include <I18n.h>

#include <algorithm>

#include "StudyReviewQueueStore.h"
#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kActionCount = 2;
constexpr const char* kActions[kActionCount] = {"Know", "Again"};

void drawActionButton(GfxRenderer& renderer, int x, int y, int w, int h, const char* label, bool selected) {
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

void StudyRecoveryActivity::onEnter() {
  Activity::onEnter();
  STUDY_REVIEW_QUEUE.loadFromFile();
  STUDY_STATE.loadFromFile();
  showingBack = false;
  selectedIndex = 0;
  actionIndex = 0;
  requestUpdate();
}

void StudyRecoveryActivity::loop() {
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
    const auto card = cards[std::clamp(selectedIndex, 0, static_cast<int>(cards.size()) - 1)];
    if (actionIndex == 0) {
      STUDY_STATE.recordReviewResult(true);
      STUDY_REVIEW_QUEUE.removeAt(StudyQueueKind::Again, selectedIndex);
    } else {
      STUDY_STATE.recordReviewResult(false);
      STUDY_REVIEW_QUEUE.recordAgain({card.front, card.back, "", card.deckName});
    }

    STUDY_REVIEW_QUEUE.loadFromFile();
    const int count = static_cast<int>(STUDY_REVIEW_QUEUE.getCards(StudyQueueKind::Again).size());
    if (count <= 0) {
      selectedIndex = 0;
    } else {
      selectedIndex = std::min(selectedIndex, count - 1);
    }
    showingBack = false;
    requestUpdate();
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

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Recovery");

  const auto& cards = STUDY_REVIEW_QUEUE.getCards(StudyQueueKind::Again);
  if (cards.empty()) {
    const int cardH = 132;
    const int cardY = (pageHeight - cardH) / 2 - 18;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 18, "No recovery cards", true, EpdFontFamily::BOLD);
    renderer.drawLine(pad + 18, cardY + 48, pageWidth - pad - 18, cardY + 48, true);
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 66, "Wrong cards will appear here");
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 94, "Miss a card in Study Cards");
  } else {
    const auto& card = cards[std::clamp(selectedIndex, 0, static_cast<int>(cards.size()) - 1)];
    char meta[48];
    snprintf(meta, sizeof(meta), "%d/%d  Count %u", selectedIndex + 1, static_cast<int>(cards.size()), card.count);
    renderer.drawText(SMALL_FONT_ID, pad, contentTop, meta);
    const std::string deckLabel = renderer.truncatedText(SMALL_FONT_ID, card.deckName.c_str(), pageWidth / 2);
    renderer.drawText(SMALL_FONT_ID, pageWidth - pad - renderer.getTextWidth(SMALL_FONT_ID, deckLabel.c_str()),
                      contentTop, deckLabel.c_str());

    const int cardY = contentTop + 24;
    const int actionArea = showingBack ? 72 : 38;
    const int cardH = contentBottom - cardY - actionArea;
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
      const int btnW = (pageWidth - pad * 2 - 18) / 2;
      const int btnH = 34;
      const int btnTop = contentBottom - 54;
      for (int i = 0; i < kActionCount; i++) {
        const int x = pad + i * (btnW + 18);
        drawActionButton(renderer, x, btnTop, btnW, btnH, kActions[i], i == actionIndex);
      }
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, contentBottom - 30, "Confirm to reveal");
    }
  }

  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), showingBack ? "Apply" : "Flip", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
