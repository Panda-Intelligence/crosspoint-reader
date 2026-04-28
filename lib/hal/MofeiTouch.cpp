#include "MofeiTouch.h"

#if MOFEI_DEVICE

#include <Logging.h>
#include <Wire.h>

namespace {
constexpr uint8_t FT6336_ADDR = 0x38;
constexpr uint8_t FT6336_REG_DEVICE_MODE = 0x00;
constexpr uint8_t FT6336_REG_TD_STATUS = 0x02;
constexpr uint8_t FT6336_REG_THGROUP = 0x80;
constexpr uint8_t FT6336_REG_PERIODACTIVE = 0x88;
constexpr uint8_t FT6336_REG_FIRMWARE_ID = 0xA6;
constexpr uint8_t FT6336_MAX_POINTS = 2;
constexpr uint8_t FT6336_DEVICE_MODE_WORKING = 0;
constexpr uint8_t FT6336_THGROUP_DEFAULT = 22;
constexpr uint8_t FT6336_PERIODACTIVE_DEFAULT = 14;
constexpr uint16_t SWIPE_THRESHOLD_PX = 80;
constexpr uint16_t TAP_MAX_MOVE_PX = 60;
constexpr unsigned long TOUCH_TIMEOUT_MS = 1200;
constexpr unsigned long TOUCH_STATUS_LOG_INTERVAL_MS = 10000;
constexpr unsigned long TOUCH_READ_ERROR_LOG_INTERVAL_MS = 5000;
constexpr unsigned long TOUCH_POWER_SETTLE_MS = 1000;  // FT6336U needs ~1s after power-on before I2C is ready
#if MOFEI_TOUCH_AUTOSCAN
// GPIO 0-2: keys; 3-8: EPD SPI; 9: BAT; 10-11,14-18,21: SD-MMC; 19-20: USB.
// Only GPIO12 and GPIO13 are free for I2C touch.
constexpr int TOUCH_AUTO_PIN_CANDIDATES[] = {12, 13};
#endif

bool readFt6336(uint8_t reg, uint8_t* buffer, uint8_t len) {
  Wire.beginTransmission(FT6336_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(FT6336_ADDR, len, static_cast<uint8_t>(true)) != len) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  for (uint8_t i = 0; i < len; ++i) {
    buffer[i] = Wire.read();
  }
  return true;
}

bool writeFt6336(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(FT6336_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

uint16_t scaleAxis(uint16_t value, uint16_t rawExtent, uint16_t logicalExtent) {
  if (rawExtent <= 1 || logicalExtent <= 1) {
    return 0;
  }
  if (value >= rawExtent) {
    value = rawExtent - 1;
  }
  return static_cast<uint16_t>((static_cast<uint32_t>(value) * logicalExtent) / rawExtent);
}
}  // namespace

void MofeiTouchDriver::begin() {
#if MOFEI_TOUCH_ENABLE
  ready = false;
  activeSda = MOFEI_TOUCH_SDA;
  activeScl = MOFEI_TOUCH_SCL;

  if (MOFEI_TOUCH_PWR >= 0) {
    pinMode(MOFEI_TOUCH_PWR, OUTPUT);
    digitalWrite(MOFEI_TOUCH_PWR, MOFEI_TOUCH_PWR_ENABLE_LEVEL);
    LOG_INF("TOUCH", "Mofei touch power GPIO%d=%d", MOFEI_TOUCH_PWR, MOFEI_TOUCH_PWR_ENABLE_LEVEL);
    delay(TOUCH_POWER_SETTLE_MS);
  }
  if (MOFEI_TOUCH_INT >= 0) {
    pinMode(MOFEI_TOUCH_INT, INPUT_PULLUP);
    LOG_INF("TOUCH", "Mofei touch INT GPIO%d", MOFEI_TOUCH_INT);
  }
  if (MOFEI_TOUCH_RST >= 0) {
    pinMode(MOFEI_TOUCH_RST, OUTPUT);
    digitalWrite(MOFEI_TOUCH_RST, LOW);
    delay(5);
    digitalWrite(MOFEI_TOUCH_RST, HIGH);
    delay(60);
  }

  ready = detectOnPins(MOFEI_TOUCH_SDA, MOFEI_TOUCH_SCL);
  if (ready && !configureController()) {
    LOG_ERR("TOUCH", "Mofei FT6336U config failed on SDA=%d SCL=%d addr=0x%02X", activeSda, activeScl, FT6336_ADDR);
  }
#if MOFEI_TOUCH_AUTOSCAN
  if (!ready) {
    ready = autoDetectPins();
    if (ready && !configureController()) {
      LOG_ERR("TOUCH", "Mofei FT6336U config failed on SDA=%d SCL=%d addr=0x%02X", activeSda, activeScl, FT6336_ADDR);
    }
  }
#endif

  LOG_INF("TOUCH", "Mofei FT6336U %s on SDA=%d SCL=%d addr=0x%02X", ready ? "detected" : "not detected", activeSda,
          activeScl, FT6336_ADDR);
  lastStatusLogMs = millis();
#else
  ready = false;
#endif
}

bool MofeiTouchDriver::update(Event* outEvent) {
  if (outEvent != nullptr) {
    *outEvent = Event{};
  }
  const unsigned long now = millis();
  logStatus(now);

  // Late-init retry: FT6336U may not be I2C-ready at boot time.
  // Retry detectOnPins every 5s after the first 8s until detected.
  if (!ready && now > 8000 && (now - lastRetryMs) > 5000) {
    lastRetryMs = now;
    ready = detectOnPins(MOFEI_TOUCH_SDA, MOFEI_TOUCH_SCL);
    if (ready) {
      if (!configureController()) {
        LOG_ERR("TOUCH", "FT6336U late-init config failed");
      }
      LOG_INF("TOUCH", "FT6336U late-init detected SDA=%d SCL=%d", activeSda, activeScl);
    }
  }

  if (!ready) {
    return false;
  }

  uint16_t x = 0;
  uint16_t y = 0;
  bool released = false;

  // Only read from I2C if the hardware INT pin is active (LOW) indicating touch,
  // or if we are already tracking an active touch and waiting for release.
  const bool intActive = MOFEI_TOUCH_INT >= 0 && digitalRead(MOFEI_TOUCH_INT) == LOW;
  if (!intActive && !touchDown) {
    return false;
  }

  if (!readPoint(&x, &y, &released)) {
    if (now - lastReadErrorLogMs >= TOUCH_READ_ERROR_LOG_INTERVAL_MS) {
      LOG_DBG("TOUCH", "FT6336U read failed on SDA=%d SCL=%d addr=0x%02X", activeSda, activeScl, FT6336_ADDR);
      lastReadErrorLogMs = now;
    }
    if (touchDown && now - startMs > TOUCH_TIMEOUT_MS) {
      const Event event = finishTouch();
      if (outEvent != nullptr) {
        *outEvent = event;
      }
      return event.type != EventType::None;
    }
    return false;
  }

  if (released) {
    if (!touchDown) {
      return false;
    }
    lastX = x;
    lastY = y;
    const Event event = finishTouch();
    if (outEvent != nullptr) {
      *outEvent = event;
    }
    return event.type != EventType::None;
  }

  if (!touchDown) {
    touchDown = true;
    startX = x;
    startY = y;
    startMs = millis();
  }
  lastX = x;
  lastY = y;
  return false;
}

bool MofeiTouchDriver::readPoint(uint16_t* x, uint16_t* y, bool* released) {
  uint8_t data[5] = {};
  if (!readFt6336(FT6336_REG_TD_STATUS, data, sizeof(data))) {
    return false;
  }

  const uint8_t points = data[0] & 0x0F;
  if (points == 0) {
    *released = true;
    *x = lastX;
    *y = lastY;
    return true;
  }
  if (points > FT6336_MAX_POINTS) {
    return false;
  }

  const uint8_t eventCode = data[1] >> 6;
  uint16_t rawX = ((static_cast<uint16_t>(data[1] & 0x0F)) << 8) | data[2];
  uint16_t rawY = ((static_cast<uint16_t>(data[3] & 0x0F)) << 8) | data[4];
  normalizePoint(&rawX, &rawY);

  *x = rawX;
  *y = rawY;
  *released = eventCode == 1;
  return true;
}

bool MofeiTouchDriver::detectOnPins(int sda, int scl) {
  if (sda < 0 || scl < 0 || sda == scl) {
    return false;
  }

  activeSda = sda;
  activeScl = scl;
  Wire.end();
  if (!Wire.begin(sda, scl, MOFEI_TOUCH_I2C_FREQ)) {
    return false;
  }
  Wire.setTimeOut(4);

  // A successful I2C read is sufficient to confirm the device is present.
  // Don't check the status value here — FT6336U may return unexpected values
  // during startup before its firmware is fully loaded.
  uint8_t status = 0;
  if (!readFt6336(FT6336_REG_TD_STATUS, &status, 1)) {
    Wire.end();
    return false;
  }

  return true;
}

bool MofeiTouchDriver::autoDetectPins() {
#if MOFEI_TOUCH_AUTOSCAN
  LOG_INF("TOUCH", "FT6336U autoscan started addr=0x%02X", FT6336_ADDR);
  for (int sda : TOUCH_AUTO_PIN_CANDIDATES) {
    for (int scl : TOUCH_AUTO_PIN_CANDIDATES) {
      if (sda == MOFEI_TOUCH_SDA && scl == MOFEI_TOUCH_SCL) {
        continue;
      }
      if (detectOnPins(sda, scl)) {
        LOG_INF("TOUCH", "FT6336U autoscan matched SDA=%d SCL=%d", activeSda, activeScl);
        return true;
      }
    }
  }
  return false;
#else
  return false;
#endif
}

bool MofeiTouchDriver::configureController() {
  const bool okMode = writeFt6336(FT6336_REG_DEVICE_MODE, FT6336_DEVICE_MODE_WORKING);
  const bool okThreshold = writeFt6336(FT6336_REG_THGROUP, FT6336_THGROUP_DEFAULT);
  const bool okPeriod = writeFt6336(FT6336_REG_PERIODACTIVE, FT6336_PERIODACTIVE_DEFAULT);

  uint8_t firmwareId = 0;
  const bool okFirmware = readFt6336(FT6336_REG_FIRMWARE_ID, &firmwareId, 1);
  uint8_t threshold = 0;
  const bool okThresholdRead = readFt6336(FT6336_REG_THGROUP, &threshold, 1);
  uint8_t period = 0;
  const bool okPeriodRead = readFt6336(FT6336_REG_PERIODACTIVE, &period, 1);

  LOG_INF("TOUCH", "FT6336U init mode=%d th=%u/%u period=%u/%u fw=%s0x%02X", okMode ? 0 : -1,
          okThresholdRead ? threshold : 0, FT6336_THGROUP_DEFAULT, okPeriodRead ? period : 0,
          FT6336_PERIODACTIVE_DEFAULT, okFirmware ? "" : "?", firmwareId);

  return okMode && okThreshold && okPeriod;
}

MofeiTouchDriver::Event MofeiTouchDriver::finishTouch() {
  const int dx = static_cast<int>(lastX) - static_cast<int>(startX);
  const int dy = static_cast<int>(lastY) - static_cast<int>(startY);
  const int absDx = abs(dx);
  const int absDy = abs(dy);
  Event event;
  event.x = lastX;
  event.y = lastY;

  if (absDx >= SWIPE_THRESHOLD_PX && absDx > absDy) {
    event.type = dx < 0 ? EventType::SwipeLeft : EventType::SwipeRight;
  } else if (absDy >= SWIPE_THRESHOLD_PX && absDy > absDx) {
    event.type = dy < 0 ? EventType::SwipeUp : EventType::SwipeDown;
  } else if (absDx <= TAP_MAX_MOVE_PX && absDy <= TAP_MAX_MOVE_PX) {
    event.type = EventType::Tap;
  }

  touchDown = false;
  return event;
}

void MofeiTouchDriver::normalizePoint(uint16_t* x, uint16_t* y) const {
  uint16_t tx = *x;
  uint16_t ty = *y;

#if MOFEI_TOUCH_SWAP_XY
  const uint16_t tmp = tx;
  tx = ty;
  ty = tmp;
#endif

#if MOFEI_TOUCH_INVERT_X
  tx = tx >= MOFEI_TOUCH_WIDTH ? 0 : (MOFEI_TOUCH_WIDTH - 1 - tx);
#endif
#if MOFEI_TOUCH_INVERT_Y
  ty = ty >= MOFEI_TOUCH_HEIGHT ? 0 : (MOFEI_TOUCH_HEIGHT - 1 - ty);
#endif

  *x = scaleAxis(tx, MOFEI_TOUCH_WIDTH, MOFEI_TOUCH_LOGICAL_WIDTH);
  *y = scaleAxis(ty, MOFEI_TOUCH_HEIGHT, MOFEI_TOUCH_LOGICAL_HEIGHT);
}

void MofeiTouchDriver::logStatus(unsigned long now) {
  if (now - lastStatusLogMs < TOUCH_STATUS_LOG_INTERVAL_MS) {
    return;
  }
  LOG_INF("TOUCH", "FT6336U status=%s SDA=%d SCL=%d addr=0x%02X", ready ? "ready" : "not detected", activeSda,
          activeScl, FT6336_ADDR);
  lastStatusLogMs = now;
}

#endif
