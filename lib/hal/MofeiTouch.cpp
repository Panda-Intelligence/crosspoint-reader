#include "MofeiTouch.h"

#if MOFEI_DEVICE

#include <Logging.h>
#include <Wire.h>

namespace {
constexpr uint8_t FT6336_ADDR = MOFEI_TOUCH_ADDR;
constexpr uint8_t FT6336_REG_DEVICE_MODE = 0x00;
constexpr uint8_t FT6336_REG_TD_STATUS = 0x02;
constexpr uint8_t FT6336_REG_GESTURE_ID = 0x01;
constexpr uint8_t FT6336_REG_THGROUP = 0x80;
constexpr uint8_t FT6336_REG_PERIODACTIVE = 0x88;
constexpr uint8_t FT6336_REG_FIRMWARE_ID = 0xA6;
constexpr uint8_t FT6336_REG_VENDOR_ID = 0xA8;
constexpr uint8_t FT6336_MAX_POINTS = 2;
constexpr uint8_t FT6336_FRAME_LEN = 11;
constexpr uint8_t FT6336_TOUCH1_OFFSET = 1;
constexpr uint8_t FT6336_TOUCH2_OFFSET = 7;
constexpr uint8_t FT6336_DEVICE_MODE_WORKING = 0;
constexpr uint8_t FT6336_THGROUP_DEFAULT = 22;
constexpr uint8_t FT6336_PERIODACTIVE_DEFAULT = 14;
constexpr uint8_t FT6336_EVENT_PUT_UP = 1;
constexpr uint8_t FT6336_EVENT_RESERVED = 3;
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
constexpr unsigned long TOUCH_RESET_LOW_MS = 50;
constexpr unsigned long TOUCH_RESET_SETTLE_MS = 100;
constexpr unsigned long TOUCH_BUS_IDLE_MS = 10;

enum class Ft6336FrameStatus : uint8_t { TransportError, Valid, Invalid };

class Ft6336Transport {
 public:
  virtual bool begin(int sda, int scl) = 0;
  virtual void end() = 0;
  virtual bool read(uint8_t reg, uint8_t* buffer, uint8_t len) = 0;
  virtual bool write(uint8_t reg, uint8_t value) = 0;
};

class WireFt6336Transport final : public Ft6336Transport {
 public:
  bool begin(int sda, int scl) override {
    Wire.end();
    if (!Wire.begin(sda, scl, MOFEI_TOUCH_I2C_FREQ)) {
      return false;
    }
    Wire.setTimeOut(4);
    return true;
  }

  void end() override { Wire.end(); }

  bool read(uint8_t reg, uint8_t* buffer, uint8_t len) override {
    Wire.beginTransmission(FT6336_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
      return false;
    }

    if (Wire.requestFrom(FT6336_ADDR, len, static_cast<uint8_t>(true)) != len) {
      while (Wire.available()) {
        Wire.read();
      }
      Wire.beginTransmission(FT6336_ADDR);
      Wire.endTransmission(true);
      return false;
    }

    for (uint8_t i = 0; i < len; ++i) {
      buffer[i] = Wire.read();
    }

    // Force a STOP condition just in case the core failed to send it.
    Wire.beginTransmission(FT6336_ADDR);
    Wire.endTransmission(true);
    return true;
  }

  bool write(uint8_t reg, uint8_t value) override {
    Wire.beginTransmission(FT6336_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission(true) == 0;
  }
};

#if MOFEI_TOUCH_SOFT_I2C
class SoftFt6336Transport final : public Ft6336Transport {
 public:
  bool begin(int sda, int scl) override {
    if (sda < 0 || scl < 0 || sda == scl) {
      return false;
    }
    sdaPin = sda;
    sclPin = scl;
    driveSdaHigh();
    delay(TOUCH_BUS_IDLE_MS);
    driveSclHigh();
    delay(TOUCH_BUS_IDLE_MS);
    return true;
  }

  void end() override {
    if (sdaPin >= 0 && sclPin >= 0) {
      driveSclLow();
      driveSdaLow();
      delayMicroseconds(10);
      driveSclHigh();
      driveSdaHigh();
    }
  }

  bool read(uint8_t reg, uint8_t* buffer, uint8_t len) override {
    if (buffer == nullptr || len == 0) {
      return false;
    }

    start();
    bool ok = writeByte(FT6336_ADDR << 1) && writeByte(reg);
    stop();
    if (!ok) {
      return false;
    }

    start();
    ok = writeByte((FT6336_ADDR << 1) | 0x01);
    if (!ok) {
      stop();
      return false;
    }
    for (uint8_t i = 0; i < len; ++i) {
      buffer[i] = readByte(i + 1 < len);
    }
    stop();
    return true;
  }

  bool write(uint8_t reg, uint8_t value) override {
    start();
    const bool ok = writeByte(FT6336_ADDR << 1) && writeByte(reg) && writeByte(value);
    stop();
    delay(2);
    return ok;
  }

 private:
  int sdaPin = -1;
  int sclPin = -1;

  void driveSdaHigh() const { pinMode(sdaPin, INPUT_PULLUP); }
  void driveSdaLow() const {
    digitalWrite(sdaPin, LOW);
    pinMode(sdaPin, OUTPUT);
  }
  void driveSclHigh() const { pinMode(sclPin, INPUT_PULLUP); }
  void driveSclLow() const {
    digitalWrite(sclPin, LOW);
    pinMode(sclPin, OUTPUT);
  }
  bool readSda() const { return digitalRead(sdaPin) != LOW; }

  void start() const {
    driveSdaHigh();
    driveSclHigh();
    delayMicroseconds(10);
    driveSdaLow();
    delayMicroseconds(10);
    driveSclLow();
  }

  void stop() const {
    driveSclLow();
    driveSdaLow();
    delayMicroseconds(10);
    driveSclHigh();
    driveSdaHigh();
    delayMicroseconds(10);
  }

  bool writeByte(uint8_t value) const {
    driveSclLow();
    for (uint8_t i = 0; i < 8; ++i) {
      if ((value & 0x80) != 0) {
        driveSdaHigh();
      } else {
        driveSdaLow();
      }
      value <<= 1;
      delayMicroseconds(10);
      driveSclHigh();
      delayMicroseconds(10);
      driveSclLow();
      delayMicroseconds(10);
    }

    driveSdaHigh();
    delayMicroseconds(10);
    driveSclHigh();
    delayMicroseconds(10);
    bool acked = false;
    for (uint8_t i = 0; i < 100; ++i) {
      if (!readSda()) {
        acked = true;
        break;
      }
      delayMicroseconds(1);
    }
    driveSclLow();
    delayMicroseconds(10);
    return acked;
  }

  uint8_t readByte(bool keepReading) const {
    uint8_t value = 0;
    driveSdaHigh();
    for (uint8_t i = 0; i < 8; ++i) {
      driveSclLow();
      delayMicroseconds(10);
      driveSclHigh();
      value <<= 1;
      if (readSda()) {
        value |= 0x01;
      }
      delayMicroseconds(10);
    }

    driveSclLow();
    if (keepReading) {
      driveSdaLow();
    } else {
      driveSdaHigh();
    }
    delayMicroseconds(10);
    driveSclHigh();
    delayMicroseconds(10);
    driveSclLow();
    driveSdaHigh();
    return value;
  }
};
#endif

WireFt6336Transport wireTransport;
#if MOFEI_TOUCH_SOFT_I2C
SoftFt6336Transport softTransport;
#endif

Ft6336Transport& activeTransport(bool softwareI2c) {
#if MOFEI_TOUCH_SOFT_I2C
  if (softwareI2c) {
    return softTransport;
  }
#else
  (void)softwareI2c;
#endif
  return wireTransport;
}

const char* transportName(bool softwareI2c) {
#if MOFEI_TOUCH_SOFT_I2C
  if (softwareI2c) {
    return "soft-i2c";
  }
#else
  (void)softwareI2c;
#endif
  return "wire";
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

bool isAht20LikeFrame(const uint8_t* data, uint8_t len) {
  if (data == nullptr || len < 5) {
    return false;
  }

  // 已观测到错误设备返回 98 38 80 76 41 这类 AHT20 风格数据帧，而不是 FT6336U 帧。
  const bool statusLooksAht20 = (data[0] & 0x18) == 0x18 || (data[0] & 0x80) != 0;
  return statusLooksAht20 && ((data[0] & 0xF0) != 0 || (data[0] & 0x0F) > FT6336_MAX_POINTS);
}

bool isPointWithinRawGeometry(uint16_t rawX, uint16_t rawY) {
  return rawX < MOFEI_TOUCH_WIDTH && rawY < MOFEI_TOUCH_HEIGHT;
}

bool validateFt6336Point(const uint8_t* point, uint8_t pointIndex, const char* context) {
  const uint8_t eventCode = point[0] >> 6;
  const uint16_t rawX = ((static_cast<uint16_t>(point[0] & 0x0F)) << 8) | point[1];
  const uint16_t rawY = ((static_cast<uint16_t>(point[2] & 0x0F)) << 8) | point[3];
  if (eventCode == FT6336_EVENT_RESERVED || !isPointWithinRawGeometry(rawX, rawY)) {
    LOG_ERR("TOUCH", "%s rejected invalid FT6336U p%u addr=0x%02X event=%u raw=%u,%u bytes: %02X %02X %02X %02X",
            context, pointIndex, FT6336_ADDR, eventCode, rawX, rawY, point[0], point[1], point[2], point[3]);
    return false;
  }
  return true;
}

Ft6336FrameStatus validateFt6336Frame(const uint8_t* data, uint8_t len, const char* context) {
  if (data == nullptr || len < 5) {
    return Ft6336FrameStatus::TransportError;
  }

  const uint8_t points = data[0] & 0x0F;
  if ((data[0] & 0xF0) != 0 || points > FT6336_MAX_POINTS) {
    if (isAht20LikeFrame(data, len)) {
      LOG_ERR("TOUCH", "%s rejected wrong-device/AHT20-like frame addr=0x%02X: %02X %02X %02X %02X %02X", context,
              FT6336_ADDR, data[0], data[1], data[2], data[3], data[4]);
    } else {
      LOG_ERR("TOUCH",
              "%s rejected invalid FT6336U status=0x%02X points=%u addr=0x%02X frame: %02X %02X %02X %02X %02X",
              context, data[0], points, FT6336_ADDR, data[0], data[1], data[2], data[3], data[4]);
    }
    return Ft6336FrameStatus::Invalid;
  }

  if (points == 0) {
    return Ft6336FrameStatus::Valid;
  }

  if (!validateFt6336Point(&data[FT6336_TOUCH1_OFFSET], 1, context)) {
    return Ft6336FrameStatus::Invalid;
  }
  if (points == 2 &&
      (len <= FT6336_TOUCH2_OFFSET + 3 || !validateFt6336Point(&data[FT6336_TOUCH2_OFFSET], 2, context))) {
    return Ft6336FrameStatus::Invalid;
  }

  return Ft6336FrameStatus::Valid;
}

void resetControllerIfConfigured() {
  if (MOFEI_TOUCH_RST >= 0) {
    pinMode(MOFEI_TOUCH_RST, OUTPUT);
    digitalWrite(MOFEI_TOUCH_RST, LOW);
    delay(TOUCH_RESET_LOW_MS);
    digitalWrite(MOFEI_TOUCH_RST, HIGH);
    delay(TOUCH_RESET_SETTLE_MS);
  }
}

#if MOFEI_TOUCH_SCAN
// GPIO helpers matching SoftFt6336Transport timing
static void scanSdaHigh(int sda) { pinMode(sda, INPUT_PULLUP); }
static void scanSdaLow(int sda) { digitalWrite(sda, LOW); pinMode(sda, OUTPUT); }
static void scanSclHigh(int scl) { pinMode(scl, INPUT_PULLUP); }
static void scanSclLow(int scl) { digitalWrite(scl, LOW); pinMode(scl, OUTPUT); }
static bool scanReadSda(int sda) { return digitalRead(sda) != LOW; }

static void scanStart(int sda, int scl) {
  scanSdaHigh(sda);
  scanSclHigh(scl);
  delayMicroseconds(10);
  scanSdaLow(sda);
  delayMicroseconds(10);
  scanSclLow(scl);
}

static void scanStop(int sda, int scl) {
  scanSclLow(scl);
  scanSdaLow(sda);
  delayMicroseconds(10);
  scanSclHigh(scl);
  scanSdaHigh(sda);
  delayMicroseconds(10);
}

static bool probeAddr(int sda, int scl, uint8_t addr7bit) {
  const uint8_t txByte = addr7bit << 1;
  scanStart(sda, scl);

  // writeByte (exact timing match with SoftFt6336Transport)
  uint8_t val = txByte;
  scanSclLow(scl);
  for (uint8_t i = 0; i < 8; ++i) {
    if ((val & 0x80) != 0) {
      scanSdaHigh(sda);
    } else {
      scanSdaLow(sda);
    }
    val <<= 1;
    delayMicroseconds(10);
    scanSclHigh(scl);
    delayMicroseconds(10);
    scanSclLow(scl);
    delayMicroseconds(10);
  }

  // Check ACK (same as writeByte)
  scanSdaHigh(sda);
  delayMicroseconds(10);
  scanSclHigh(scl);
  delayMicroseconds(10);
  bool acked = false;
  for (uint8_t i = 0; i < 100; ++i) {
    if (!scanReadSda(sda)) {
      acked = true;
      break;
    }
    delayMicroseconds(1);
  }
  scanSclLow(scl);
  delayMicroseconds(10);

  scanStop(sda, scl);
  return acked;
}

void scanGpioPair(int sda, int scl) {
  // Full address scan on the known bus; quick probe on others
  bool foundAny = false;
  for (int addr = 1; addr < 127; ++addr) {
    if (probeAddr(sda, scl, static_cast<uint8_t>(addr))) {
      LOG_INF("SCAN", "  FOUND device at 0x%02X on SDA=%d SCL=%d", addr, sda, scl);
      foundAny = true;
    }
  }
  if (!foundAny) {
    LOG_INF("SCAN", "  No devices found on SDA=%d SCL=%d", sda, scl);
  }
}

void quickProbePair(int sda, int scl) {
  static constexpr uint8_t checkAddrs[] = {0x2E, 0x38};
  for (uint8_t addr : checkAddrs) {
    if (probeAddr(sda, scl, addr)) {
      LOG_INF("SCAN", "  FOUND device at 0x%02X on SDA=%d SCL=%d", addr, sda, scl);
    }
  }
}

void scanAllPins() {
  delay(6000);
  LOG_INF("SCAN", "=== Focused ic.png Diagnostic (SDA=13 SCL=12) ===");

  int sda = 13;
  int scl = 12;
  int pwr = 45;
  int intr = 44;

  auto probeTouch = [sda, scl](uint8_t addr) -> bool {
    scanStart(sda, scl);
    // writeByte logic (simplified for ACK check)
    uint8_t val = addr << 1;
    scanSclLow(scl);
    for (int i = 0; i < 8; i++) {
      if (val & 0x80) scanSdaHigh(sda); else scanSdaLow(sda);
      val <<= 1; delayMicroseconds(20); scanSclHigh(scl); delayMicroseconds(20); scanSclLow(scl); delayMicroseconds(20);
    }
    scanSdaHigh(sda); delayMicroseconds(20); scanSclHigh(scl); delayMicroseconds(20);
    bool ack = !scanReadSda(sda);
    scanSclLow(scl); scanStop(sda, scl);
    return ack;
  };

  static constexpr int8_t rstPins[] = {-1, 7, 46};
  bool pwrStates[] = {true, false};

  for (bool pwrHigh : pwrStates) {
    LOG_INF("SCAN", "Testing with TOUCH_PWR(GPIO45) = %s", pwrHigh ? "HIGH" : "LOW");
    pinMode(pwr, OUTPUT);
    digitalWrite(pwr, pwrHigh ? HIGH : LOW);
    
    for (int8_t rst : rstPins) {
      if (rst != -1) {
        LOG_INF("SCAN", "  Applying Reset on GPIO%d...", rst);
        pinMode(rst, OUTPUT);
        digitalWrite(rst, LOW); delay(20); digitalWrite(rst, HIGH); delay(100);
      } else {
        LOG_INF("SCAN", "  No explicit reset...");
      }

      if (probeTouch(0x2E)) {
        LOG_INF("SCAN", "  >>> SUCCESS! FOUND 0x2E (FT6336U) on 13/12! <<<");
      } else {
        LOG_INF("SCAN", "  0x2E not found.");
      }
      
      // Also check 0x38 as control (it should always be there if pwr doesn't kill the bus)
      if (probeTouch(0x38)) {
        LOG_INF("SCAN", "  (Control 0x38 is present)");
      }
    }
  }

  LOG_INF("SCAN", "=== Diagnostic complete ===");
}
#endif  // MOFEI_TOUCH_SCAN

}  // namespace

void MofeiTouchDriver::begin() {
#if MOFEI_TOUCH_ENABLE
  ready = false;
  touchDown = false;
  lastReadInvalidFrame = false;
  useSoftwareI2c = MOFEI_TOUCH_SOFT_I2C != 0;
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
  resetControllerIfConfigured();

#if MOFEI_TOUCH_SCAN
  scanAllPins();
#endif

  LOG_INF("TOUCH", "Mofei FT6336U transport=%s SDA=%d SCL=%d INT=%d RST=%d PWR=%d addr=0x%02X",
          transportName(useSoftwareI2c), MOFEI_TOUCH_SDA, MOFEI_TOUCH_SCL, MOFEI_TOUCH_INT, MOFEI_TOUCH_RST,
          MOFEI_TOUCH_PWR, FT6336_ADDR);

  ready = detectOnPins(MOFEI_TOUCH_SDA, MOFEI_TOUCH_SCL);
  if (ready && !configureController()) {
    LOG_ERR("TOUCH", "Mofei FT6336U config failed on SDA=%d SCL=%d addr=0x%02X", activeSda, activeScl, FT6336_ADDR);
    markUnavailable();
  }
#if MOFEI_TOUCH_AUTOSCAN
  if (!ready) {
    ready = autoDetectPins();
    if (ready && !configureController()) {
      LOG_ERR("TOUCH", "Mofei FT6336U config failed on SDA=%d SCL=%d addr=0x%02X", activeSda, activeScl, FT6336_ADDR);
      markUnavailable();
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
        LOG_ERR("TOUCH", "FT6336U late-init config failed addr=0x%02X", FT6336_ADDR);
        markUnavailable();
      } else {
        LOG_INF("TOUCH", "FT6336U late-init detected SDA=%d SCL=%d addr=0x%02X", activeSda, activeScl, FT6336_ADDR);
      }
    }
  }

  if (!ready) {
    return false;
  }

  // Only read from I2C if the hardware INT pin is active (LOW) indicating touch,
  // or if we are already tracking an active touch and waiting for release.
  const bool intActive = MOFEI_TOUCH_INT >= 0 && digitalRead(MOFEI_TOUCH_INT) == LOW;
  if (!intActive && !touchDown) {
    return false;
  }

  uint16_t x = 0;
  uint16_t y = 0;
  bool released = false;

  if (!readPoint(&x, &y, &released)) {
    if (now - lastReadErrorLogMs >= TOUCH_READ_ERROR_LOG_INTERVAL_MS) {
      LOG_DBG("TOUCH", "FT6336U read failed transport=%s SDA=%d SCL=%d addr=0x%02X", transportName(useSoftwareI2c),
              activeSda, activeScl, FT6336_ADDR);
      lastReadErrorLogMs = now;
    }
    if (lastReadInvalidFrame) {
      markUnavailable();
      return false;
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
  uint8_t data[FT6336_FRAME_LEN] = {};
  lastReadInvalidFrame = false;
  const bool readOk = readRegister(FT6336_REG_TD_STATUS, data, sizeof(data));
  const Ft6336FrameStatus frameStatus =
      readOk ? validateFt6336Frame(data, sizeof(data), "read") : Ft6336FrameStatus::TransportError;
  if (frameStatus != Ft6336FrameStatus::Valid) {
    lastReadInvalidFrame = frameStatus == Ft6336FrameStatus::Invalid;
    return false;
  }

  const uint8_t points = data[0] & 0x0F;
  if (points == 0) {
    *released = true;
    *x = lastX;
    *y = lastY;
    return true;
  }

  const uint8_t eventCode = data[1] >> 6;
  uint16_t rawX = ((static_cast<uint16_t>(data[1] & 0x0F)) << 8) | data[2];
  uint16_t rawY = ((static_cast<uint16_t>(data[3] & 0x0F)) << 8) | data[4];
  normalizePoint(&rawX, &rawY);

  *x = rawX;
  *y = rawY;
  *released = eventCode == FT6336_EVENT_PUT_UP;
  return true;
}

bool MofeiTouchDriver::detectOnPins(int sda, int scl) {
  if (sda < 0 || scl < 0 || sda == scl) {
    return false;
  }

  activeSda = sda;
  activeScl = scl;
  if (!activeTransport(useSoftwareI2c).begin(sda, scl)) {
    LOG_DBG("TOUCH", "FT6336U detect transport=%s begin failed SDA=%d SCL=%d addr=0x%02X",
            transportName(useSoftwareI2c), sda, scl, FT6336_ADDR);
    return false;
  }

  uint8_t data[FT6336_FRAME_LEN] = {};
  const bool readOk = readRegister(FT6336_REG_TD_STATUS, data, sizeof(data));
  if (!readOk || validateFt6336Frame(data, sizeof(data), "detect") != Ft6336FrameStatus::Valid) {
    if (!readOk) {
      LOG_DBG("TOUCH", "FT6336U detect transport=%s read failed SDA=%d SCL=%d addr=0x%02X",
              transportName(useSoftwareI2c), activeSda, activeScl, FT6336_ADDR);
      activeTransport(useSoftwareI2c).end();
      return false;
    }
    // 0x38 may be an uninitialized FT6336U returning garbage that mimics AHT20 frames.
    // Let configureController() attempt full init and re-validate after config writes.
    if (FT6336_ADDR == 0x38) {
      LOG_INF("TOUCH", "FT6336U detect: non-valid frame at 0x38 addr, proceeding to init phase");
    } else {
      activeTransport(useSoftwareI2c).end();
      return false;
    }
  }

#if MOFEI_TOUCH_DIAGNOSTIC_LOG
  uint8_t mode = 0xFF;
  uint8_t gesture = 0xFF;
  uint8_t firmwareId = 0xFF;
  uint8_t vendorId = 0xFF;
  const bool modeOk = readRegister(FT6336_REG_DEVICE_MODE, &mode, 1);
  const bool gestureOk = readRegister(FT6336_REG_GESTURE_ID, &gesture, 1);
  const bool firmwareOk = readRegister(FT6336_REG_FIRMWARE_ID, &firmwareId, 1);
  const bool vendorOk = readRegister(FT6336_REG_VENDOR_ID, &vendorId, 1);
  LOG_INF("TOUCH",
          "FT6336U diagnostic transport=%s SDA=%d SCL=%d status=0x%02X mode=%s0x%02X gesture=%s0x%02X "
          "addr=0x%02X fw=%s0x%02X vendor=%s0x%02X frame=%02X %02X %02X %02X %02X %02X",
          transportName(useSoftwareI2c), activeSda, activeScl, data[0], modeOk ? "" : "?", mode, gestureOk ? "" : "?",
          gesture, FT6336_ADDR, firmwareOk ? "" : "?", firmwareId, vendorOk ? "" : "?", vendorId, data[0], data[1],
          data[2], data[3], data[4], data[5]);
#endif

  return true;
}

#if MOFEI_TOUCH_AUTOSCAN
bool MofeiTouchDriver::autoDetectPins() {
  LOG_INF("TOUCH", "FT6336U autoscan started addr=0x%02X", FT6336_ADDR);
  for (int sda : TOUCH_AUTO_PIN_CANDIDATES) {
    for (int scl : TOUCH_AUTO_PIN_CANDIDATES) {
      if (sda == MOFEI_TOUCH_SDA && scl == MOFEI_TOUCH_SCL) {
        continue;
      }
      if (detectOnPins(sda, scl)) {
        LOG_INF("TOUCH", "FT6336U autoscan matched SDA=%d SCL=%d addr=0x%02X", activeSda, activeScl, FT6336_ADDR);
        return true;
      }
    }
  }
  return false;
}
#endif

bool MofeiTouchDriver::readRegister(uint8_t reg, uint8_t* buffer, uint8_t len) {
  return activeTransport(useSoftwareI2c).read(reg, buffer, len);
}

bool MofeiTouchDriver::writeRegister(uint8_t reg, uint8_t value) {
  return activeTransport(useSoftwareI2c).write(reg, value);
}

bool MofeiTouchDriver::configureController() {
  const bool okMode = writeRegister(FT6336_REG_DEVICE_MODE, FT6336_DEVICE_MODE_WORKING);
  const bool okThreshold = writeRegister(FT6336_REG_THGROUP, FT6336_THGROUP_DEFAULT);
  const bool okPeriod = writeRegister(FT6336_REG_PERIODACTIVE, FT6336_PERIODACTIVE_DEFAULT);

  uint8_t mode = 0xFF;
  const bool okModeRead = readRegister(FT6336_REG_DEVICE_MODE, &mode, 1);
  uint8_t firmwareId = 0;
  const bool okFirmware = readRegister(FT6336_REG_FIRMWARE_ID, &firmwareId, 1);
  uint8_t threshold = 0;
  const bool okThresholdRead = readRegister(FT6336_REG_THGROUP, &threshold, 1);
  uint8_t period = 0;
  const bool okPeriodRead = readRegister(FT6336_REG_PERIODACTIVE, &period, 1);
  uint8_t data[FT6336_FRAME_LEN] = {};
  const bool okFrame = readRegister(FT6336_REG_TD_STATUS, data, sizeof(data)) &&
                       validateFt6336Frame(data, sizeof(data), "config") == Ft6336FrameStatus::Valid;

  LOG_INF("TOUCH", "FT6336U init transport=%s addr=0x%02X mode=%d th=%u/%u period=%u/%u fw=%s0x%02X",
          transportName(useSoftwareI2c), FT6336_ADDR, okModeRead ? mode : -1, okThresholdRead ? threshold : 0,
          FT6336_THGROUP_DEFAULT, okPeriodRead ? period : 0, FT6336_PERIODACTIVE_DEFAULT, okFirmware ? "" : "?",
          firmwareId);

  if (!okMode || !okThreshold || !okPeriod || !okModeRead || !okThresholdRead || !okPeriodRead || !okFrame) {
    return false;
  }
  return mode == FT6336_DEVICE_MODE_WORKING && threshold == FT6336_THGROUP_DEFAULT &&
         period == FT6336_PERIODACTIVE_DEFAULT;
}

void MofeiTouchDriver::markUnavailable() {
  ready = false;
  touchDown = false;
  lastReadInvalidFrame = false;
  activeTransport(useSoftwareI2c).end();
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
  LOG_INF("TOUCH", "FT6336U status=%s transport=%s SDA=%d SCL=%d addr=0x%02X", ready ? "ready" : "not detected",
          transportName(useSoftwareI2c), activeSda, activeScl, FT6336_ADDR);
  lastStatusLogMs = now;
}

#endif
