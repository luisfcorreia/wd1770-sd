# WD1770-SD Pin Assignments

Complete pin mapping for STM32F411 Black Pill board.

## Pin Assignment Summary

### Issues Resolved
1. **USB pins:** PA11/PA12 reserved for USB (not available as GPIO)
2. **SWD pins:** PA13/PA14 reserved for programming (avoid as GPIO)
3. **LSE oscillator:** PC14/PC15 cause crashes when used (avoid entirely)
4. **Pin name conflicts:** PIN_A0/PIN_A1 conflicted with Arduino analog pins (renamed to WD_A0/WD_A1)
5. **Data bus:** PB0-PB7 must remain consecutive for efficient parallel I/O
6. **R/W signal:** Corrected to single pin matching real WD1770 chip (not separate RE/WE)

---

## Complete Pin Map

### WD1770 Data Bus
**Location:** PB0-PB7 (consecutive, cannot be changed)
```
D0 = PB0
D1 = PB1
D2 = PB2
D3 = PB3
D4 = PB4
D5 = PB5
D6 = PB6
D7 = PB7
```

### WD1770 Control Signals
```
A0    = PA8   (Register select bit 0)
A1    = PA9   (Register select bit 1)
CS    = PA10  (Chip select, active low)
R/W   = PB15  (Read/Write: HIGH=read, LOW=write)
INTRQ = PA15  (Interrupt request output)
DRQ   = PB8   (Data request output)
```

### WD1770 Input Signals (from target system)
```
DDEN = PB9   (Density select, active low)
DS0  = PB12  (Drive select 0, active high)
DS1  = PB13  (Drive select 1, active high)
```

### SD Card (SPI1)
```
CS   = PA4  (Chip select)
SCK  = PA5  (SPI clock)
MISO = PA6  (Master in, slave out)
MOSI = PA7  (Master out, slave in)
```

### OLED Display (Software I2C, SH1106 driver)
```
SDA = PB14  (I2C data)
SCL = PA3   (I2C clock)
VCC = 3.3V  (CRITICAL: NOT 5V!)
GND = GND
```
**Note:** Software I2C used because all hardware I2C pins conflict with data bus or other signals.

### User Interface
```
LED = PC13  (On-board LED)

Rotary Encoder:
  CLK = PA0  (Encoder output A)
  DT  = PA1  (Encoder output B)
  SW  = PA2  (Push button, active low)
```

---

## Reserved Pins - DO NOT USE

**These pins cause system failures if used as GPIO:**

### USB Pins (breaks serial communication)
```
PA11 = USB D-
PA12 = USB D+
```

### SWD Programming Pins (breaks debugging/upload)
```
PA13 = SWDIO
PA14 = SWDCLK
```

### LSE Oscillator Pins (causes system crashes)
```
PC14 = LSE_IN
PC15 = LSE_OUT
```

### Not Physically Exposed
```
PB11 = Not available on Black Pill PCB
```

---

## Free Pins Available for Expansion

```
PB10 - Available (previously incorrectly assigned)
```

---

## Pull-up Resistors Required

**For FDC bus interface (10kΩ to 3.3V):**
```
WD_A0   (PA8)
WD_A1   (PA9)
WD_CS   (PA10)
WD_RW   (PB15)
WD_DDEN (PB9)
WD_DS0  (PB12)
WD_DS1  (PB13)
```

**Optional for I2C (4.7kΩ to 3.3V):**
```
OLED_SDA (PB14)
OLED_SCL (PA3)
```
Improves signal integrity with long wires or multiple I2C devices.

---

## Why These Specific Pins?

### Data Bus on PB0-PB7
- **Reason:** Allows reading entire byte in single GPIO port read
- **Performance:** ~10x faster than reading individual pins
- **Requirement:** Must be consecutive pins on same port

### Software I2C for OLED
Hardware I2C conflicts:
- **I2C1 (PB6/PB7):** Used by data bus D6/D7
- **I2C1 (PB8/PB9):** Used by DRQ/DDEN
- **I2C2 (PB10/PB3):** PB3 used by data bus D3

Software I2C on PB14/PA3 avoids all conflicts and provides adequate speed (~50-100kHz) for OLED.

### Single R/W Pin
Real WD1770 chip pin 2 is R/W (not separate RE/WE):
- **HIGH:** CPU reading from WD1770 (chip drives data bus)
- **LOW:** CPU writing to WD1770 (chip reads data bus)

---

## Register Select Mapping

| A1 | A0 | Register | Description |
|----|----|----|-------------|
| 0 | 0 | 0 | Status (read) / Command (write) |
| 0 | 1 | 1 | Track register |
| 1 | 0 | 2 | Sector register |
| 1 | 1 | 3 | Data register |

---

## Voltage Levels

**All pins operate at 3.3V logic levels.**

If connecting to 5V systems:
- do nothing, STM32 is 5V tolerant

---

## Wiring Verification Checklist

Before powering on:
- [ ] OLED VCC connected to 3.3V (NOT 5V)
- [ ] All GND connections secure
- [ ] SD card module powered from 3.3V
- [ ] No shorts between VCC and GND
- [ ] USB cable data-capable (not charge-only)
- [ ] Pull-ups installed on FDC control signals (if connecting to hardware)

---

## TEST_MODE Configuration

When `TEST_MODE = 1` in code:
- DDEN configured as INPUT_PULLDOWN (reads LOW = FDC enabled)
- DS0 configured as INPUT_PULLUP (reads HIGH = drive 0 selected)
- DS1 configured as INPUT_PULLDOWN (reads LOW = drive 1 not selected)

Allows testing without FDC hardware connected.

---

## Pin Name Changes from Original Code

To avoid Arduino framework conflicts:

| Old Name | New Name | Reason |
|----------|----------|--------|
| PIN_A0 | WD_A0 | Conflicted with Arduino A0 (analog pin) |
| PIN_A1 | WD_A1 | Conflicted with Arduino A1 (analog pin) |
| PIN_D0-D7 | WD_D0-D7 | Clarity (WD = Western Digital) |
| PIN_CS | WD_CS | Consistency |
| PIN_RE, PIN_WE | WD_RW | Corrected to match real WD1770 |

---

## Quick Reference Table

| Pin | Function | Direction | Pull-up | Notes |
|-----|----------|-----------|---------|-------|
| PA0 | ROTARY_CLK | Input | Internal | Encoder A |
| PA1 | ROTARY_DT | Input | Internal | Encoder B |
| PA2 | ROTARY_SW | Input | Internal | Button |
| PA3 | OLED_SCL | Output | Optional | Software I2C |
| PA4 | SD_CS | Output | - | SPI |
| PA5 | SD_SCK | Output | - | SPI |
| PA6 | SD_MISO | Input | - | SPI |
| PA7 | SD_MOSI | Output | - | SPI |
| PA8 | WD_A0 | Input | 10k | FDC |
| PA9 | WD_A1 | Input | 10k | FDC |
| PA10 | WD_CS | Input | 10k | FDC |
| PA11 | USB D- | - | - | Reserved |
| PA12 | USB D+ | - | - | Reserved |
| PA13 | SWDIO | - | - | Reserved |
| PA14 | SWDCLK | - | - | Reserved |
| PA15 | WD_INTRQ | Output | - | FDC |
| PB0-7 | WD_D0-D7 | Bidir | - | FDC data |
| PB8 | WD_DRQ | Output | - | FDC |
| PB9 | WD_DDEN | Input | 10k | FDC |
| PB10 | - | Free | - | Available |
| PB11 | - | - | - | Not exposed |
| PB12 | WD_DS0 | Input | 10k | FDC |
| PB13 | WD_DS1 | Input | 10k | FDC |
| PB14 | OLED_SDA | Bidir | Optional | Software I2C |
| PB15 | WD_RW | Input | 10k | FDC |
| PC13 | LED | Output | - | Status |
| PC14 | - | - | - | Avoid (LSE) |
| PC15 | - | - | - | Avoid (LSE) |

---

**Last Updated:** February 2026 - Matches wd1770-emu-u8g2.ino v1.0
