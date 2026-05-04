#include "LockScreenActivity.h"

#include <I18n.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "../../CrossPointSettings.h"
#include "../../components/PasscodeHash.h"
#include "../../components/UITheme.h"
#include "HalDisplay.h"

namespace {
constexpr int kTopBand = 140;     // title + dots + status
constexpr int kBottomMargin = 60; // button hints
}  // namespace

LockScreenActivity::LockScreenActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Mode mode)
    : Activity("LockScreen", renderer, mappedInput), mode(mode) {}

void LockScreenActivity::onEnter() {
  Activity::onEnter();

  filled = 0;
  buffer[0] = '\0';
  focusedKey = 1;

  if (SETTINGS.lockScreenHash[0] == '\0' || SETTINGS.lockScreenSalt[0] == '\0') {
    status = Status::NoPasscode;
  } else if (isLockedOut()) {
    status = Status::LockedOut;
  } else {
    status = Status::Prompt;
  }

  keypad.layout(renderer, kTopBand, kBottomMargin);
  const int sw = renderer.getScreenWidth();
  dotsY = kTopBand - 50;
  dotsX = sw / 2 - (PasscodeKeypadView::kPasscodeLen * 18) / 2;
  statusY = kTopBand - 20;

  dirtyRender = true;
  requestUpdate(true);
}

bool LockScreenActivity::isLockedOut() const {
  if (SETTINGS.lockScreenLockoutUntilEpoch == 0) return false;
  const auto now = static_cast<uint32_t>(time(nullptr));
  return now < SETTINGS.lockScreenLockoutUntilEpoch;
}

uint32_t LockScreenActivity::lockoutSecondsRemaining() const {
  if (SETTINGS.lockScreenLockoutUntilEpoch == 0) return 0;
  const auto now = static_cast<uint32_t>(time(nullptr));
  if (now >= SETTINGS.lockScreenLockoutUntilEpoch) return 0;
  return SETTINGS.lockScreenLockoutUntilEpoch - now;
}

void LockScreenActivity::appendDigit(char digit) {
  if (filled >= PasscodeKeypadView::kPasscodeLen) return;
  buffer[filled++] = digit;
  buffer[filled] = '\0';
  dirtyRender = true;
}

void LockScreenActivity::backspace() {
  if (filled == 0) return;
  buffer[--filled] = '\0';
  dirtyRender = true;
}

void LockScreenActivity::confirm() {
  if (filled != PasscodeKeypadView::kPasscodeLen) return;
  if (status == Status::NoPasscode) return;
  if (isLockedOut()) {
    status = Status::LockedOut;
    dirtyRender = true;
    return;
  }

  char enteredHash[passcode::kHashHexLen + 1] = "";
  passcode::hashPasscode(SETTINGS.lockScreenSalt, buffer, enteredHash);
  const bool match = passcode::constantTimeEquals(enteredHash, SETTINGS.lockScreenHash);

  // Wipe the in-memory passcode.
  for (int i = 0; i < PasscodeKeypadView::kPasscodeLen + 1; i++) buffer[i] = '\0';
  filled = 0;

  if (match) {
    LOG_DBG("LOCK", "Unlocked");
    SETTINGS.lockScreenWrongCount = 0;
    SETTINGS.lockScreenLockoutUntilEpoch = 0;
    if (mode == Mode::VERIFY_AND_CLEAR) {
      SETTINGS.lockScreenHash[0] = '\0';
      SETTINGS.lockScreenSalt[0] = '\0';
      SETTINGS.lockScreenEnabled = 0;
    }
    SETTINGS.saveToFile();
    onUnlocked();
    return;
  }

  SETTINGS.lockScreenWrongCount = static_cast<uint8_t>(SETTINGS.lockScreenWrongCount + 1);
  LOG_DBG("LOCK", "Wrong attempt %u", SETTINGS.lockScreenWrongCount);
  if (SETTINGS.lockScreenWrongCount >= 3) {
    SETTINGS.lockScreenLockoutUntilEpoch = static_cast<uint32_t>(time(nullptr)) + 30;
    SETTINGS.lockScreenWrongCount = 0;
    status = Status::LockedOut;
  } else {
    status = Status::Wrong;
  }
  SETTINGS.saveToFile();
  dirtyRender = true;
}

void LockScreenActivity::onUnlocked() {
  if (mode == Mode::WAKE_GUARD) {
    activityManager.goHome();
    return;
  }
  ActivityResult res;
  res.isCancelled = false;
  setResult(std::move(res));
  finish();
}

void LockScreenActivity::activateKey(int keyIdx) {
  if (isLockedOut()) {
    status = Status::LockedOut;
    dirtyRender = true;
    return;
  }
  char digit = '\0';
  switch (keypad.decode(keyIdx, &digit)) {
    case PasscodeKeypadView::KeyAction::AppendDigit:
      appendDigit(digit);
      if (status == Status::Wrong && filled > 0) status = Status::Prompt;
      break;
    case PasscodeKeypadView::KeyAction::Backspace:
      backspace();
      break;
    case PasscodeKeypadView::KeyAction::Confirm:
      confirm();
      break;
    default:
      break;
  }
}

void LockScreenActivity::loop() {
  // Clear lockout once the deadline passes.
  if (status == Status::LockedOut && !isLockedOut()) {
    status = Status::Prompt;
    dirtyRender = true;
  }
  // Update the countdown each second while locked.
  if (status == Status::LockedOut) {
    const uint32_t s = lockoutSecondsRemaining();
    if (s != lastDrawSecond) {
      lastDrawSecond = s;
      dirtyRender = true;
    }
  }

  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (touchEvent.isTap()) {
      const int keyIdx = keypad.hitTestKey(touchEvent.x, touchEvent.y);
      if (keyIdx >= 0) {
        focusedKey = keyIdx;
        activateKey(keyIdx);
      }
    }
  }

  for (auto btn : {MappedInputManager::Button::Up, MappedInputManager::Button::Down,
                   MappedInputManager::Button::Left, MappedInputManager::Button::Right}) {
    if (mappedInput.wasReleased(btn)) {
      if (keypad.moveFocus(focusedKey, btn)) dirtyRender = true;
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateKey(focusedKey);
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (mode != Mode::WAKE_GUARD) {
      ActivityResult res;
      res.isCancelled = true;
      setResult(std::move(res));
      finish();
      return;
    }
  }

  if (dirtyRender) {
    dirtyRender = false;
    requestUpdate();
  }
}

void LockScreenActivity::render(RenderLock&&) {
  renderer.clearScreen();

  renderer.drawCenteredText(UI_12_FONT_ID, 36, I18N.get(StrId::STR_LOCKSCREEN_TITLE), true, EpdFontFamily::BOLD);
  PasscodeKeypadView::drawDots(renderer, dotsX, dotsY, filled);

  char statusBuf[64];
  const char* statusText = "";
  switch (status) {
    case Status::Prompt:
      statusText = I18N.get(StrId::STR_LOCKSCREEN_ENTER);
      break;
    case Status::Wrong:
      statusText = I18N.get(StrId::STR_LOCKSCREEN_WRONG);
      break;
    case Status::NoPasscode:
      statusText = I18N.get(StrId::STR_LOCKSCREEN_NOT_SET);
      break;
    case Status::LockedOut: {
      const uint32_t remaining = lockoutSecondsRemaining();
      snprintf(statusBuf, sizeof(statusBuf), I18N.get(StrId::STR_LOCKSCREEN_LOCKED), static_cast<unsigned>(remaining));
      statusText = statusBuf;
      break;
    }
  }
  renderer.drawCenteredText(UI_10_FONT_ID, statusY, statusText, true);

  keypad.render(renderer, focusedKey);

  if (mode == Mode::WAKE_GUARD) {
    const auto labels = mappedInput.mapLabels("", I18N.get(StrId::STR_CONFIRM), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else {
    const auto labels = mappedInput.mapLabels(I18N.get(StrId::STR_CANCEL), I18N.get(StrId::STR_CONFIRM), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
}
