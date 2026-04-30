#include "StudyQuizActivity.h"

#include <I18n.h>

#include <algorithm>

#include "StudyDeckStore.h"
#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
const char* modeLabel(const StudyQuizActivity::QuizMode mode) {
  switch (mode) {
    case StudyQuizActivity::QuizMode::TrueFalse:
      return "True / False";
    case StudyQuizActivity::QuizMode::FirstLetter:
      return "First Letter";
    case StudyQuizActivity::QuizMode::TwoChoice:
    default:
      return "Two Choice";
  }
}

void drawOptionButton(const GfxRenderer& renderer, int x, int y, int w, int h, const std::string& text, bool selected,
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
  } else if (mode == QuizMode::TrueFalse) {
    truthPromptMatches = (questionIndex % 2) == 0;
    correctOption = truthPromptMatches ? 0 : 1;
  } else {
    correctOption = questionIndex % 2;
    truthPromptMatches = false;
  }
}

void StudyQuizActivity::cycleMode() {
  switch (mode) {
    case QuizMode::TwoChoice:
      mode = QuizMode::TrueFalse;
      break;
    case QuizMode::TrueFalse:
      mode = QuizMode::FirstLetter;
      break;
    case QuizMode::FirstLetter:
    default:
      mode = QuizMode::TwoChoice;
      break;
  }
  loadQuestion();
  requestUpdate();
}

void StudyQuizActivity::confirmSelection() {
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

void StudyQuizActivity::onEnter() {
  Activity::onEnter();
  STUDY_STATE.loadFromFile();
  STUDY_DECKS.refresh();
  questionIndex = 0;
  loadQuestion();
  requestUpdate();
}

void StudyQuizActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent)) {
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      mappedInput.suppressTouchButtonFallback();
      if (STUDY_DECKS.getCards().size() < 2) {
        STUDY_DECKS.refresh();
        loadQuestion();
        requestUpdate();
        return;
      }
      if (!showingResult) {
        const auto& metrics = UITheme::getInstance().getMetrics();
        const int pageWidth = renderer.getScreenWidth();
        const int pageHeight = renderer.getScreenHeight();
        const int pad = metrics.contentSidePadding;
        const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
        const int cardY = contentTop + 24;
        const int optionTop = cardY + 112 + 16;
        const int optionH = 42;
        const int optionW = pageWidth - pad * 2;
        const Rect option0{pad, optionTop, optionW, optionH};
        const Rect option1{pad, optionTop + optionH + 12, optionW, optionH};
        if (TouchHitTest::pointInRect(touchEvent.x, touchEvent.y, option0) ||
            TouchHitTest::pointInRect(touchEvent.x, touchEvent.y, option1)) {
          selectedOption = TouchHitTest::pointInRect(touchEvent.x, touchEvent.y, option0) ? 0 : 1;
        } else if (touchEvent.y < contentTop + 28) {
          cycleMode();
          return;
        } else if (touchEvent.y > pageHeight / 2) {
          selectedOption = 1;
        } else {
          selectedOption = 0;
        }
      }
      confirmSelection();
      return;
    }
    if (!buttonHintTap && STUDY_DECKS.getCards().size() >= 2 && !showingResult &&
        (touchEvent.type == InputTouchEvent::Type::SwipeLeft ||
         touchEvent.type == InputTouchEvent::Type::SwipeRight)) {
      mappedInput.suppressTouchButtonFallback();
      cycleMode();
      return;
    }
    if (!buttonHintTap && STUDY_DECKS.getCards().size() >= 2 && !showingResult &&
        TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedOption = ButtonNavigator::nextIndex(selectedOption, 2);
      requestUpdate();
      return;
    }
    if (!buttonHintTap && STUDY_DECKS.getCards().size() >= 2 && !showingResult &&
        TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedOption = ButtonNavigator::previousIndex(selectedOption, 2);
      requestUpdate();
      return;
    }
  }

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
      cycleMode();
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
    confirmSelection();
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
    std::string options[2];
    if (mode == QuizMode::TwoChoice) {
      options[0] = correctOption == 0 ? question.back : distractor.back;
      options[1] = correctOption == 1 ? question.back : distractor.back;
    } else if (mode == QuizMode::TrueFalse) {
      options[0] = "True";
      options[1] = "False";
    } else {
      const char correctLetter = question.front.empty() ? '?' : static_cast<char>(toupper(question.front.front()));
      const char wrongLetter = distractor.front.empty() ? 'X' : static_cast<char>(toupper(distractor.front.front()));
      options[0] = std::string(1, correctOption == 0 ? correctLetter : wrongLetter);
      options[1] = std::string(1, correctOption == 1 ? correctLetter : wrongLetter);
    }

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
    renderer.drawText(SMALL_FONT_ID, pad + 14, cardY + 14, mode == QuizMode::TwoChoice
                                                           ? "Pick the correct answer"
                                                           : (mode == QuizMode::TrueFalse ? "Does this answer match?"
                                                                                          : "Pick the first letter"));

    std::string prompt = question.front;
    if (mode == QuizMode::TrueFalse) {
      prompt += "\n\n" + candidate.back;
    } else if (mode == QuizMode::FirstLetter) {
      prompt = question.back;
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
