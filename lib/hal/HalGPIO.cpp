#include <HalGPIO.h>
#include <Logging.h>
#include <Preferences.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_sleep.h>

// Global HalGPIO instance
HalGPIO gpio;

namespace X3GPIO {

struct X3ProbeResult {
  bool bq27220 = false;
  bool ds3231 = false;
  bool qmi8658 = false;

  uint8_t score() const {
    return static_cast<uint8_t>(bq27220) + static_cast<uint8_t>(ds3231) + static_cast<uint8_t>(qmi8658);
  }
};

bool readI2CReg8(uint8_t addr, uint8_t reg, uint8_t* outValue) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(addr, static_cast<uint8_t>(1), static_cast<uint8_t>(true)) < 1) {
    return false;
  }
  *outValue = Wire.read();
  return true;
}

bool readI2CReg16LE(uint8_t addr, uint8_t reg, uint16_t* outValue) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(addr, static_cast<uint8_t>(2), static_cast<uint8_t>(true)) < 2) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }
  const uint8_t lo = Wire.read();
  const uint8_t hi = Wire.read();
  *outValue = (static_cast<uint16_t>(hi) << 8) | lo;
  return true;
}

bool readBQ27220CurrentMA(int16_t* outCurrent) {
  uint16_t raw = 0;
  if (!readI2CReg16LE(I2C_ADDR_BQ27220, BQ27220_CUR_REG, &raw)) {
    return false;
  }
  *outCurrent = static_cast<int16_t>(raw);
  return true;
}

bool probeBQ27220Signature() {
  uint16_t soc = 0;
  uint16_t voltageMv = 0;
  if (!readI2CReg16LE(I2C_ADDR_BQ27220, BQ27220_SOC_REG, &soc)) {
    return false;
  }
  if (soc > 100) {
    return false;
  }
  if (!readI2CReg16LE(I2C_ADDR_BQ27220, BQ27220_VOLT_REG, &voltageMv)) {
    return false;
  }
  return voltageMv >= 2500 && voltageMv <= 5000;
}

bool probeDS3231Signature() {
  uint8_t sec = 0;
  if (!readI2CReg8(I2C_ADDR_DS3231, DS3231_SEC_REG, &sec)) {
    return false;
  }
  const uint8_t tensDigit = (sec >> 4) & 0x07;
  const uint8_t onesDigit = sec & 0x0F;

  return tensDigit <= 5 && onesDigit <= 9;
}

bool probeQMI8658Signature() {
  uint8_t whoami = 0;
  if (readI2CReg8(I2C_ADDR_QMI8658, QMI8658_WHO_AM_I_REG, &whoami) && whoami == QMI8658_WHO_AM_I_VALUE) {
    return true;
  }
  if (readI2CReg8(I2C_ADDR_QMI8658_ALT, QMI8658_WHO_AM_I_REG, &whoami) && whoami == QMI8658_WHO_AM_I_VALUE) {
    return true;
  }
  return false;
}

X3ProbeResult runX3ProbePass() {
  X3ProbeResult result;
  Wire.begin(X3_I2C_SDA, X3_I2C_SCL, X3_I2C_FREQ);
  Wire.setTimeOut(6);

  result.bq27220 = probeBQ27220Signature();
  result.ds3231 = probeDS3231Signature();
  result.qmi8658 = probeQMI8658Signature();

  Wire.end();
#if !MOFEI_DEVICE
  pinMode(20, INPUT);
#endif
  pinMode(0, INPUT);
  return result;
}

}  // namespace X3GPIO

namespace {
constexpr char HW_NAMESPACE[] = "cphw";
constexpr char NVS_KEY_DEV_OVERRIDE[] = "dev_ovr";  // 0=auto, 1=x4, 2=x3
constexpr char NVS_KEY_DEV_CACHED[] = "dev_det";    // 0=unknown, 1=x4, 2=x3

enum class NvsDeviceValue : uint8_t { Unknown = 0, X4 = 1, X3 = 2 };

constexpr unsigned long BUTTON_DEBOUNCE_DELAY_MS = 5;
#if MOFEI_DEVICE
constexpr uint16_t MOFEI_TOUCH_LOGICAL_WIDTH_PX = MOFEI_TOUCH_LOGICAL_WIDTH;
constexpr uint16_t MOFEI_TOUCH_LOGICAL_HEIGHT_PX = MOFEI_TOUCH_LOGICAL_HEIGHT;
constexpr uint16_t MOFEI_TOUCH_BOTTOM_ZONE_PX = 110;
constexpr uint16_t MOFEI_TOUCH_EDGE_ZONE_PX = 80;
#endif

NvsDeviceValue readNvsDeviceValue(const char* key, NvsDeviceValue defaultValue) {
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, true)) {
    return defaultValue;
  }
  const uint8_t raw = prefs.getUChar(key, static_cast<uint8_t>(defaultValue));
  prefs.end();
  if (raw > static_cast<uint8_t>(NvsDeviceValue::X3)) {
    return defaultValue;
  }
  return static_cast<NvsDeviceValue>(raw);
}

void writeNvsDeviceValue(const char* key, NvsDeviceValue value) {
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, false)) {
    return;
  }
  prefs.putUChar(key, static_cast<uint8_t>(value));
  prefs.end();
}

HalGPIO::DeviceType nvsToDeviceType(NvsDeviceValue value) {
  return value == NvsDeviceValue::X3 ? HalGPIO::DeviceType::X3 : HalGPIO::DeviceType::X4;
}

HalGPIO::DeviceType detectDeviceTypeWithFingerprint() {
  // Explicit override for recovery/support:
  // 0 = auto, 1 = force X4, 2 = force X3
  const NvsDeviceValue overrideValue = readNvsDeviceValue(NVS_KEY_DEV_OVERRIDE, NvsDeviceValue::Unknown);
  if (overrideValue == NvsDeviceValue::X3 || overrideValue == NvsDeviceValue::X4) {
    LOG_INF("HW", "Device override active: %s", overrideValue == NvsDeviceValue::X3 ? "X3" : "X4");
    return nvsToDeviceType(overrideValue);
  }

  const NvsDeviceValue cachedValue = readNvsDeviceValue(NVS_KEY_DEV_CACHED, NvsDeviceValue::Unknown);
  if (cachedValue == NvsDeviceValue::X3 || cachedValue == NvsDeviceValue::X4) {
    LOG_INF("HW", "Using cached device type: %s", cachedValue == NvsDeviceValue::X3 ? "X3" : "X4");
    return nvsToDeviceType(cachedValue);
  }

  // No cache yet: run active X3 fingerprint probe and persist result.
  const X3GPIO::X3ProbeResult pass1 = X3GPIO::runX3ProbePass();
  delay(2);
  const X3GPIO::X3ProbeResult pass2 = X3GPIO::runX3ProbePass();

  const uint8_t score1 = pass1.score();
  const uint8_t score2 = pass2.score();
  LOG_INF("HW", "X3 probe scores: pass1=%u(bq=%d rtc=%d imu=%d) pass2=%u(bq=%d rtc=%d imu=%d)", score1, pass1.bq27220,
          pass1.ds3231, pass1.qmi8658, score2, pass2.bq27220, pass2.ds3231, pass2.qmi8658);
  const bool x3Confirmed = (score1 >= 2) && (score2 >= 2);
  const bool x4Confirmed = (score1 == 0) && (score2 == 0);

  if (x3Confirmed) {
    writeNvsDeviceValue(NVS_KEY_DEV_CACHED, NvsDeviceValue::X3);
    return HalGPIO::DeviceType::X3;
  }

  if (x4Confirmed) {
    writeNvsDeviceValue(NVS_KEY_DEV_CACHED, NvsDeviceValue::X4);
    return HalGPIO::DeviceType::X4;
  }

  // Conservative fallback for first boot with inconclusive probes.
  return HalGPIO::DeviceType::X4;
}

}  // namespace

uint8_t HalGPIO::readMofeiButtonState() const {
  uint8_t state = 0;
  if (digitalRead(MOFEI_KEY_LOCK) == LOW) {
    state |= (1 << BTN_BACK);
  }
  if (digitalRead(MOFEI_KEY1) == LOW) {
    state |= (1 << BTN_CONFIRM);
  }
  if (digitalRead(MOFEI_KEY2) == LOW) {
    state |= (1 << BTN_DOWN);
  }
  return state;
}

void HalGPIO::updateMofeiButtons() {
  const unsigned long currentTime = millis();
  const uint8_t state = readMofeiButtonState();

  mofeiPressedEvents = 0;
  mofeiReleasedEvents = 0;

  if (state != mofeiLastState) {
    mofeiLastDebounceTime = currentTime;
    mofeiLastState = state;
  }

  if ((currentTime - mofeiLastDebounceTime) > BUTTON_DEBOUNCE_DELAY_MS && state != mofeiCurrentState) {
    mofeiPressedEvents = state & ~mofeiCurrentState;
    mofeiReleasedEvents = mofeiCurrentState & ~state;

    if (mofeiPressedEvents > 0 && mofeiCurrentState == 0) {
      mofeiButtonPressStart = currentTime;
    }
    if (mofeiReleasedEvents > 0 && state == 0) {
      mofeiButtonPressFinish = currentTime;
    }

    mofeiCurrentState = state;
  }
}

#if MOFEI_DEVICE
void HalGPIO::injectMofeiButtonEvent(uint8_t buttonIndex) {
  if (buttonIndex > BTN_POWER) {
    return;
  }
  const uint8_t bit = 1 << buttonIndex;
  mofeiPressedEvents |= bit;
  mofeiReleasedEvents |= bit;
  mofeiButtonPressStart = millis();
  mofeiButtonPressFinish = mofeiButtonPressStart;
}

void HalGPIO::updateMofeiTouch() {
  MofeiTouchDriver::Event event;
  if (!mofeiTouch.update(&event)) {
    return;
  }

  uint8_t button = BTN_CONFIRM;
  switch (event.type) {
    case MofeiTouchDriver::EventType::SwipeLeft:
      button = BTN_RIGHT;
      break;
    case MofeiTouchDriver::EventType::SwipeRight:
      button = BTN_LEFT;
      break;
    case MofeiTouchDriver::EventType::SwipeUp:
      button = BTN_DOWN;
      break;
    case MofeiTouchDriver::EventType::SwipeDown:
      button = BTN_UP;
      break;
    case MofeiTouchDriver::EventType::Tap:
      if (event.y >= MOFEI_TOUCH_LOGICAL_HEIGHT_PX - MOFEI_TOUCH_BOTTOM_ZONE_PX) {
        const uint16_t segment = (static_cast<uint32_t>(event.x) * 4) / MOFEI_TOUCH_LOGICAL_WIDTH_PX;
        button = segment == 0 ? BTN_BACK : (segment == 1 ? BTN_CONFIRM : (segment == 2 ? BTN_LEFT : BTN_RIGHT));
      } else if (event.x <= MOFEI_TOUCH_EDGE_ZONE_PX) {
        button = BTN_BACK;
      } else if (event.x < MOFEI_TOUCH_LOGICAL_WIDTH_PX / 3) {
        button = BTN_LEFT;
      } else if (event.x >= (MOFEI_TOUCH_LOGICAL_WIDTH_PX * 2) / 3) {
        button = BTN_RIGHT;
      } else {
        button = BTN_CONFIRM;
      }
      break;
    case MofeiTouchDriver::EventType::None:
    default:
      return;
  }

  injectMofeiButtonEvent(button);
  LOG_DBG("TOUCH", "event=%u x=%u y=%u button=%u", static_cast<unsigned>(event.type), event.x, event.y, button);
}
#endif

void HalGPIO::begin() {
#if MOFEI_DEVICE
  _deviceType = DeviceType::MOFEI;
  pinMode(MOFEI_KEY_LOCK, INPUT_PULLUP);
  pinMode(MOFEI_KEY1, INPUT_PULLUP);
  pinMode(MOFEI_KEY2, INPUT_PULLUP);
  pinMode(BAT_GPIO0, INPUT);
  SPI.begin(EPD_SCLK, SPI_MISO, EPD_MOSI, EPD_CS);
  mofeiTouch.begin();
#else
  inputMgr.begin();
  SPI.begin(EPD_SCLK, SPI_MISO, EPD_MOSI, EPD_CS);

  _deviceType = detectDeviceTypeWithFingerprint();

  if (deviceIsX4()) {
    pinMode(BAT_GPIO0, INPUT);
    pinMode(UART0_RXD, INPUT);
  }
#endif

  lastUsbConnected = isUsbConnected();
}

void HalGPIO::update() {
#if MOFEI_DEVICE
  updateMofeiButtons();
  updateMofeiTouch();
#else
  inputMgr.update();
#endif
  const bool connected = isUsbConnected();
  usbStateChanged = (connected != lastUsbConnected);
  lastUsbConnected = connected;
}

bool HalGPIO::wasUsbStateChanged() const { return usbStateChanged; }

bool HalGPIO::isPressed(uint8_t buttonIndex) const {
#if MOFEI_DEVICE
  return mofeiCurrentState & (1 << buttonIndex);
#else
  return inputMgr.isPressed(buttonIndex);
#endif
}

bool HalGPIO::wasPressed(uint8_t buttonIndex) const {
#if MOFEI_DEVICE
  return mofeiPressedEvents & (1 << buttonIndex);
#else
  return inputMgr.wasPressed(buttonIndex);
#endif
}

bool HalGPIO::wasAnyPressed() const {
#if MOFEI_DEVICE
  return mofeiPressedEvents > 0;
#else
  return inputMgr.wasAnyPressed();
#endif
}

bool HalGPIO::wasReleased(uint8_t buttonIndex) const {
#if MOFEI_DEVICE
  return mofeiReleasedEvents & (1 << buttonIndex);
#else
  return inputMgr.wasReleased(buttonIndex);
#endif
}

bool HalGPIO::wasAnyReleased() const {
#if MOFEI_DEVICE
  return mofeiReleasedEvents > 0;
#else
  return inputMgr.wasAnyReleased();
#endif
}

unsigned long HalGPIO::getHeldTime() const {
#if MOFEI_DEVICE
  if (mofeiCurrentState > 0) {
    return millis() - mofeiButtonPressStart;
  }
  return mofeiButtonPressFinish - mofeiButtonPressStart;
#else
  return inputMgr.getHeldTime();
#endif
}

void HalGPIO::startDeepSleep() {
#if MOFEI_DEVICE
  LOG_INF("PWR", "Mofei deep sleep disabled until PMIC/latch GPIO is mapped");
  return;
#else
  // Ensure that the power button has been released to avoid immediately turning back on if you're holding it
  while (inputMgr.isPressed(BTN_POWER)) {
    delay(50);
    inputMgr.update();
  }
  // Arm the wakeup trigger *after* the button is released
  gpio_wakeup_enable(static_cast<gpio_num_t>(InputManager::POWER_BUTTON_PIN), GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  // Enter Deep Sleep
  esp_deep_sleep_start();
#endif
}

void HalGPIO::verifyPowerButtonWakeup(uint16_t requiredDurationMs, bool shortPressAllowed) {
#if MOFEI_DEVICE
  (void)requiredDurationMs;
  (void)shortPressAllowed;
  return;
#else
  if (shortPressAllowed) {
    // Fast path - no duration check needed
    return;
  }
  // TODO: Intermittent edge case remains: a single tap followed by another single tap
  // can still power on the device. Tighten wake debounce/state handling here.

  // Calibrate: subtract boot time already elapsed, assuming button held since boot
  const uint16_t calibration = millis();
  const uint16_t calibratedDuration = (calibration < requiredDurationMs) ? (requiredDurationMs - calibration) : 1;

  const auto start = millis();
  inputMgr.update();
  // inputMgr.isPressed() may take up to ~500ms to return correct state
  while (!inputMgr.isPressed(BTN_POWER) && millis() - start < 1000) {
    delay(10);
    inputMgr.update();
  }
  if (inputMgr.isPressed(BTN_POWER)) {
    do {
      delay(10);
      inputMgr.update();
    } while (inputMgr.isPressed(BTN_POWER) && inputMgr.getHeldTime() < calibratedDuration);
    if (inputMgr.getHeldTime() < calibratedDuration) {
      startDeepSleep();
    }
  } else {
    startDeepSleep();
  }
#endif
}

bool HalGPIO::isUsbConnected() const {
  if (deviceIsMofei()) {
    return false;
  }
  if (deviceIsX3()) {
    // X3: infer USB/charging via BQ27220 Current() register (0x0C, signed mA).
    // Positive current means charging.
    for (uint8_t attempt = 0; attempt < 2; ++attempt) {
      int16_t currentMa = 0;
      if (X3GPIO::readBQ27220CurrentMA(&currentMa)) {
        return currentMa > 0;
      }
      delay(2);
    }
    return false;
  }
  // U0RXD/GPIO20 reads HIGH when USB is connected
  return digitalRead(UART0_RXD) == HIGH;
}

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
#if MOFEI_DEVICE
  const auto resetReason = esp_reset_reason();
  if (resetReason == ESP_RST_POWERON) {
    return WakeupReason::AfterFlash;
  }
  return WakeupReason::Other;
#else
  const auto wakeupCause = esp_sleep_get_wakeup_cause();
  const auto resetReason = esp_reset_reason();

  const bool usbConnected = isUsbConnected();

  if ((wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && !usbConnected) ||
      (wakeupCause == ESP_SLEEP_WAKEUP_GPIO && resetReason == ESP_RST_DEEPSLEEP && usbConnected)) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_UNKNOWN && usbConnected) {
    return WakeupReason::AfterFlash;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && usbConnected) {
    return WakeupReason::AfterUSBPower;
  }
  return WakeupReason::Other;
#endif
}
