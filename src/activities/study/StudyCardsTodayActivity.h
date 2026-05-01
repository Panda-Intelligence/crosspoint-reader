#pragma once

#include <I18nKeys.h>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class StudyCardsTodayActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  bool inSession = false;
  bool showingBack = false;
  bool sessionComplete = false;
  int cardIndex = 0;
  int actionIndex = 0;
  int reviewedInSession = 0;
  int sessionAgainCount = 0;
  int sessionLaterCount = 0;
  int savedCount = 0;

  enum class NextStep { Recovery, Later, Saved, Report };

  void startSession();
  void advanceCard(bool recordResult, bool correct, bool saved);
  void applySelectedAction();
  NextStep recommendedNextStep() const;
  StrId nextStepLabelId() const;
  StrId nextStepHintId() const;
  void openRecommendedNextStep();

 public:
  explicit StudyCardsTodayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("StudyCardsToday", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
