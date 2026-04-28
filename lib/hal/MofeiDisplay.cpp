#include "MofeiDisplay.h"

#if MOFEI_DEVICE

#include <Logging.h>

#include <cstring>

namespace {
constexpr uint8_t CMD_DEEP_SLEEP = 0x10;
constexpr uint8_t CMD_DATA_ENTRY_MODE = 0x11;
constexpr uint8_t CMD_SOFT_RESET = 0x12;
constexpr uint8_t CMD_TEMP_SENSOR_CONTROL = 0x18;
constexpr uint8_t CMD_DISPLAY_UPDATE_CTRL2 = 0x22;
constexpr uint8_t CMD_WRITE_RAM_BW = 0x24;
constexpr uint8_t CMD_WRITE_RAM_RED = 0x26;
constexpr uint8_t CMD_WRITE_LUT = 0x32;
constexpr uint8_t CMD_BORDER_WAVEFORM = 0x3C;
constexpr uint8_t CMD_SET_RAM_X_RANGE = 0x44;
constexpr uint8_t CMD_SET_RAM_Y_RANGE = 0x45;
constexpr uint8_t CMD_SET_RAM_X_COUNTER = 0x4E;
constexpr uint8_t CMD_SET_RAM_Y_COUNTER = 0x4F;
constexpr uint8_t CMD_MASTER_ACTIVATION = 0x20;
constexpr uint8_t CMD_BOOSTER_SOFT_START = 0x0C;
constexpr uint8_t CMD_DRIVER_OUTPUT_CONTROL = 0x01;
constexpr uint8_t CMD_GATE_VOLTAGE = 0x03;
constexpr uint8_t CMD_SOURCE_VOLTAGE = 0x04;
constexpr uint8_t CMD_WRITE_VCOM = 0x2C;
constexpr uint8_t CMD_WRITE_TEMP = 0x1A;
constexpr uint32_t MOFEI_SPI_HZ = 10000000;
constexpr uint32_t BUSY_TIMEOUT_MS = 30000;
constexpr uint8_t MOFEI_LUT_WAVEFORM_BYTES = 105;
constexpr uint8_t MOFEI_LUT_GATE_OFFSET = 105;
constexpr uint8_t MOFEI_LUT_SOURCE_OFFSET = 106;
constexpr uint8_t MOFEI_LUT_VCOM_OFFSET = 109;
constexpr uint8_t MOFEI_LUT_TOTAL_BYTES = 112;

// GDEQ0426T82-T01C_Arduino.ino WS_20_80, used for the panel full waveform.
const uint8_t MOFEI_LUT_FULL[] PROGMEM = {
    0xA0, 0x48, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x48, 0xA8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xA0, 0x48, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x48, 0xA8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A, 0x14, 0x00, 0x00, 0x00, 0x0D, 0x01,
    0x0D, 0x01, 0x02, 0x0A, 0x0A, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x22, 0x22, 0x22, 0x22, 0x22, 0x17, 0x41, 0xA8, 0x32, 0x48, 0x00, 0x00};

// GDEQ0426T82-T01C_Arduino.ino WS_80_127, used by EPD_Update_Fast().
const uint8_t MOFEI_LUT_FAST[] PROGMEM = {
    0xA8, 0x00, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xA8, 0x00, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0D, 0x0B, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0A, 0x0A, 0x05, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x01, 0x22, 0x22, 0x22, 0x22, 0x22, 0x17, 0x41, 0xA8, 0x32, 0x30, 0x00, 0x00};

static_assert(sizeof(MOFEI_LUT_FULL) == MOFEI_LUT_TOTAL_BYTES, "Mofei full LUT must match Good Display waveform table");
static_assert(sizeof(MOFEI_LUT_FAST) == MOFEI_LUT_TOTAL_BYTES, "Mofei fast LUT must match Good Display waveform table");

uint8_t progmemRead(const uint8_t* data, uint32_t index, bool fromProgmem) {
  return fromProgmem ? pgm_read_byte(data + index) : data[index];
}
}  // namespace

MofeiDisplay::MofeiDisplay(int8_t sclk, int8_t mosi, int8_t cs, int8_t dc, int8_t rst, int8_t busy)
    : sclk(sclk), mosi(mosi), cs(cs), dc(dc), rst(rst), busy(busy), spiSettings(MOFEI_SPI_HZ, MSBFIRST, SPI_MODE0) {}

void MofeiDisplay::begin() {
  frameBuffer = frameBuffer0;
  memset(frameBuffer, 0xFF, BUFFER_SIZE);

  SPI.begin(sclk, -1, mosi, cs);
  pinMode(cs, OUTPUT);
  pinMode(dc, OUTPUT);
  pinMode(rst, OUTPUT);
  pinMode(busy, INPUT);
  digitalWrite(cs, HIGH);
  digitalWrite(dc, HIGH);

  LOG_INF("DISPLAY", "Mofei GDEQ0426T82-T01C init pins CLK=%d DIN=%d CS=%d DC=%d RST=%d BUSY=%d", sclk, mosi, cs, dc,
          rst, busy);
  initController(false);
  forceFullRefreshNext = true;
  isAwake = true;
}

void MofeiDisplay::resetDisplay() {
  digitalWrite(rst, LOW);
  delay(10);
  digitalWrite(rst, HIGH);
  delay(10);
}

void MofeiDisplay::initController(bool fastMode) {
  resetDisplay();
  waitWhileBusy("reset");

  sendCommand(CMD_SOFT_RESET);
  waitWhileBusy("soft reset");

  sendCommand(CMD_TEMP_SENSOR_CONTROL);
  sendData(0x80);

  sendCommand(CMD_BOOSTER_SOFT_START);
  sendData(0xAE);
  sendData(0xC7);
  sendData(0xC3);
  sendData(0xC0);
  sendData(0x80);

  sendCommand(CMD_DRIVER_OUTPUT_CONTROL);
  sendData((DISPLAY_HEIGHT - 1) % 256);
  sendData((DISPLAY_HEIGHT - 1) / 256);
  sendData(0x02);

  sendCommand(CMD_BORDER_WAVEFORM);
  sendData(0x01);

  setRamArea();

  if (fastMode) {
    sendCommand(CMD_WRITE_TEMP);
    sendData(0x5A);
    sendCommand(CMD_DISPLAY_UPDATE_CTRL2);
    sendData(0x91);
    sendCommand(CMD_MASTER_ACTIVATION);
    waitWhileBusy("fast init");
  }
}

void MofeiDisplay::waitWhileBusy(const char* comment) const {
  const unsigned long start = millis();
  while (digitalRead(busy) == HIGH) {
    delay(1);
    if (millis() - start > BUSY_TIMEOUT_MS) {
      LOG_ERR("DISPLAY", "Mofei busy timeout: %s", comment ? comment : "unknown");
      break;
    }
  }
  if (comment != nullptr) {
    LOG_DBG("DISPLAY", "Mofei wait complete: %s (%lu ms)", comment, millis() - start);
  }
}

void MofeiDisplay::sendCommand(uint8_t command) const {
  SPI.beginTransaction(spiSettings);
  digitalWrite(dc, LOW);
  digitalWrite(cs, LOW);
  SPI.transfer(command);
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
}

void MofeiDisplay::sendData(uint8_t data) const {
  SPI.beginTransaction(spiSettings);
  digitalWrite(dc, HIGH);
  digitalWrite(cs, LOW);
  SPI.transfer(data);
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
}

void MofeiDisplay::sendData(const uint8_t* data, uint32_t length) const {
  SPI.beginTransaction(spiSettings);
  digitalWrite(dc, HIGH);
  digitalWrite(cs, LOW);
  while (length > 0) {
    const uint32_t chunk = length > 4096 ? 4096 : length;
    SPI.writeBytes(data, chunk);
    data += chunk;
    length -= chunk;
  }
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
}

void MofeiDisplay::setRamArea() {
  sendCommand(CMD_DATA_ENTRY_MODE);
  sendData(0x03);

  sendCommand(CMD_SET_RAM_X_RANGE);
  sendData(0x00);
  sendData(0x00);
  sendData((DISPLAY_WIDTH - 1) % 256);
  sendData((DISPLAY_WIDTH - 1) / 256);

  sendCommand(CMD_SET_RAM_Y_RANGE);
  sendData(0x00);
  sendData(0x00);
  sendData((DISPLAY_HEIGHT - 1) % 256);
  sendData((DISPLAY_HEIGHT - 1) / 256);

  sendCommand(CMD_SET_RAM_X_COUNTER);
  sendData(0x00);
  sendData(0x00);
  sendCommand(CMD_SET_RAM_Y_COUNTER);
  sendData(0x00);
  sendData(0x00);
  waitWhileBusy("ram area");
}

void MofeiDisplay::writeRam(uint8_t command, const uint8_t* data) {
  setRamArea();
  sendCommand(command);
  sendData(data, BUFFER_SIZE);
}

void MofeiDisplay::writeRepeatedRam(uint8_t command, uint8_t value) {
  setRamArea();
  sendCommand(command);
  SPI.beginTransaction(spiSettings);
  digitalWrite(dc, HIGH);
  digitalWrite(cs, LOW);
  for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
    SPI.transfer(value);
  }
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
}

void MofeiDisplay::writeLut(const uint8_t* waveform) {
  sendCommand(CMD_WRITE_LUT);
  for (uint8_t i = 0; i < MOFEI_LUT_WAVEFORM_BYTES; ++i) {
    sendData(pgm_read_byte(waveform + i));
  }
  waitWhileBusy("lut");

  sendCommand(CMD_GATE_VOLTAGE);
  sendData(pgm_read_byte(waveform + MOFEI_LUT_GATE_OFFSET));

  sendCommand(CMD_SOURCE_VOLTAGE);
  sendData(pgm_read_byte(waveform + MOFEI_LUT_SOURCE_OFFSET));
  sendData(pgm_read_byte(waveform + MOFEI_LUT_SOURCE_OFFSET + 1));
  sendData(pgm_read_byte(waveform + MOFEI_LUT_SOURCE_OFFSET + 2));

  sendCommand(CMD_WRITE_VCOM);
  sendData(pgm_read_byte(waveform + MOFEI_LUT_VCOM_OFFSET));
}

void MofeiDisplay::writeLutFull() { writeLut(MOFEI_LUT_FULL); }

void MofeiDisplay::writeLutFast() { writeLut(MOFEI_LUT_FAST); }

void MofeiDisplay::updateFull() {
  writeLutFull();
  sendCommand(CMD_DISPLAY_UPDATE_CTRL2);
  sendData(0xC7);
  sendCommand(CMD_MASTER_ACTIVATION);
  waitWhileBusy("full");
  isAwake = true;
}

void MofeiDisplay::updateFast() {
  writeLutFast();
  waitWhileBusy("fast lut");
  sendCommand(CMD_DISPLAY_UPDATE_CTRL2);
  sendData(0xC7);
  sendCommand(CMD_MASTER_ACTIVATION);
  waitWhileBusy("fast");
  isAwake = true;
}

void MofeiDisplay::clearScreen(uint8_t color) const { memset(frameBuffer, color, BUFFER_SIZE); }

void MofeiDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                             bool fromProgmem) const {
  if (imageData == nullptr || frameBuffer == nullptr) {
    return;
  }
  const uint16_t imageWidthBytes = w / 8;
  for (uint16_t row = 0; row < h; ++row) {
    const uint16_t destY = y + row;
    if (destY >= DISPLAY_HEIGHT) {
      break;
    }
    const uint16_t destByteX = x / 8;
    if (destByteX >= DISPLAY_WIDTH_BYTES) {
      break;
    }
    const uint16_t copyBytes = min<uint16_t>(imageWidthBytes, DISPLAY_WIDTH_BYTES - destByteX);
    const uint32_t destOffset = static_cast<uint32_t>(destY) * DISPLAY_WIDTH_BYTES + destByteX;
    const uint32_t srcOffset = static_cast<uint32_t>(row) * imageWidthBytes;
    for (uint16_t col = 0; col < copyBytes; ++col) {
      frameBuffer[destOffset + col] = progmemRead(imageData, srcOffset + col, fromProgmem);
    }
  }
}

void MofeiDisplay::drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                        bool fromProgmem) const {
  if (imageData == nullptr || frameBuffer == nullptr) {
    return;
  }
  const uint16_t imageWidthBytes = w / 8;
  for (uint16_t row = 0; row < h; ++row) {
    const uint16_t destY = y + row;
    if (destY >= DISPLAY_HEIGHT) {
      break;
    }
    const uint16_t destByteX = x / 8;
    if (destByteX >= DISPLAY_WIDTH_BYTES) {
      break;
    }
    const uint16_t copyBytes = min<uint16_t>(imageWidthBytes, DISPLAY_WIDTH_BYTES - destByteX);
    const uint32_t destOffset = static_cast<uint32_t>(destY) * DISPLAY_WIDTH_BYTES + destByteX;
    const uint32_t srcOffset = static_cast<uint32_t>(row) * imageWidthBytes;
    for (uint16_t col = 0; col < copyBytes; ++col) {
      frameBuffer[destOffset + col] &= progmemRead(imageData, srcOffset + col, fromProgmem);
    }
  }
}

void MofeiDisplay::copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
  copyGrayscaleLsbBuffers(lsbBuffer);
  copyGrayscaleMsbBuffers(msbBuffer);
}

void MofeiDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) {
  if (lsbBuffer == nullptr) {
    grayscaleLsbReady = false;
    return;
  }
  writeRam(CMD_WRITE_RAM_BW, lsbBuffer);
  grayscaleLsbReady = true;
}

void MofeiDisplay::copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) {
  if (msbBuffer == nullptr || !grayscaleLsbReady) {
    return;
  }
  writeRam(CMD_WRITE_RAM_RED, msbBuffer);
}

void MofeiDisplay::cleanupGrayscaleBuffers(const uint8_t* bwBuffer) {
  if (bwBuffer == nullptr) {
    return;
  }
  writeRam(CMD_WRITE_RAM_RED, bwBuffer);
  grayscaleLsbReady = false;
}

void MofeiDisplay::displayBuffer(RefreshMode mode, bool turnOffScreen) {
  if (forceFullRefreshNext || !isAwake) {
    LOG_INF("DISPLAY", "Mofei forcing full refresh for display RAM baseline");
    mode = FULL_REFRESH;
    forceFullRefreshNext = false;
  }

  if (mode == FULL_REFRESH) {
    writeRam(CMD_WRITE_RAM_BW, frameBuffer);
    writeRam(CMD_WRITE_RAM_RED, frameBuffer);
    updateFull();
  } else {
    writeRam(CMD_WRITE_RAM_BW, frameBuffer);
    updateFast();
    writeRam(CMD_WRITE_RAM_RED, frameBuffer);
  }

  if (turnOffScreen) {
    deepSleep();
  }
}

void MofeiDisplay::displayGrayBuffer(bool turnOffScreen) {
  if (grayscaleLsbReady) {
    updateFast();
  } else {
    displayBuffer(FAST_REFRESH, turnOffScreen);
    return;
  }
  if (turnOffScreen) {
    deepSleep();
  }
}

void MofeiDisplay::refreshDisplay(RefreshMode mode, bool turnOffScreen) {
  if (mode == FULL_REFRESH) {
    updateFull();
  } else {
    updateFast();
  }
  if (turnOffScreen) {
    deepSleep();
  }
}

void MofeiDisplay::requestFullRefresh() { forceFullRefreshNext = true; }

void MofeiDisplay::deepSleep() {
  sendCommand(CMD_DEEP_SLEEP);
  sendData(0x01);
  delay(100);
  isAwake = false;
  forceFullRefreshNext = true;
}

#endif
