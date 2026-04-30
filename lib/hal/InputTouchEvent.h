#pragma once

#include <cstdint>

struct InputTouchEvent {
  enum class Type : uint8_t { None, Tap, SwipeLeft, SwipeRight, SwipeUp, SwipeDown };

  Type type = Type::None;
  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  bool hasRaw = false;

  bool isTap() const { return type == Type::Tap; }
  uint16_t sourceX() const { return hasRaw ? rawX : x; }
  uint16_t sourceY() const { return hasRaw ? rawY : y; }
};
