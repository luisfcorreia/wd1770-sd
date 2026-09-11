# WD1770-SD Pin Assignments

Complete pin mapping for STM32F411 Black Pill board.

## Quick Reference Table

| Pin | Function | Direction | Pull-up | Notes |
|-----|----------|-----------|---------|-------|
| PA0 | BTN_UP | Input | Internal | Button, active low |
| PA1 | BTN_DOWN | Input | Internal | Button, active low |
| PA2 | BTN_SELECT | Input | Internal | Button, active low |
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
LED    = PC13  (On-board LED)

Buttons (active low, internal pull-up):
  BTN_UP     = PA0  (Navigate up)
  BTN_DOWN   = PA1  (Navigate down)
  BTN_SELECT = PA2  (Confirm / enter menu)
```

---

## Free Pins Available for Expansion

```
PB10 - Available
```

---

## TEST_MODE Configuration

When `TEST_MODE = 1` in wd1770.ino:
- DDEN configured as INPUT_PULLDOWN (reads LOW = FDC enabled)
- DS0 configured as INPUT_PULLUP (reads HIGH = drive 0 selected)
- DS1 configured as INPUT_PULLDOWN (reads LOW = drive 1 not selected)

Allows testing without FDC hardware connected.

---

**Last Updated:** February 2026 - Matches wd1770/wd1770.ino
