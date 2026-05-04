#pragma once

#include <cstdint>

#include "../../components/PasscodeKeypad.h"
#include "../Activity.h"

// PasscodeEnrollActivity — captures a new 4-digit passcode in two stages
// (enter, then confirm). On match: derives a fresh 16-byte salt, computes
// SHA-256(salt || code), persists both to SETTINGS, and finishes with
// success. On mismatch: re-prompts at stage 1 with a "codes don't match"
// hint.
//
// This activity does NOT verify any existing passcode. The caller (e.g.,
// the Settings action handler) is responsible for chaining a
// LockScreenActivity{VERIFY} sub-activity first when an existing passcode
// is present.
class PasscodeEnrollActivity : public Activity {
 public:
  PasscodeEnrollActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class Stage : uint8_t { Enter, Confirm };
  enum class Status : uint8_t { Prompt, Mismatch };

  PasscodeKeypadView keypad;
  Stage stage = Stage::Enter;
  Status status = Status::Prompt;
  int focusedKey = 1;
  bool dirtyRender = true;

  int dotsX = 0;
  int dotsY = 0;
  int statusY = 0;

  // Captured first-stage code (kept on stack of this activity object;
  // wiped after enrollment finishes or on cancel).
  char firstCode[PasscodeKeypadView::kPasscodeLen + 1] = "";
  // Current input buffer.
  char buffer[PasscodeKeypadView::kPasscodeLen + 1] = "";
  int filled = 0;

  void appendDigit(char d);
  void backspace();
  void confirm();
  void activateKey(int keyIdx);

  // Generate salt+hash from firstCode and persist; finish with success.
  void persistAndFinish();

  void wipeBuffers();
};
