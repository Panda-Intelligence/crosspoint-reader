#pragma once

#include <array>

#include "../Activity.h"

class FortuneActivity final : public Activity {
 public:
  explicit FortuneActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Fortune", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Mode { Fortune, Coin };

  Mode mode = Mode::Fortune;
  int selectedFortune = 0;
  bool coinHeads = true;

  void drawAgain();
  void toggleMode();
  const char* modeLabel() const;
  const char* resultTitle() const;
  const char* resultDetail() const;
};
