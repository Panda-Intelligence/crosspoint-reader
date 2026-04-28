#pragma once

#include <Arduino.h>
#include <SPI.h>

#if MOFEI_DEVICE

class MofeiDisplay {
 public:
  enum RefreshMode {
    FULL_REFRESH,
    HALF_REFRESH,
    FAST_REFRESH,
  };

  MofeiDisplay(int8_t sclk, int8_t mosi, int8_t cs, int8_t dc, int8_t rst, int8_t busy);

  void begin();

  static constexpr uint16_t DISPLAY_WIDTH = 800;
  static constexpr uint16_t DISPLAY_HEIGHT = 480;
  static constexpr uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
  static constexpr uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;

  uint16_t getDisplayWidth() const { return DISPLAY_WIDTH; }
  uint16_t getDisplayHeight() const { return DISPLAY_HEIGHT; }
  uint16_t getDisplayWidthBytes() const { return DISPLAY_WIDTH_BYTES; }
  uint32_t getBufferSize() const { return BUFFER_SIZE; }

  void clearScreen(uint8_t color = 0xFF) const;
  void drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 bool fromProgmem = false) const;
  void drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            bool fromProgmem = false) const;

  void copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer);
  void copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer);
  void copyGrayscaleMsbBuffers(const uint8_t* msbBuffer);
  void cleanupGrayscaleBuffers(const uint8_t* bwBuffer);

  void displayBuffer(RefreshMode mode = FAST_REFRESH, bool turnOffScreen = false);
  void displayGrayBuffer(bool turnOffScreen = false);
  void refreshDisplay(RefreshMode mode = FAST_REFRESH, bool turnOffScreen = false);
  void requestFullRefresh();
  void deepSleep();

  uint8_t* getFrameBuffer() const { return frameBuffer; }

 private:
  int8_t sclk;
  int8_t mosi;
  int8_t cs;
  int8_t dc;
  int8_t rst;
  int8_t busy;
  SPISettings spiSettings;
  uint8_t frameBuffer0[BUFFER_SIZE];
  uint8_t* frameBuffer = frameBuffer0;
  bool isAwake = false;
  bool forceFullRefreshNext = true;
  bool grayscaleLsbReady = false;

  void resetDisplay();
  void initController(bool fastMode);
  void waitWhileBusy(const char* comment = nullptr) const;
  void sendCommand(uint8_t command) const;
  void sendData(uint8_t data) const;
  void sendData(const uint8_t* data, uint32_t length) const;
  void setRamArea();
  void writeRam(uint8_t command, const uint8_t* data);
  void writeRepeatedRam(uint8_t command, uint8_t value);
  void writeLut(const uint8_t* waveform);
  void writeLutFull();
  void writeLutFast();
  void updateFull();
  void updateFast();
};

#endif
