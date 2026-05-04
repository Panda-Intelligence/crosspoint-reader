#include "PasscodeEnrollActivity.h"

#include <I18n.h>

#include <cstring>

#include "../../CrossPointSettings.h"
#include "../../components/PasscodeHash.h"
#include "../../components/UITheme.h"
#include "HalDisplay.h"

namespace {
constexpr int kTopBand = 140;
constexpr int kBottomMargin = 60;
}  // namespace

PasscodeEnrollActivity::PasscodeEnrollActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("PasscodeEnroll", renderer, mappedInput) {}

void PasscodeEnrollActivity::onEnter() {
  Activity::onEnter();

  stage = Stage::Enter;
  status = Status::Prompt;
  filled = 0;
  buffer[0] = '\0';
  firstCode[0] = '\0';
  focusedKey = 1;

  keypad.layout(renderer, kTopBand, kBottomMargin);
  const int sw = renderer.getScreenWidth();
  dotsY = kTopBand - 50;
  dotsX = sw / 2 - (PasscodeKeypadView::kPasscodeLen * 18) / 2;
  statusY = kTopBand - 20;

  dirtyRender = true;
  requestUpdate(true);
}

void PasscodeEnrollActivity::wipeBuffers() {
  for (int i = 0; i < PasscodeKeypadView::kPasscodeLen + 1; i++) {
    firstCode[i] = '\0';
    buffer[i] = '\0';
  }
}

void PasscodeEnrollActivity::appendDigit(char d) {
  if (filled >= PasscodeKeypadView::kPasscodeLen) return;
  buffer[filled++] = d;
  buffer[filled] = '\0';
  if (status == Status::Mismatch && filled > 0) status = Status::Prompt;
  dirtyRender = true;
}

void PasscodeEnrollActivity::backspace() {
  if (filled == 0) return;
  buffer[--filled] = '\0';
  dirtyRender = true;
}

void PasscodeEnrollActivity::confirm() {
  if (filled != PasscodeKeypadView::kPasscodeLen) return;

  if (stage == Stage::Enter) {
    // Save first code and advance.
    memcpy(firstCode, buffer, sizeof(firstCode));
    for (int i = 0; i < PasscodeKeypadView::kPasscodeLen + 1; i++) buffer[i] = '\0';
    filled = 0;
    stage = Stage::Confirm;
    status = Status::Prompt;
    dirtyRender = true;
    return;
  }

  // Stage::Confirm — compare the two buffers in constant time.
  const bool match = passcode::constantTimeEquals(firstCode, buffer);
  // Wipe input regardless of outcome.
  for (int i = 0; i < PasscodeKeypadView::kPasscodeLen + 1; i++) buffer[i] = '\0';
  filled = 0;

  if (!match) {
    // Restart at stage 1 with the mismatch hint.
    for (int i = 0; i < PasscodeKeypadView::kPasscodeLen + 1; i++) firstCode[i] = '\0';
    stage = Stage::Enter;
    status = Status::Mismatch;
    dirtyRender = true;
    return;
  }

  persistAndFinish();
}

void PasscodeEnrollActivity::persistAndFinish() {
  // Generate a fresh per-device salt and the SHA-256 hash of salt||code.
  passcode::generateSaltHex(SETTINGS.lockScreenSalt);
  passcode::hashPasscode(SETTINGS.lockScreenSalt, firstCode, SETTINGS.lockScreenHash);
  SETTINGS.lockScreenEnabled = 1;
  SETTINGS.lockScreenWrongCount = 0;
  SETTINGS.lockScreenLockoutUntilEpoch = 0;
  SETTINGS.saveToFile();

  LOG_DBG("LOCK", "Passcode enrolled");
  wipeBuffers();

  ActivityResult res;
  res.isCancelled = false;
  setResult(std::move(res));
  finish();
}

void PasscodeEnrollActivity::activateKey(int keyIdx) {
  char digit = '\0';
  switch (keypad.decode(keyIdx, &digit)) {
    case PasscodeKeypadView::KeyAction::AppendDigit:
      appendDigit(digit);
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

void PasscodeEnrollActivity::loop() {
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
    wipeBuffers();
    ActivityResult res;
    res.isCancelled = true;
    setResult(std::move(res));
    finish();
    return;
  }

  if (dirtyRender) {
    dirtyRender = false;
    requestUpdate();
  }
}

void PasscodeEnrollActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const StrId titleId =
      stage == Stage::Enter ? StrId::STR_LOCKSCREEN_NEW : StrId::STR_LOCKSCREEN_CONFIRM;
  renderer.drawCenteredText(UI_12_FONT_ID, 36, I18N.get(titleId), true, EpdFontFamily::BOLD);

  PasscodeKeypadView::drawDots(renderer, dotsX, dotsY, filled);

  const char* statusText =
      status == Status::Mismatch ? I18N.get(StrId::STR_LOCKSCREEN_MISMATCH) : I18N.get(StrId::STR_LOCKSCREEN_ENTER);
  renderer.drawCenteredText(UI_10_FONT_ID, statusY, statusText, true);

  keypad.render(renderer, focusedKey);

  const auto labels = mappedInput.mapLabels(I18N.get(StrId::STR_CANCEL), I18N.get(StrId::STR_CONFIRM), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
}
