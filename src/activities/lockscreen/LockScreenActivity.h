#pragma once

#include <cstdint>

#include "../../fontIds.h"
#include "../Activity.h"
#include "../../components/PasscodeKeypad.h"

// LockScreenActivity — 4-digit numeric passcode entry.
//
// Used in three modes (see Mode enum below):
//   - WAKE_GUARD: shown on wake-from-sleep / cold boot when enabled.
//     Cannot be dismissed; success routes to home via activityManager.goHome().
//   - VERIFY: sub-activity. Returns success/cancel via ActivityResult.
//     Used as Step 0 of "change passcode".
//   - VERIFY_AND_CLEAR: sub-activity. Same as VERIFY but on success it
//     clears the stored hash+salt and toggles lockScreenEnabled off.
//
// Lockout: 3 wrong attempts → 30s lockout window, persisted to SETTINGS so
// reboot cannot reset the rate-limit. While locked the keypad ignores
// digit input and the status line shows the remaining seconds.
class LockScreenActivity : public Activity {
 public:
  enum class Mode : uint8_t { WAKE_GUARD, VERIFY, VERIFY_AND_CLEAR };

  LockScreenActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Mode mode);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class Status : uint8_t { Prompt, Wrong, LockedOut, NoPasscode };

  Mode mode;
  PasscodeKeypadView keypad;
  int focusedKey = 1;

  // Layout anchors.
  int dotsX = 0;
  int dotsY = 0;
  int statusY = 0;

  char buffer[PasscodeKeypadView::kPasscodeLen + 1] = "";
  int filled = 0;
  bool dirtyRender = true;
  Status status = Status::Prompt;
  uint32_t lastDrawSecond = 0;

  void appendDigit(char digit);
  void backspace();
  void confirm();
  void activateKey(int keyIdx);
  bool isLockedOut() const;
  uint32_t lockoutSecondsRemaining() const;
  void onUnlocked();
};
