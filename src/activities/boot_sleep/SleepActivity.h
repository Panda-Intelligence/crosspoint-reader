#pragma once
#include "../Activity.h"

class Bitmap;

class SleepActivity final : public Activity {
  bool previewMode = false;

 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Sleep", renderer, mappedInput) {}
  SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool previewMode)
      : Activity("SleepPreview", renderer, mappedInput), previewMode(previewMode) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderCoverSleepScreen() const;
  void renderBitmapSleepScreen(const Bitmap& bitmap) const;
  void renderBlankSleepScreen() const;
};
