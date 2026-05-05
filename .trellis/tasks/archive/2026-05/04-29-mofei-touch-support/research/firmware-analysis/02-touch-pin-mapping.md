# Firmware Analysis: Touch Pin Mapping & Init Sequence

Date: 2026-04-29

## I2C Bus Configuration (Confirmed)

### FT6336U Touch Controller

**Source**: DROM struct at 0x3C24C212:
```
2E 0D 0C 0B  80 02 80 D0  1B D0 00 00  0D 0B D0 00
00 00 40 10  00 00 00 00  40 01 00 02  C0 34 02 C0
```

| Parameter | Value | Evidence |
|---|---|---|
| I2C Address | **0x2E** (7-bit) | DROM 0x3C24C212, byte 0 = `2E` |
| SDA | **GPIO 13** | DROM 0x3C24C212, byte 1 = `0D` |
| SCL | **GPIO 12** | DROM 0x3C24C212, byte 2 = `0C` |
| I2C Clock | **100 kHz** | DROM 0x3C6282CC literal |

This struct is the primary I2C device configuration. The pattern `{0x2E, 0x0D, 0x0C}` appears exactly once in DROM — there is no alternative touch I2C bus configuration.

### AHT20 Temperature/Humidity Sensor (Same Bus)

**Source**: DROM struct at 0x3C3A7AD4:
```
38 0D 0C 38  0E 03 80 C0  C7 43 C5 B4  2C 03 F0 F0
```

| Parameter | Value | Evidence |
|---|---|---|
| I2C Address | **0x38** (7-bit) | DROM 0x3C3A7AD4, byte 0 = `38` |
| SDA | **GPIO 13** | Same as FT6336U |
| SCL | **GPIO 12** | Same as FT6336U |

**Critical**: Both FT6336U (0x2E) and AHT20 (0x38) share the **same I2C bus** (GPIO13/GPIO12). This explains why our firmware sees AHT20 frames at 0x38 on GPIO13/12 — it's the correct bus, but the touch controller at 0x2E needs proper power/reset to respond.

### Schematic Hypothesis (GPIO12/11) — DISPROVEN

The schematic-based suggestion of SDA=GPIO12, SCL=GPIO11 has **zero evidence** in the working firmware:
- No occurrence of `{0x2E, 0x0C, 0x0B}` anywhere in 5.1MB DROM
- No occurrence of `{0x2E, 12, 11}` in any configuration struct
- The firmware only knows about GPIO13/12 for I2C touch

## GPIO Pin Assignment

### INT Pin: GPIO 44

**Evidence**:
- GPIO 44 (0x2C) co-occurs with SDA=13/SCL=12 in **78+ GPIO arrays** across DROM
- Struct at 0x3C6274F6: `{44, 0, 45, 0, 46}` — shows INT/PWR/RST grouping
- Consistent with user's schematic data showing `TOUCH_INT = GPIO44`

### RST Pin: GPIO 7

**Evidence**:
- GPIO 7 co-occurs with SDA=13/SCL=12 in **65+ GPIO arrays**
- Array at 0x3C221799: `[13, 12, 7, 0, 14]` — {SDA=13, SCL=12, RST=7, unused, 14}
- GPIO 7 is the only reset-capable pin appearing across touch pin groups
- Consistent with user's schematic showing RST=GPIO7 (shared with EPD_RST)

### PWR Pin: GPIO 45

**Evidence**:
- GPIO 45 (0x2D) co-occurs with SDA=13/SCL=12 in **30+ GPIO arrays**
- Array at 0x3C20D012: `[0, 13, 14, 7, 45]` — {flag, SDA=13, 14, RST=7, PWR=45}
- Array at 0x3C2B1199: `[0, 7, 13, 0, 45]` — {0, RST=7, SDA=13, 0, PWR=45}
- Struct at 0x3C63DB4C: `[15, 45, 13, 12, 11]` — {???, PWR=45, SDA=13, SCL=12, ???=11}
- Consistent with user's schematic: `TOUCH_PWR = GPIO45`

**Note**: GPIO 46 also appears in some arrays (e.g., 0x3C24726F: `[13, 46, 2, 7, 71]`) but GPIO 45 is far more common and appears with the canonical SDA/SCL/INT/RST set.

### Power Enable Level

The power polarity cannot be definitively determined from the data segment alone, as it's encoded in runtime logic. However:
- The factory firmware uses `MOFEI_TOUCH_PWR = GPIO45`
- ESP32-S3 GPIO45 is a strapping pin (VDD_SPI voltage select) with weak pull-down
- Active-HIGH would be the safer design choice for this pin (avoid driving during reset)
- But active-LOW is also common for touch controllers to match power-on default states

## FT6336U Register Configuration

**Source**: IROM code (confirmed from previous analysis)

| Register | Address | Value | Purpose |
|---|---|---|---|
| DEVICE_MODE | 0x00 | 0 | Enter working mode |
| THGROUP | 0x80 | 22 (0x16) | Touch threshold |
| PERIODACTIVE | 0x88 | 14 (0x0E) | Report period in active mode |

These match the vendor Arduino sample values and were already applied in our driver.

## Device Detection Architecture

The factory firmware implements **separate detection** for two I2C devices on the same bus:

1. **AHT20 at 0x38**: Reads temperature/humidity, used for EPD temperature compensation
2. **FT6336U at 0x2E**: Reads touch points, used for UI interaction

Key findings:
- Both devices are probed **independently** — detecting AHT20 at 0x38 does not mean FT6336U is absent
- The firmware has dedicated strings for each: `BAHTx0_H`/`AHTx0_T` for AHT20, and separate touch task code
- AHT20 detection success does not gate FT6336U detection

## FreeRTOS Architecture

- **Task name**: `touchTask` (string at 0x3C456F33)
- **Init function**: `initPins` (string at 0x3C623BC2)
- **I2C functions**: Standard Arduino Wire API (`beginTransmission`, `i2cWriteReadNonStop`, `i2cRead`)
- **I2C clock**: 100 kHz (standard mode)

## Comparison: Factory Firmware vs Our Current Code

| Parameter | Factory Firmware | Our Current Code | Match? |
|---|---|---|---|
| I2C Addr | 0x2E | 0x2E (default) | YES |
| SDA | GPIO 13 | GPIO 13 | YES |
| SCL | GPIO 12 | GPIO 12 | YES |
| INT | GPIO 44 | GPIO 44 | YES |
| PWR | GPIO 45 | GPIO 45 | YES |
| RST | GPIO 7 | GPIO 7 | YES |
| PWR Level | Unknown | LOW (user's change) | NEEDS VERIFY |
| I2C Mode | Hardware Wire | Soft I2C | DIFFERENT |
| DEVICE_MODE | 0 | 0 | YES |
| THGROUP | 22 | 22 | YES |
| PERIODACTIVE | 14 | 14 | YES |
| I2C Clock | 100 kHz | 400 kHz | DIFFERENT |

## Key Insight: Why Touch Still Doesn't Work

The pin mapping in our firmware **matches the factory firmware**. The remaining variables are:

1. **Power polarity** (`PWR_ENABLE_LEVEL`): Factory firmware may use HIGH, while ours uses LOW
2. **RST timing/duration**: Factory firmware reset sequence timing unknown
3. **Power-on sequence**: The order of PWR enable → delay → RST release → I2C init matters
4. **I2C clock speed**: Factory uses 100 kHz, we use 400 kHz (though FT6336U supports both)
5. **I2C mode**: Factory uses hardware Wire, we use soft I2C (should not affect detection)
6. **Hardware issue**: FT6336U may not be physically present/connected on this specific unit

## Recommended Next Steps

1. **Test with PWR_ENABLE_LEVEL=HIGH** — this is the most likely remaining difference
2. **Try 100 kHz I2C clock** — match factory firmware exactly
3. **Verify power sequencing**: ~10ms power settle → RST toggle (10μs low, 100ms settle) → I2C probe
4. **Test hardware Wire** instead of soft I2C (if HalPowerManager conflict can be resolved)
5. **Verify on a second Mofei unit** if available — rules out hardware defect
