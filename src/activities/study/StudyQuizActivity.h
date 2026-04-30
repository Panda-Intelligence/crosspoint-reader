#pragma once

#include <array>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class StudyQuizActivity final : public Activity {
 public:
  enum class QuizMode : uint8_t { TwoChoice, TrueFalse, FirstLetter };

 private:
  ButtonNavigator buttonNavigator;
  int questionIndex = 0;
  int selectedOption = 0;
  int correctOption = 0;
  bool showingResult = false;
  bool answerCorrect = false;
  bool truthPromptMatches = false;
  QuizMode mode = QuizMode::TwoChoice;

  void loadQuestion();
  void cycleMode();
  void confirmSelection();

 public:
  explicit StudyQuizActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("StudyQuiz", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
