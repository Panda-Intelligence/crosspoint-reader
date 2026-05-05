# Firmware Analysis: firmware_V618_E426_TOUCH.bin

Date: 2026-04-29
Owner: isaac.liu (analysis by Codex)

## Binary Identity

| Field | Value |
|---|---|
| File | `firmware_V618_E426_TOUCH.bin` |
| Size | 7,015,888 bytes |
| Magic | 0xE9 (ESP32-S3 image) |
| Segments | 7 |
| Entry point | 0x40377FF8 |
| Flash size | 16MB |
| Flash freq | 80 MHz |
| Flash mode | DIO |
| Chip | ESP32-S3 (chip ID 9) |

## Build Provenance

| Field | Value |
|---|---|
| Project name | `arduino-lib-builder` |
| App version | `esp-idf: v4.4.7 38eeba213a` |
| ESP-IDF | v4.4.7-dirty |
| Compile time | 2024-03-05 12:12:53 |
| Builder | HZW (Windows path: `C:/Users/HZW/.platformio/...`) |
| Framework | `framework-arduinoespressif32` (Arduino on ESP-IDF) |
| ELF SHA256 | `08e51e7b9245573916152697edc16b2475a59245c1256c7937fd6d31950f693a` |
| Image SHA256 | `a40bd1467ca436319e2db7c147a1922761b172f2c74f4bde4c3217e1af5c9ba4` |

## Segment Layout

| Seg | Load Address | Size | Type |
|---|---|---|---|
| 0 | 0x3C1A0020 | 5,195,416 | DROM |
| 1 | 0x3FC9AC60 | 30,464 | DRAM |
| 2 | 0x40374000 | 16,976 | IRAM |
| 3 | 0x42000020 | 1,682,128 | IROM |
| 4 | 0x40378250 | 76,296 | IRAM2 |
| 5 | 0x50000000 | 7,276 | RTC_DATA |
| 6 | 0x600FE000 | 7,212 | RTC_DRAM |

## Key Strings Extracted

| Address | String | Meaning |
|---|---|---|
| 0x3C456F33 | `touchTask` | FreeRTOS task name for touch handling |
| 0x3C623BC2 | `initPins` | GPIO initialization function |
| 0x3C623C03 | `BAHTx0_H` | AHT20 humidity reading variable |
| 0x3C623C0C | `AHTx0_T` | AHT20 temperature reading variable |
| 0x3C58AD8D | `FT6` | FT6336U driver reference (abbreviated) |
| 0x3C5D869A | `aht` | AHT20 driver reference |
| 0x3C623B93 | `beginTransmission` | I2C begin function |
| 0x3C639D80 | `setPins` | Pin configuration function |
| 0x3C3EF760 | `pwr` | Power control reference |
| 0x3C44BEE7 | `Touch` | UI touch label |
| 0x3C449E72 | `touch` | touch detection code |
| 0x3C44D782 | `detect` | detection function |
