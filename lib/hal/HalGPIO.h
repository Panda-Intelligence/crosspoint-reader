#pragma once

#include <Arduino.h>
#if !MOFEI_DEVICE
#include <InputManager.h>
#endif

#if MOFEI_DEVICE
#include "MofeiTouch.h"
#endif

// Display SPI pins.
#if MOFEI_DEVICE
#define EPD_SCLK 4   // EPD CLK
#define EPD_MOSI 3   // EPD DIN
#define EPD_CS 5     // EPD CS
#define EPD_DC 6     // EPD DC
#define EPD_RST 7    // EPD RST
#define EPD_BUSY 8   // EPD BUSY
#define SPI_MISO -1  // Mofei EPD bus has no MISO line
#define BAT_GPIO0 9  // Mofei BAT_ADC
#else
#define EPD_SCLK 8   // SPI Clock
#define EPD_MOSI 10  // SPI MOSI (Master Out Slave In)
#define EPD_CS 21    // Chip Select
#define EPD_DC 4     // Data/Command
#define EPD_RST 5    // Reset
#define EPD_BUSY 6   // Busy
#define SPI_MISO 7   // SPI MISO, shared between SD card and display (Master In Slave Out)
#define BAT_GPIO0 0  // Battery voltage
#endif

#define UART0_RXD 20  // Used for USB connection detection

// Mofei ESP32-S3 discrete keys from the main MCU schematic.
#define MOFEI_KEY_LOCK 0
#define MOFEI_KEY1 1
#define MOFEI_KEY2 2

// Xteink X3 Hardware
#define X3_I2C_SDA 20
#define X3_I2C_SCL 0
#define X3_I2C_FREQ 400000

// TI BQ27220 Fuel gauge I2C
#define I2C_ADDR_BQ27220 0x55  // Fuel gauge I2C address
#define BQ27220_SOC_REG 0x2C   // StateOfCharge() command code (%)
#define BQ27220_CUR_REG 0x0C   // Current() command code (signed mA)
#define BQ27220_VOLT_REG 0x08  // Voltage() command code (mV)

// Analog DS3231 RTC I2C
#define I2C_ADDR_DS3231 0x68  // RTC I2C address
#define DS3231_SEC_REG 0x00   // Seconds command code (BCD)

// QST QMI8658 IMU I2C
#define I2C_ADDR_QMI8658 0x6B        // IMU I2C address
#define I2C_ADDR_QMI8658_ALT 0x6A    // IMU I2C fallback address
#define QMI8658_WHO_AM_I_REG 0x00    // WHO_AM_I command code
#define QMI8658_WHO_AM_I_VALUE 0x05  // WHO_AM_I expected value

class HalGPIO {
#if CROSSPOINT_EMULATED == 0 && !MOFEI_DEVICE
  InputManager inputMgr;
#endif

  bool lastUsbConnected = false;
  bool usbStateChanged = false;

 public:
  enum class DeviceType : uint8_t { X4, X3, MOFEI };

 private:
  DeviceType _deviceType = DeviceType::X4;
  uint8_t mofeiCurrentState = 0;
  uint8_t mofeiLastState = 0;
  uint8_t mofeiPressedEvents = 0;
  uint8_t mofeiReleasedEvents = 0;
  unsigned long mofeiLastDebounceTime = 0;
  unsigned long mofeiButtonPressStart = 0;
  unsigned long mofeiButtonPressFinish = 0;
#if MOFEI_DEVICE
  MofeiTouchDriver mofeiTouch;
  MofeiTouchDriver::Event mofeiTouchEvent;
  bool mofeiTouchEventPending = false;
#endif

  uint8_t readMofeiButtonState() const;
  void updateMofeiButtons();
#if MOFEI_DEVICE
  void updateMofeiTouch();
  void injectMofeiButtonEvent(uint8_t buttonIndex);
  bool mapMofeiButtonHintTapToButton(uint16_t x, uint16_t y, uint8_t* buttonIndex) const;
  bool mapMofeiTouchToButton(const MofeiTouchDriver::Event& event, uint8_t* buttonIndex) const;
#endif

 public:
  HalGPIO() = default;

  // Inline device type helpers for cleaner downstream checks
  inline bool deviceIsX3() const { return _deviceType == DeviceType::X3; }
  inline bool deviceIsX4() const { return _deviceType == DeviceType::X4; }
  inline bool deviceIsMofei() const { return _deviceType == DeviceType::MOFEI; }

  // Start button GPIO and setup SPI for screen and SD card
  void begin();

  // Button input methods
  void update();
  bool isPressed(uint8_t buttonIndex) const;
  bool wasPressed(uint8_t buttonIndex) const;
  bool wasAnyPressed() const;
  bool wasReleased(uint8_t buttonIndex) const;
  bool wasAnyReleased() const;
  unsigned long getHeldTime() const;
#if MOFEI_DEVICE
  bool consumeMofeiTouchEvent(MofeiTouchDriver::Event* outEvent);
  bool isMofeiTouchButtonHintTap(uint16_t x, uint16_t y) const;
#endif

  // Setup wake up GPIO and enter deep sleep
  void startDeepSleep();

  // Verify power button was held long enough after wakeup.
  // If verification fails, enters deep sleep and does not return.
  // Should only be called when wakeup reason is PowerButton.
  void verifyPowerButtonWakeup(uint16_t requiredDurationMs, bool shortPressAllowed);

  // Check if USB is connected
  bool isUsbConnected() const;

  // Returns true once per edge (plug or unplug) since the last update()
  bool wasUsbStateChanged() const;

  enum class WakeupReason { PowerButton, AfterFlash, AfterUSBPower, Other };

  WakeupReason getWakeupReason() const;

  // Button indices
  static constexpr uint8_t BTN_BACK = 0;
  static constexpr uint8_t BTN_CONFIRM = 1;
  static constexpr uint8_t BTN_LEFT = 2;
  static constexpr uint8_t BTN_RIGHT = 3;
  static constexpr uint8_t BTN_UP = 4;
  static constexpr uint8_t BTN_DOWN = 5;
  static constexpr uint8_t BTN_POWER = 6;
};

extern HalGPIO gpio;
