#include "StudyQuizActivity.h"

#include <I18n.h>

#include <algorithm>

#include "StudyDeckStore.h"
#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
const char* modeLabel(const StudyQuizActivity::QuizMode mode) {
  return mode == StudyQuizActivity::QuizMode::TwoChoice ? "Two Choice" : "True / False";
}

void drawOptionButton(GfxRenderer& renderer, int x, int y, int w, int h, const std::string& text, bool selected,
                      bool reveal, bool correct) {
  if (reveal && correct) {
    renderer.fillRoundedRect(x, y, w, h, 8, Color::Black);
  } else if (selected) {
    renderer.fillRoundedRect(x, y, w, h, 8, Color::LightGray);
  } else {
    renderer.drawRoundedRect(x, y, w, h, 1, 8, true);
  }

  const std::string line = renderer.truncatedText(SMALL_FONT_ID, text.c_str(), w - 20);
  const int tw = renderer.getTextWidth(SMALL_FONT_ID, line.c_str(), reveal && correct ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  const int th = renderer.getTextHeight(SMALL_FONT_ID);
  const bool blackText = !(reveal && correct);
  renderer.drawText(SMALL_FONT_ID, x + (w - tw) / 2, y + (h - th) / 2, line.c_str(), blackText,
                    reveal && correct ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
}
}  // namespace

void StudyQuizActivity::loadQuestion() {
  const auto& cards = STUDY_DECKS.getCards();
  if (cards.size() < 2) {
    questionIndex = 0;
    selectedOption = 0;
    correctOption = 0;
    showingResult = false;
    answerCorrect = false;
    return;
  }

  questionIndex %= static_cast<int>(cards.size());
  selectedOption = 0;
  showingResult = false;
  answerCorrect = false;
  if (mode == QuizMode::TwoChoice) {
    correctOption = questionIndex % 2;
    truthPromptMatches = false;
  } else {
    truthPromptMatches = (questionIndex % 2) == 0;
    correctOption = truthPromptMatches ? 0 : 1;
  }
}

void StudyQuizActivity::onEnter() {
  Activity::onEnter();
  STUDY_STATE.loadFromFile();
  STUDY_DECKS.refresh();
  questionIndex = 0;
  loadQuestion();
  requestUpdate();
}

void StudyQuizActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (STUDY_DECKS.getCards().size() < 2) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      STUDY_DECKS.refresh();
      loadQuestion();
      requestUpdate();
    }
    return;
  }

  if (!showingResult) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
        mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      mode = mode == QuizMode::TwoChoice ? QuizMode::TrueFalse : QuizMode::TwoChoice;
      loadQuestion();
      requestUpdate();
      return;
    }

    buttonNavigator.onNextRelease([this] {
      selectedOption = ButtonNavigator::nextIndex(selectedOption, 2);
      requestUpdate();
    });

    buttonNavigator.onPreviousRelease([this] {
      selectedOption = ButtonNavigator::previousIndex(selectedOption, 2);
      requestUpdate();
    });
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (!showingResult) {
      showingResult = true;
      answerCorrect = selectedOption == correctOption;
      STUDY_STATE.recordReviewResult(answerCorrect);
      requestUpdate();
      return;
    }

    questionIndex = (questionIndex + 1) % static_cast<int>(STUDY_DECKS.getCards().size());
    loadQuestion();
    requestUpdate();
  }
}

void StudyQuizActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int pad = metrics.contentSidePadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - 8;

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Quiz Practice");

  const auto& cards = STUDY_DECKS.getCards();
  if (cards.size() < 2) {
    const int cardH = 132;
    const int cardY = (pageHeight - cardH) / 2 - 18;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 18, "Need at least 2 cards", true, EpdFontFamily::BOLD);
    renderer.drawLine(pad + 18, cardY + 48, pageWidth - pad - 18, cardY + 48, true);
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 66, "Import more deck cards");
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 94, "Press Confirm to refresh");
  } else {
    const StudyCard& question = cards[questionIndex];
    const StudyCard& distractor = cards[(questionIndex + 1) % static_cast<int>(cards.size())];
    const StudyCard& candidate = truthPromptMatches ? question : distractor;
    const std::string options[2] = {
        mode == QuizMode::TwoChoice ? (correctOption == 0 ? question.back : distractor.back) : std::string("True"),
        mode == QuizMode::TwoChoice ? (correctOption == 1 ? question.back : distractor.back) : std::string("False")};

    char meta[32];
    snprintf(meta, sizeof(meta), "Question %d/%d", questionIndex + 1, static_cast<int>(cards.size()));
    renderer.drawText(SMALL_FONT_ID, pad, contentTop, meta);
    const std::string deckName = renderer.truncatedText(SMALL_FONT_ID, question.deckName.c_str(), pageWidth / 2);
    renderer.drawText(SMALL_FONT_ID, pageWidth - pad - renderer.getTextWidth(SMALL_FONT_ID, deckName.c_str()), contentTop,
                      deckName.c_str());
    renderer.drawCenteredText(SMALL_FONT_ID, contentTop, modeLabel(mode));

    const int cardY = contentTop + 24;
    const int cardH = 112;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawText(SMALL_FONT_ID, pad + 14, cardY + 14,
                      mode == QuizMode::TwoChoice ? "Pick the correct answer" : "Does this answer match?");

    std::string prompt = question.front;
    if (mode == QuizMode::TrueFalse) {
      prompt += "\n\n" + candidate.back;
    }

    const auto promptLines =
        renderer.wrappedText(UI_12_FONT_ID, prompt.c_str(), pageWidth - pad * 2 - 32, 4, EpdFontFamily::BOLD);
    const int promptY = cardY + 46;
    for (size_t i = 0; i < promptLines.size(); i++) {
      renderer.drawCenteredText(UI_12_FONT_ID, promptY + static_cast<int>(i) * 26, promptLines[i].c_str(), true,
                                EpdFontFamily::BOLD);
    }

    const int optionTop = cardY + cardH + 16;
    const int optionH = 42;
    const int optionW = pageWidth - pad * 2;
    drawOptionButton(renderer, pad, optionTop, optionW, optionH, options[0], selectedOption == 0, showingResult,
                     correctOption == 0);
    drawOptionButton(renderer, pad, optionTop + optionH + 12, optionW, optionH, options[1], selectedOption == 1,
                     showingResult, correctOption == 1);

    if (showingResult) {
      renderer.drawCenteredText(UI_10_FONT_ID, contentBottom - 30, answerCorrect ? "Correct - Confirm for next"
                                                                                 : "Wrong - Confirm for next");
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, contentBottom - 30, "Up/Down answer  Left/Right mode");
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Confirm", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
