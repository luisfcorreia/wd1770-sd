# WD1770-SD RP2350B Hardware Documentation

Hardware specifications, pinout, and wiring guide for the WD1770 floppy controller emulator on WeAct RP2350B Core.

## Table of Contents
1. [Hardware Overview](#hardware-overview)
2. [Pin Assignments](#pin-assignments)
3. [Wiring Guide](#wiring-guide)
4. [Electrical Specifications](#electrical-specifications)
5. [Troubleshooting](#troubleshooting)

---

## Hardware Overview

### WeAct RP2350B Core Board
- **MCU:** Raspberry Pi RP2350B
- **Core:** Dual ARM Cortex-M33 @ 150MHz (or dual Hazard3 RISC-V)
- **Flash:** 16MB QSPI (W25Q128JV)
- **RAM:** 520KB SRAM
- **GPIO:** 48 multi-function GPIO pins, 3.3V logic level
- **USB:** USB 1.1 device/host (USB-C connector)
- **SPI:** 2x hardware SPI controllers (SPI0, SPI1)
- **I2C:** 2x hardware I2C controllers
- **PIO:** 3 programmable I/O blocks, 12 state machines
- **PWM:** 24 channels
- **ADC:** 8-channel 12-bit 500ksps
- **Package:** QFN-80 (RP2350B variant)
- **Dimensions:** 41.4 x 41.1 mm
- **Price:** ~$4-5

**Why WeAct RP2350B Core?**
- All 48 GPIOs broken out (2x 30-pin headers)
- Same 3.3V logic as STM32 Black Pill (no level shifting needed)
- 1.5x faster clock (150MHz vs 100MHz)
- 4x more RAM (520KB vs 128KB)
- PIO hardware for timing-critical bus emulation
- Footprint for optional PSRAM on back
- Cost effective, breadboard-friendly
- Native USB for debug serial

### Display: SH1106 128x64 OLED
**NOT SSD1306!** Most "128x64 I2C OLED" displays sold are actually SH1106.

**Specifications:**
- Resolution: 128x64 pixels
- Interface: I2C (software bit-bang on any GPIO)
- I2C Address: Usually 0x3C (sometimes 0x3D)
- Power: 3.3V only (5V will destroy it!)
- Current: ~20mA typical

**Driver:** U8g2 library with `u8g2_Setup_sh1106_128x64_noname_f()`

### SD Card Module
- Interface: SPI (hardware SPI0)
- Supported cards: microSD, SDHC (up to 32GB recommended)
- Format: FAT32 only (exFAT not supported)
- Logic level: 3.3V
- Power: 3.3V, ~100mA peak during writes
- SPI clock: 16MHz (configurable)

### Push Buttons
- Type: Momentary push buttons, normally open
- Count: 3 (UP, DOWN, SELECT)
- Logic: Active low (internal RP2350B pull-ups used)
- Connections: One terminal to GPIO pin, other terminal to GND

---

## Pin Assignments

### Complete Pinout Table

| Function | GPIO | Direction | Pull-up | RP2350B Pin Notes |
|----------|------|-----------|---------|-------------------|
| **WD1770 Data Bus** | | | | |
| D0 | GP1 | Bidirectional | None | PIO0 SM0 IN base |
| D1 | GP2 | Bidirectional | None | |
| D2 | GP3 | Bidirectional | None | |
| D3 | GP4 | Bidirectional | None | |
| D4 | GP5 | Bidirectional | None | |
| D5 | GP6 | Bidirectional | None | |
| D6 | GP7 | Bidirectional | None | |
| D7 | GP8 | Bidirectional | None | |
| **WD1770 Control Signals** | | | | |
| A0 | GP9 | Input | 10k to 3.3V | Register address bit 0 |
| A1 | GP10 | Input | 10k to 3.3V | Register address bit 1 |
| CS | GP11 | Input | 10k to 3.3V | Chip select (active low) |
| R/W | GP12 | Input | 10k to 3.3V | HIGH=read, LOW=write |
| INTRQ | GP13 | Output | None | Interrupt request |
| DRQ | GP14 | Output | None | Data request |
| DDEN | GP15 | Input | 10k to 3.3V | Density enable (active low) |
| DS0 | GP16 | Input | 10k to 3.3V | Drive select 0 |
| DS1 | GP17 | Input | 10k to 3.3V | Drive select 1 |
| **SD Card (SPI0)** | | | | |
| CS | GP21 | Output | None | SPI0 CSn (native) |
| SCK | GP18 | Output | None | SPI0 SCK (native) |
| MOSI | GP19 | Output | None | SPI0 TX (native) |
| MISO | GP20 | Input | None | SPI0 RX (native) |
| **OLED Display (Software I2C)** | | | | |
| SCL | GP22 | Bidirectional | 4.7k to 3.3V | I2C clock (bit-bang) |
| SDA | GP24 | Bidirectional | 4.7k to 3.3V | I2C data (bit-bang) |
| **User Interface** | | | | |
| BTN_SELECT | GP23 | Input | Internal | On-board KEY button |
| BTN_UP | GP26 | Input | Internal | Active low button |
| BTN_DOWN | GP27 | Input | Internal | Active low button |
| LED | GP25 | Output | None | On-board blue LED |
| **Reserved/Avoid** | | | | |
| PSRAM/Flash CS | GP0 | - | - | Optional PSRAM/footprint on underside |
| SWDIO | GP28 | - | - | SWD debug |
| SWDCLK | GP29 | - | - | SWD debug |

### Reserved/Avoid Pins

| GPIO | Reason |
|------|--------|
| GP0 | Optional PSRAM/flash CS footprint on underside of board — leave unallocated |
| GP28 | SWDIO (programming) |
| GP29 | SWDCLK (programming) |

**Note:** On the WeAct RP2350B Core, GP23 is the on-board KEY button and GP25 is the on-board blue LED. USB D-/D+ are on dedicated USB pads (not GPIO). RP2350 also exposes a dedicated QSPI flash interface on non-GPIO pins; the board's W25Q128JV flash uses those dedicated pads and does not consume GPIO 1-8.

### Free Pins Available for Expansion

```
GP30-GP47 - Available (on header, RP2350B-specific high GPIOs)
```

### PIO Pin Constraints

The RP2350B has 48 GPIOs, but each PIO instance can only address 32 pins at a time:
- **PIO base 0:** GPIO 0-31
- **PIO base 16:** GPIO 16-47

Our data bus (GP1-GP8) uses PIO0 with base 0. This is fine — all data bus pins are in range 0-31.

---

## Wiring Guide

### Power Distribution
```
WeAct 3.3V  -> Breadboard 3.3V rail
WeAct GND   -> Breadboard GND rail
```

**CRITICAL:** All components MUST use 3.3V:
- OLED: 3.3V (5V will destroy it)
- SD card module: 3.3V

**Current budget:** The RP2350B can source ~12mA per GPIO (max 100mA total across all GPIOs). The 3.3V regulator on the WeAct board can supply ~300mA. Total system draw:

| Component | Typical | Peak |
|-----------|---------|------|
| RP2350B MCU | ~30mA | ~100mA |
| OLED display | ~20mA | ~30mA |
| SD card module | ~30mA | ~100mA |
| External pull-ups | ~5mA | ~5mA |
| **Total** | **~85mA** | **~235mA** |

This is within the board's 3.3V regulator capacity.

### WD1770 Bus Interface Connection

```
Timex FDD 3000                WeAct RP2350B Core
──────────────                ──────────────────
DB0        ──────────────────> GP1  (D0)
DB1        ──────────────────> GP2  (D1)
DB2        ──────────────────> GP3  (D2)
DB3        ──────────────────> GP4  (D3)
DB4        ──────────────────> GP5  (D4)
DB5        ──────────────────> GP6  (D5)
DB6        ──────────────────> GP7  (D6)
DB7        ──────────────────> GP8  (D7)

A0         ─────┬────────────> GP9  (A0)
                └─ 10k ── 3.3V

A1         ─────┬────────────> GP10 (A1)
                └─ 10k ── 3.3V

/CS        ─────┬────────────> GP11 (CS)
                └─ 10k ── 3.3V

R/W        ─────┬────────────> GP12 (R/W)
                └─ 10k ── 3.3V

/INTRQ     <────────────────── GP13 (INTRQ)

/DRQ       <────────────────── GP14 (DRQ)

/DDEN      ─────┬────────────> GP15 (DDEN)
                └─ 10k ── 3.3V

DS0        ─────┬────────────> GP16 (DS0)
                └─ 10k ── 3.3V

DS1        ─────┬────────────> GP17 (DS1)
                └─ 10k ── 3.3V
```

**Pull-up resistors:** 10kOhm to 3.3V on all input control signals (A0, A1, CS, R/W, DDEN, DS0, DS1).

### SD Card Module Connection

```
SD Card Module               WeAct RP2350B Core
──────────────               ──────────────────
VCC         ──────────────── 3.3V
GND         ──────────────── GND
CS          <──────────────── GP21 (SPI0 CSn)
SCK         <──────────────── GP18 (SPI0 SCK)
MOSI        <──────────────── GP19 (SPI0 TX)
MISO        ────────────────> GP20 (SPI0 RX)
```

**Capacitor recommended:** 10uF across VCC/GND near SD card module for stable power.

### OLED Display Connection

```
OLED Module                  WeAct RP2350B Core
──────────────               ──────────────────
VCC         ──────────────── 3.3V
GND         ──────────────── GND
SCL         <──────────────── GP22 (I2C clock, software)
SDA         <──────────────── GP24 (I2C data, software)
```

**Pull-ups:** 4.7kOhm resistors from SCL and SDA to 3.3V (optional for short wires, recommended for >100mm).

### Push Button Connection

```
BTN_SELECT one terminal -> GP23 (on-board KEY button)
BTN_UP one terminal     -> GP26
BTN_DOWN one terminal   -> GP27
All other terminals     -> GND
```

Internal pull-ups are enabled in firmware; no external resistors needed.

**Optional:** 100nF ceramic capacitors from each signal pin to GND for hardware debouncing.

---

## Electrical Specifications

### GPIO Characteristics (RP2350B)

| Parameter | Value |
|-----------|-------|
| Logic high voltage (VOH) | > 2.1V (at 12mA source) |
| Logic low voltage (VOL) | < 0.48V (at 12mA sink) |
| Input high voltage (VIH) | > 1.8V (VDDIO * 0.6) |
| Input low voltage (VIL) | < 0.8V (VDDIO * 0.3) |
| Max source current per pin | 12mA |
| Max sink current per pin | 12mA |
| Max total current (all pins) | 100mA |
| Internal pull-up resistance | ~47kOhm (weak), ~4.7kOhm (strong) |
| GPIO drive strength | Configurable: 2mA, 4mA, 8mA, 12mA |

### Signal Timing

| Signal | Min Period | Notes |
|--------|------------|-------|
| WD1770 bus cycle | ~250ns (Z80 at 4MHz) | PIO captures at 150MHz — plenty of margin |
| SPI SD card clock | 62.5ns (16MHz) | Hardware SPI handles timing |
| I2C OLED (bit-bang) | ~10us per bit | ~100kHz effective I2C speed |
| Button debounce | 50ms | Software debounce in firmware |

### RP2350B vs STM32F411 Comparison

| Parameter | STM32F411 | RP2350B | Advantage |
|-----------|-----------|---------|-----------|
| Clock speed | 100 MHz | 150 MHz | 1.5x faster |
| Flash | 512 KB | 16 MB | 32x more |
| RAM | 128 KB | 520 KB | 4x more |
| GPIO count | 36 | 48 | 12 more |
| PIO blocks | 0 | 3 (12 SMs) | Hardware bus emulation |
| SPI controllers | 2 | 2 | Same |
| I2C controllers | 3 | 2 | -1 |
| USB | 2.0 FS | 1.1 | Similar |
| Price | ~$5 | ~$4 | Cheaper |
| Package | QFN-48 | QFN-80 | More pins |

---

## Troubleshooting

### OLED Issues

**Symptom:** No display
- Check power: Must be 3.3V, measure at OLED pins
- Check address: Try both 0x3C and 0x3D in code
- Check driver: Must be SH1106, not SSD1306
- Check wiring: SDA=GP24, SCL=GP22

**Symptom:** Garbage display
- Wrong driver (SSD1306 vs SH1106)
- Poor signal integrity (add pull-ups, shorten wires)
- Software I2C timing too fast (increase delays in u8g2_setup.c)

### SD Card Issues

**Symptom:** Not detected
- Check format: Must be FAT32
- Check card type: Use Class 4-10, avoid UHS-II
- Check power: Needs stable 3.3V, add 10uF capacitor
- Try different card: Some cards are incompatible
- Check SPI wiring: SCK=GP18, MOSI=GP19, MISO=GP20, CS=GP21

**Symptom:** Random failures
- Power supply noise (add capacitor)
- SPI wiring too long (keep <100mm)
- SPI clock too high (reduce baud_rate in hw_config.c)

### Button Issues

**Symptom:** No response
- Check wiring: BTN_UP=GP26, BTN_DOWN=GP27, BTN_SELECT=GP23 (on-board KEY)
- Verify button connects GPIO pin to GND when pressed
- Note: first press wakes screensaver; second press activates menu
- Add hardware debouncing (100nF capacitors from pin to GND)

### Bus Interface Issues

**Symptom:** Target system doesn't boot
- Missing pull-ups (10kOhm required on all control inputs)
- Incorrect R/W polarity (HIGH = read from FDC, LOW = write to FDC)
- Data bus conflict (check tri-state logic — RP2350B must release bus when not driving)
- Wrong chip select polarity (active LOW)

**Symptom:** Data read errors
- Bus timing too slow (check GPIO drive strength — set to 8mA or 12mA for data bus)
- PIO not capturing data (check PIO0 SM0 initialization)
- GPIO direction switching too slow (use `gpio_set_dir_masked()` for 8-pin bulk operation)

**Symptom:** PIO bus capture not working
- Verify PIO0 has enough instruction memory (read program is ~4 words)
- Check that GP1-GP8 are all within PIO0's GPIO base range (0-31)
- Ensure PIO state machine is enabled before bus transaction

### USB Debug Issues

**Symptom:** No printf output
- Check that USB CDC is enabled in CMakeLists.txt (`pico_enable_stdio_usb`)
- Wait for USB enumeration (~1 second after boot)
- Use `stdio_init_all()` in main() before any printf calls

---

## References

**Datasheets:**
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [RP2350B Pinout](https://datasheets.raspberrypi.com/rp2350/pinout.pdf)
- [WD1770 Datasheet](http://pdf.datasheetcatalog.com/datasheet/westerndigital/WD1770-00.pdf)
- [SH1106 Datasheet](https://www.velleman.eu/downloads/29/infosheets/sh1106_datasheet.pdf)

**SDK Documentation:**
- [Pico SDK C/C++ API](https://rptl.io/pico-c-sdk)
- [Pico SDK Examples](https://github.com/raspberrypi/pico-examples)

**Additional Documentation:**
- See README.md for software setup and build instructions
- See source code for implementation details

---

**Last Updated:** September 2026
**Hardware Revision:** Port from STM32F411 v1.0 to RP2350B (untested — awaiting hardware validation)
