# WD1770-SD Hardware Documentation

Hardware specifications, pinout, and wiring guide for the WD1770 floppy controller emulator.

## Table of Contents
1. [Hardware Overview](#hardware-overview)
2. [Pin Assignments](#pin-assignments)
3. [Wiring Guide](#wiring-guide)

---

## Hardware Overview

### STM32F411 Black Pill
- **MCU:** STM32F411CEU6
- **Core:** ARM Cortex-M4 @ 100MHz
- **Flash:** 512KB
- **RAM:** 128KB
- **GPIO:** 3.3V logic level
- **USB:** Native USB 2.0 Full Speed
- **SPI:** 2x hardware SPI controllers
- **I2C:** 3x hardware I2C controllers (using software I2C for flexibility)

**Why Black Pill?**
- Fast enough for real-time FDC emulation
- Sufficient RAM for sector buffers and display framebuffer
- Native USB for easy debugging
- Cost effective (~$4-6)
- Readily available

### Display: SH1106 128x64 OLED
**NOT SSD1306!** Most "128x64 I2C OLED" displays sold are actually SH1106.

**Specifications:**
- Resolution: 128x64 pixels
- Interface: I2C (software bit-bang on any GPIO)
- I2C Address: Usually 0x3C (sometimes 0x3D)
- Power: 3.3V only (5V will destroy it!)
- Current: ~20mA typical

**Driver:** U8g2 library with `U8G2_SH1106_128X64_NONAME_F_SW_I2C`

### SD Card Module
- Interface: SPI
- Supported cards: microSD, SDHC (up to 32GB recommended)
- Format: FAT32 only (exFAT not supported)
- Logic level: 3.3V
- Power: 3.3V, ~100mA peak during writes

### Push Buttons
- Type: Momentary push buttons, normally open
- Count: 3 (UP, DOWN, SELECT)
- Logic: Active low (internal STM32 pull-ups used)
- Connections: One terminal to GPIO pin, other terminal to GND

---

## Pin Assignments

### Complete Pinout Table

| Function | STM32 Pin | Notes |
|----------|-----------|-------|
| **Data Bus** | | |
| D0 | PB0 | WD1770 data bit 0 |
| D1 | PB1 | WD1770 data bit 1 |
| D2 | PB2 | WD1770 data bit 2 |
| D3 | PB3 | WD1770 data bit 3 |
| D4 | PB4 | WD1770 data bit 4 |
| D5 | PB5 | WD1770 data bit 5 |
| D6 | PB6 | WD1770 data bit 6 |
| D7 | PB7 | WD1770 data bit 7 |
| **Control Signals** | | |
| A0 | PA8 | Register select bit 0 |
| A1 | PA9 | Register select bit 1 |
| CS | PA10 | Chip select (active low) |
| R/W | PB15 | Read/Write (HIGH=read, LOW=write) |
| INTRQ | PA15 | Interrupt request output |
| DRQ | PB8 | Data request output |
| DDEN | PB9 | Density select input |
| DS0 | PB12 | Drive select 0 input |
| DS1 | PB13 | Drive select 1 input |
| **SD Card (SPI1)** | | |
| CS | PA4 | SPI chip select |
| SCK | PA5 | SPI clock |
| MISO | PA6 | SPI data in |
| MOSI | PA7 | SPI data out |
| **OLED (Software I2C)** | | |
| SDA | PB14 | I2C data (software) |
| SCL | PA3 | I2C clock (software) |
| **User Interface** | | |
| BTN_UP | PA0 | Up button (active low) |
| BTN_DOWN | PA1 | Down button (active low) |
| BTN_SELECT | PA2 | Select button (active low) |
| LED | PC13 | On-board LED |
| **Reserved/Avoid** | | |
| USB D- | PA11 | USB (do not use as GPIO) |
| USB D+ | PA12 | USB (do not use as GPIO) |
| SWDIO | PA13 | SWD programming |
| SWDCLK | PA14 | SWD programming |
| LSE_IN | PC14 | Oscillator (causes crashes) |
| LSE_OUT | PC15 | Oscillator (causes crashes) |

### Free Pins
- **PB10** - Available for future use
- **PB11** - NOT exposed on Black Pill PCB

---

## Wiring Guide

### Power Distribution
```
Black Pill 3V3 pin -> Breadboard 3.3V rail
Black Pill GND pin -> Breadboard GND rail
```

**CRITICAL:** All components MUST use 3.3V:
- OLED: 3.3V (5V will destroy it)
- SD card module: 3.3V

#### OLED Display
```
OLED VCC  -> 3.3V
OLED GND  -> GND
OLED SDA  -> PB14
OLED SCL  -> PA3
```

**Optional pull-ups:** 4.7kOhm resistors from SDA and SCL to 3.3V improve signal integrity for long wires.

#### SD Card Module
```
SD VCC   -> 3.3V
SD GND   -> GND
SD CS    -> PA4
SD SCK   -> PA5
SD MISO  -> PA6
SD MOSI  -> PA7
```

**Capacitor recommended:** 10uF across VCC/GND near SD card module for stable power.

#### Push Buttons
```
BTN_UP one terminal     -> PA0
BTN_DOWN one terminal   -> PA1
BTN_SELECT one terminal -> PA2
All other terminals     -> GND
```

Internal pull-ups are enabled in firmware; no external resistors needed.

**Optional:** 100nF ceramic capacitors from each signal pin to GND for hardware debouncing (firmware already does software debouncing).

#### WD1770 Bus Interface

**Pull-up resistors required (10kOhm to 3.3V):**
```
A0    -> PA8   (with 10kOhm pull-up)
A1    -> PA9   (with 10kOhm pull-up)
CS    -> PA10  (with 10kOhm pull-up)
R/W   -> PB15  (with 10kOhm pull-up)
DDEN  -> PB9   (with 10kOhm pull-up)
DS0   -> PB12  (with 10kOhm pull-up)
DS1   -> PB13  (with 10kOhm pull-up)
```

**Data bus (bidirectional, no pull-ups):**
```
D0-D7 -> PB0-PB7
```

**Output signals (no pull-ups needed):**
```
INTRQ -> PA15
DRQ   -> PB8
```

---

## Troubleshooting

### OLED Issues

**Symptom:** No display
- Check power: Must be 3.3V, measure at OLED pins
- Check address: Try both 0x3C and 0x3D in code
- Check driver: Must be SH1106, not SSD1306
- Check wiring: SDA=PB14, SCL=PA3

**Symptom:** Garbage display
- Wrong driver (SSD1306 vs SH1106)
- Poor signal integrity (add pull-ups, shorten wires)

### SD Card Issues

**Symptom:** Not detected
- Check format: Must be FAT32
- Check card type: Use Class 4-10, avoid UHS-II
- Check power: Needs stable 3.3V, add 10uF capacitor
- Try different card: Some cards are incompatible

**Symptom:** Random failures
- Power supply noise (add capacitor)
- SPI wiring too long (keep <100mm)

### Button Issues

**Symptom:** No response
- Check wiring: BTN_UP=PA0, BTN_DOWN=PA1, BTN_SELECT=PA2
- Verify button connects GPIO pin to GND when pressed
- Note: first press wakes screensaver; second press activates menu
- Add hardware debouncing (100nF capacitors from pin to GND)

### Bus Interface Issues

**Symptom:** Target system doesn't boot
- Missing pull-ups (10kOhm required on all control inputs)
- Incorrect R/W polarity
- Data bus conflict (check tri-state logic)

---

## References

**Datasheets:**
- [STM32F411 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00119316.pdf)
- [WD1770 Datasheet](http://pdf.datasheetcatalog.com/datasheet/westerndigital/WD1770-00.pdf)
- [SH1106 Datasheet](https://www.velleman.eu/downloads/29/infosheets/sh1106_datasheet.pdf)

**Additional Documentation:**
- See README.md for software setup
- See PIN_ASSIGNMENTS.md for quick reference
- See project source files for implementation details

---

**Last Updated:** February 2026
**Hardware Revision:** 1.0 (oled, buttons and menu workflow tested and validated, no FDC tests yet)
