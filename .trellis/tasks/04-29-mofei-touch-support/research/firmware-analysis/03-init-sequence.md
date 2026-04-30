# Firmware Analysis: FT6336U Initialization Sequence

Date: 2026-04-29

## Reconstructed Init Sequence from Factory Firmware

Based on string references, struct layouts, and register patterns found in `firmware_V618_E426_TOUCH.bin`:

### 1. GPIO Initialization (`initPins`)
```
1. pinMode(GPIO45, OUTPUT);     // PWR — enable touch controller power
2. digitalWrite(GPIO45, ?);       // Set to enable level (HIGH or LOW — polarity unknown)
3. pinMode(GPIO7, OUTPUT);       // RST — configure reset pin
4. digitalWrite(GPIO7, HIGH);    // RST de-asserted (assumed active-low on FT6336U)
5. pinMode(GPIO44, INPUT);       // INT — configure interrupt input
6. delay(?ms);                   // Power settle time (unknown from binary alone)
```

### 2. I2C Bus Initialization (`beginInternal`)
```
1. Wire.begin(13, 12, 100000);   // SDA=GPIO13, SCL=GPIO12, 100 kHz
   // Hardware I2C (not soft/bitbang)
```

### 3. Device Detection
```
For each configured address + pin combination:
  1. Wire.beginTransmission(0x2E);
  2. If ACK received:
     a. Read TD_STATUS (register 0x02) — expect valid frame
     b. Validate frame (max 2 touch points, valid status bits)
     c. If valid → mark device detected
  3. If NAK → device not present
```

### 4. Controller Configuration (`configureController`)
```
If detected:
  1. Write DEVICE_MODE (0x00) = 0     // Working mode
  2. Write THGROUP (0x80) = 22        // Touch threshold
  3. Write PERIODACTIVE (0x88) = 14   // 14ms report interval
  4. Read back DEVICE_MODE to verify  // Confirm writes took effect
```

### 5. Touch Polling Task (`touchTask`)
```
Task: touchTask (FreeRTOS task)
Poll interval: ~10-20ms (typical for FT6336U)

Loop:
  1. Check INT pin (GPIO44) — if LOW, touch data available
  2. Read TD_STATUS (0x02): 6+ bytes
     - Byte 0: TD_STATUS (bits 0-3 = touch points count, max 2)
     - Byte 1-2: Touch 1 X coordinate
     - Byte 3-4: Touch 1 Y coordinate
     - Byte 5+: Touch 2 data (if present)
  3. Parse coordinates (12-bit values)
  4. Generate events: tap, swipe left/right/up/down
  5. vTaskDelay(poll_interval)
```

## Windows/Arduino Build Environment

The factory firmware was built with:
- **OS**: Windows (path: `C:/Users/HZW/`)
- **Framework**: Arduino ESP32 (via PlatformIO)
- **ESP-IDF**: v4.4.7 (with local modifications — "dirty" tag)
- **Arduino core**: `framework-arduinoespressif32`
- **I2C driver**: Standard Arduino `Wire` library (hardware I2C)
- **No soft I2C**: The firmware uses `i2c_param_config`/`i2c_driver_install` from ESP-IDF — hardware I2C peripheral

## Key Observations

1. The factory firmware uses **hardware I2C** (ESP32-S3 I2C peripheral), not soft/bitbang I2C
2. AHT20 and FT6336U are **separately detected** — they can coexist on the same bus
3. The firmware does NOT do GPIO auto-scanning — pins are hardcoded
4. There is no evidence of alternate I2C bus configurations (no SDA=12/SCL=11 anywhere)
5. The detection sequence is: power → reset → I2C probe at 0x2E → configure → poll

## What We Cannot Determine from Binary Alone

1. **Power polarity**: HIGH vs LOW for PWR_ENABLE_LEVEL — encoded in runtime code paths
2. **Exact delay values**: Power settle time, reset pulse width, settle after reset
3. **INT polarity**: Active-LOW or active-HIGH trigger edge
4. **Swipe detection thresholds**: Distance and time thresholds for gesture classification
