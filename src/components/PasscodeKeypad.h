#pragma once

#include <cstdint>

#include "../MappedInputManager.h"
#include "../fontIds.h"
#include "GfxRenderer.h"
#include "InputTouchEvent.h"

// Shared 3×4 numeric keypad view used by both LockScreenActivity and
// PasscodeEnrollActivity. Stateless w.r.t. the actual passcode buffer —
// the owning activity owns the digits, this class only handles layout,
// drawing, and input mapping.
//
// Logical key indices (must match the visual layout):
//   0 1 2
//   3 4 5
//   6 7 8
//   9 10 11      (← 0 OK)
//
// Index meaning: 0..8 → digits 1..9, 9 → backspace, 10 → digit 0, 11 → OK.
class PasscodeKeypadView {
 public:
  static constexpr int kKeyCount = 12;
  static constexpr int kBackspaceIdx = 9;
  static constexpr int kZeroIdx = 10;
  static constexpr int kOkIdx = 11;
  static constexpr int kPasscodeLen = 4;

  // What activateKey() is asking the owner to do.
  enum class KeyAction : uint8_t { None, AppendDigit, Backspace, Confirm };

  PasscodeKeypadView() = default;

  // Compute the per-key rectangles for the current screen size. Reserves
  // space at the top for title/dots/status (topBand) and at the bottom for
  // button hints (bottomMargin).
  void layout(const GfxRenderer& renderer, int topBand, int bottomMargin);

  // Hit-test a touch coordinate. Returns key index, or -1 if outside any key.
  int hitTestKey(int x, int y) const;

  // Decode a key index into an action + (for AppendDigit) the digit char.
  KeyAction decode(int keyIdx, char* outDigit) const;

  // Render the keypad. Caller has already cleared the screen and drawn its
  // own header. focusedKey may be -1 to render with no focus.
  void render(GfxRenderer& renderer, int focusedKey) const;

  // D-pad navigation. Updates focusedKey in place; returns true if it moved.
  bool moveFocus(int& focusedKey, MappedInputManager::Button button) const;

  // Draw kPasscodeLen progress dots at (x,y); filled count = filled.
  static void drawDots(GfxRenderer& renderer, int x, int y, int filled);

 private:
  struct KeyRect {
    int x;
    int y;
    int w;
    int h;
  };
  KeyRect keys[kKeyCount] = {};
};
