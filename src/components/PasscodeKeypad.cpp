#include "PasscodeKeypad.h"

#include <cstring>

namespace {

void labelOf(int keyIdx, char* outLabel) {
  if (keyIdx == PasscodeKeypadView::kBackspaceIdx) {
    outLabel[0] = '<';
    outLabel[1] = '\0';
  } else if (keyIdx == PasscodeKeypadView::kZeroIdx) {
    outLabel[0] = '0';
    outLabel[1] = '\0';
  } else if (keyIdx == PasscodeKeypadView::kOkIdx) {
    strncpy(outLabel, "OK", 4);
  } else {
    outLabel[0] = static_cast<char>('1' + keyIdx);
    outLabel[1] = '\0';
  }
}

}  // namespace

void PasscodeKeypadView::layout(const GfxRenderer& renderer, int topBand, int bottomMargin) {
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  const int keypadHeight = sh - topBand - bottomMargin;
  const int keypadWidth = sw - 80;
  constexpr int rows = 4;
  constexpr int cols = 3;
  const int keyW = keypadWidth / cols;
  const int keyH = keypadHeight / rows;
  const int xOffset = (sw - keyW * cols) / 2;
  const int yOffset = topBand;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      const int idx = row * cols + col;
      keys[idx] = KeyRect{xOffset + col * keyW, yOffset + row * keyH, keyW, keyH};
    }
  }
}

int PasscodeKeypadView::hitTestKey(int x, int y) const {
  for (int i = 0; i < kKeyCount; i++) {
    const KeyRect& r = keys[i];
    if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
      return i;
    }
  }
  return -1;
}

PasscodeKeypadView::KeyAction PasscodeKeypadView::decode(int keyIdx, char* outDigit) const {
  if (keyIdx < 0 || keyIdx >= kKeyCount) return KeyAction::None;
  if (keyIdx == kBackspaceIdx) return KeyAction::Backspace;
  if (keyIdx == kOkIdx) return KeyAction::Confirm;
  if (keyIdx == kZeroIdx) {
    if (outDigit) *outDigit = '0';
    return KeyAction::AppendDigit;
  }
  if (outDigit) *outDigit = static_cast<char>('1' + keyIdx);
  return KeyAction::AppendDigit;
}

void PasscodeKeypadView::render(GfxRenderer& renderer, int focusedKey) const {
  for (int i = 0; i < kKeyCount; i++) {
    const KeyRect& r = keys[i];
    const bool focused = (i == focusedKey);
    if (focused) {
      renderer.fillRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, true);
    } else {
      renderer.drawRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, true);
    }
    char label[4] = "";
    labelOf(i, label);
    const int textY = r.y + r.h / 2 - 8;
    renderer.drawText(UI_12_FONT_ID, r.x + r.w / 2 - 4, textY, label, !focused, EpdFontFamily::BOLD);
  }
}

bool PasscodeKeypadView::moveFocus(int& focusedKey, MappedInputManager::Button button) const {
  switch (button) {
    case MappedInputManager::Button::Up:
      if (focusedKey >= 3) {
        focusedKey -= 3;
        return true;
      }
      return false;
    case MappedInputManager::Button::Down:
      if (focusedKey + 3 < kKeyCount) {
        focusedKey += 3;
        return true;
      }
      return false;
    case MappedInputManager::Button::Left:
      if (focusedKey % 3 != 0) {
        focusedKey--;
        return true;
      }
      return false;
    case MappedInputManager::Button::Right:
      if (focusedKey % 3 != 2) {
        focusedKey++;
        return true;
      }
      return false;
    default:
      return false;
  }
}

void PasscodeKeypadView::drawDots(GfxRenderer& renderer, int x, int y, int filled) {
  for (int i = 0; i < kPasscodeLen; i++) {
    const int dx = x + i * 18;
    if (i < filled) {
      renderer.fillRect(dx, y, 12, 12, true);
    } else {
      renderer.drawRect(dx, y, 12, 12, true);
    }
  }
}
