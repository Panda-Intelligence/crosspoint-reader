#pragma once

#include <cstdint>

struct InputTouchEvent {
  enum class Type : uint8_t { None, Tap, SwipeLeft, SwipeRight, SwipeUp, SwipeDown };

  Type type = Type::None;
  uint16_t x = 0;
  uint16_t y = 0;

  bool isTap() const { return type == Type::Tap; }
};
