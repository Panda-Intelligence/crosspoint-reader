#pragma once

#include <Arduino.h>

#if MOFEI_DEVICE

#ifndef MOFEI_TOUCH_ENABLE
#define MOFEI_TOUCH_ENABLE 1
#endif

#ifndef MOFEI_TOUCH_SDA
#define MOFEI_TOUCH_SDA 13
#endif

#ifndef MOFEI_TOUCH_SCL
#define MOFEI_TOUCH_SCL 12
#endif

#ifndef MOFEI_TOUCH_PWR
#define MOFEI_TOUCH_PWR 45
#endif

#ifndef MOFEI_TOUCH_PWR_ENABLE_LEVEL
#define MOFEI_TOUCH_PWR_ENABLE_LEVEL LOW
#endif

#ifndef MOFEI_TOUCH_INT
#define MOFEI_TOUCH_INT 44
#endif

#ifndef MOFEI_TOUCH_POLL_INTERVAL_MS
#define MOFEI_TOUCH_POLL_INTERVAL_MS 10
#endif

#ifndef MOFEI_TOUCH_RST
#define MOFEI_TOUCH_RST 7
#endif

#ifndef MOFEI_TOUCH_I2C_FREQ
#define MOFEI_TOUCH_I2C_FREQ 400000
#endif

#ifndef MOFEI_TOUCH_ADDR
#define MOFEI_TOUCH_ADDR 0x2E
#endif

#ifndef MOFEI_TOUCH_AUTOSCAN
#define MOFEI_TOUCH_AUTOSCAN 0
#endif

#ifndef MOFEI_TOUCH_SOFT_I2C
#define MOFEI_TOUCH_SOFT_I2C 1
#endif

#ifndef MOFEI_TOUCH_DIAGNOSTIC_LOG
#define MOFEI_TOUCH_DIAGNOSTIC_LOG 0
#endif

#ifndef MOFEI_TOUCH_SCAN
#define MOFEI_TOUCH_SCAN 0
#endif

#ifndef MOFEI_TOUCH_WIDTH
#define MOFEI_TOUCH_WIDTH 480
#endif

#ifndef MOFEI_TOUCH_HEIGHT
#define MOFEI_TOUCH_HEIGHT 800
#endif

#ifndef MOFEI_TOUCH_LOGICAL_WIDTH
#define MOFEI_TOUCH_LOGICAL_WIDTH 480
#endif

#ifndef MOFEI_TOUCH_LOGICAL_HEIGHT
#define MOFEI_TOUCH_LOGICAL_HEIGHT 800
#endif

#ifndef MOFEI_TOUCH_SWAP_XY
#define MOFEI_TOUCH_SWAP_XY 0
#endif

#ifndef MOFEI_TOUCH_INVERT_X
#define MOFEI_TOUCH_INVERT_X 0
#endif

#ifndef MOFEI_TOUCH_INVERT_Y
#define MOFEI_TOUCH_INVERT_Y 0
#endif

#ifndef MOFEI_TOUCH_OFFSET_X
#define MOFEI_TOUCH_OFFSET_X 0
#endif

#ifndef MOFEI_TOUCH_OFFSET_Y
#define MOFEI_TOUCH_OFFSET_Y 0
#endif

class MofeiTouchDriver {
 public:
  enum class EventType : uint8_t { None, Tap, SwipeLeft, SwipeRight, SwipeUp, SwipeDown };

  struct Event {
    EventType type = EventType::None;
    uint16_t x = 0;
    uint16_t y = 0;
  };

  void begin();
  bool update(Event* outEvent);
  bool available() const { return ready; }

 private:
  bool ready = false;
  bool touchDown = false;
  bool lastReadInvalidFrame = false;
  bool useSoftwareI2c = false;
  uint8_t lastLoggedFrameStatus = 0xFF;
  unsigned long lastStatusLogMs = 0;
  unsigned long lastReadErrorLogMs = 0;
  unsigned long lastRetryMs = 0;  // for late-init retry
  unsigned long lastPollMs = 0;
  unsigned long lastDiagnosticSampleMs = 0;
  int activeSda = MOFEI_TOUCH_SDA;
  int activeScl = MOFEI_TOUCH_SCL;
  uint16_t startX = 0;
  uint16_t startY = 0;
  uint16_t lastX = 0;
  uint16_t lastY = 0;
  unsigned long startMs = 0;

  bool readPoint(uint16_t* x, uint16_t* y, bool* released, bool* hasPoint);
  bool detectOnPins(int sda, int scl);
#if MOFEI_TOUCH_AUTOSCAN
  bool autoDetectPins();
#endif
  bool readRegister(uint8_t reg, uint8_t* buffer, uint8_t len);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool configureController();
  void markUnavailable();
  Event finishTouch();
  void normalizePoint(uint16_t* x, uint16_t* y) const;
  void logStatus(unsigned long now);
  void logDiagnosticSample(unsigned long now);
};

#endif
