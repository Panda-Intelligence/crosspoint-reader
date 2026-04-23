#pragma once

#include <array>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class StudyQuizActivity final : public Activity {
 public:
  enum class QuizMode : uint8_t { TwoChoice, TrueFalse };

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

 public:
  explicit StudyQuizActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("StudyQuiz", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
