# WD1770-SD Hardware Documentation

Complete hardware specifications, pinout, wiring guide, and technical details for the WD1770 floppy controller emulator.

## Table of Contents
1. [Hardware Overview](#hardware-overview)
2. [Pin Assignments](#pin-assignments)
3. [Wiring Guide](#wiring-guide)
4. [Component Details](#component-details)
5. [Power Requirements](#power-requirements)
6. [Signal Specifications](#signal-specifications)
7. [PCB Considerations](#pcb-considerations)

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
- Resolution: 128×64 pixels
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

### Rotary Encoder
- Type: Standard mechanical rotary encoder with push button
- Pins: CLK (A), DT (B), SW (button), GND, +3.3V
- Detents: Any (20-30 typical)
- Built-in pull-ups: Optional (code uses internal STM32 pull-ups)

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
| CS̅ | PA10 | Chip select (active low) |
| R/W̅ | PB15 | Read/Write (HIGH=read, LOW=write) |
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
| Encoder CLK | PA0 | Rotary encoder A |
| Encoder DT | PA1 | Rotary encoder B |
| Encoder SW | PA2 | Rotary encoder button |
| LED | PC13 | On-board LED |
| **Reserved/Avoid** | | |
| USB D- | PA11 | USB (do not use as GPIO) |
| USB D+ | PA12 | USB (do not use as GPIO) |
| SWDIO | PA13 | SWD programming |
| SWDCLK | PA14 | SWD programming |
| LSE_IN | PC14 | Oscillator (causes crashes) |
| LSE_OUT | PC15 | Oscillator (causes crashes) |

### Free Pins
- **PB10** - Available for future use (previously incorrectly assigned)
- **PB11** - NOT exposed on Black Pill PCB

---

## Wiring Guide

### Breadboard Prototype Setup

#### Power Distribution
```
Black Pill 3V3 pin → Breadboard 3.3V rail
Black Pill GND pin → Breadboard GND rail
```

**CRITICAL:** All components MUST use 3.3V:
- OLED: 3.3V (5V will destroy it)
- SD card module: 3.3V
- Rotary encoder: 3.3V

#### OLED Display
```
OLED VCC  → 3.3V
OLED GND  → GND
OLED SDA  → PB14
OLED SCL  → PA3
```

**Optional pull-ups:** 4.7kΩ resistors from SDA and SCL to 3.3V improve signal integrity for long wires.

#### SD Card Module
```
SD VCC   → 3.3V
SD GND   → GND
SD CS    → PA4
SD SCK   → PA5
SD MISO  → PA6
SD MOSI  → PA7
```

**Capacitor recommended:** 10µF across VCC/GND near SD card module for stable power.

#### Rotary Encoder
```
Encoder +   → 3.3V
Encoder GND → GND
Encoder CLK → PA0
Encoder DT  → PA1
Encoder SW  → PA2
```

**Optional:** 100nF ceramic capacitors from each signal pin to GND for hardware debouncing (code already does software debouncing).

#### WD1770 Bus Interface (For Real Hardware Connection)

**Pull-up resistors required (10kΩ to 3.3V):**
```
A0    → PA8   (with 10kΩ pull-up)
A1    → PA9   (with 10kΩ pull-up)
CS̅    → PA10  (with 10kΩ pull-up)
R/W̅   → PB15  (with 10kΩ pull-up)
DDEN  → PB9   (with 10kΩ pull-up)
DS0   → PB12  (with 10kΩ pull-up)
DS1   → PB13  (with 10kΩ pull-up)
```

**Data bus (bidirectional):**
```
D0-D7 → PB0-PB7 (no pull-ups, driven by either CPU or emulator)
```

**Output signals (no pull-ups needed):**
```
INTRQ → PA15 (driven by emulator)
DRQ   → PB8  (driven by emulator)
```

### Level Shifting (If Needed)

If target system uses 5V logic:
```
Use bidirectional level shifters for:
- Data bus (D0-D7) - 8 channels
- All control signals - 5 channels minimum

Recommended: TXB0108 (8-bit bidirectional)
- 2x TXB0108 covers all signals
- Auto-direction detection
- Fast switching (25ns typical)
```

---

## Component Details

### Black Pill Variants

**Correct board:** WeAct Studio STM32F411CEU6
- MCU marking: STM32F411CEU6
- 25MHz crystal (not 8MHz)
- USB-C connector (newer) or micro-USB (older)

**Incompatible:** 
- STM32F401 variants (less RAM/flash)
- Generic F411 with wrong crystal frequency
- STM32F103 "Blue Pill" (completely different MCU)

### OLED Display Identification

**Correct:** SH1106 controller
```
Physical check:
- Usually has "0.96 inch" marking
- 128×64 resolution
- 4-pin I2C interface (VCC, GND, SCL, SDA)

Test: Use oled-test-u8g2.ino to verify
```

**Incorrect:** SSD1306 will initialize but show garbage or wrong offset

### SD Card Module Types

**Preferred:** Modules with onboard 3.3V regulator
- Can accept 5V power (but use 3.3V anyway)
- Built-in level shifters
- More reliable

**Basic modules:** Direct connection types
- No regulator (3.3V only)
- May need pull-up resistors on MISO
- Cheaper but less reliable

**SD Card Selection:**
- Class 4 or Class 10 recommended (not UHS-II)
- 1GB to 32GB size range
- Name-brand cards more reliable
- Older/slower cards often work better

---

## Power Requirements

### Current Draw

| Component | Idle | Active | Peak |
|-----------|------|--------|------|
| STM32F411 | 15mA | 40mA | 80mA |
| OLED Display | 10mA | 20mA | 25mA |
| SD Card | 5mA | 40mA | 100mA |
| Rotary Encoder | <1mA | <1mA | <1mA |
| **Total** | **30mA** | **100mA** | **200mA** |

### Power Sources

**USB Power (Recommended for development):**
- 5V from USB, Black Pill regulator provides 3.3V
- 500mA available (plenty)
- Stable and reliable

**External 3.3V Supply:**
- Connect to 3V3 pin on Black Pill
- Must provide >200mA
- Bypass Black Pill regulator

**Battery Power:**
- 3.7V LiPo → 3.3V regulator (LM1117-3.3 or better)
- Need >300mA regulator for safety margin
- Add 100µF capacitor on output

---

## Signal Specifications

### WD1770 Bus Timing

Based on real WD1770 datasheet:

**Setup times:**
- Address setup: 50ns min
- Data setup (write): 100ns min
- CS̅ to R/W̅: 0ns min

**Hold times:**
- Address hold: 10ns min
- Data hold (write): 10ns min

**Pulse widths:**
- CS̅ low: 500ns min
- R/W̅ stable during CS̅: Required

**STM32F411 @ 100MHz:**
- Clock period: 10ns
- GPIO toggle: ~20-30ns (fast enough)
- Can meet all timing requirements

### Register Select Decoding

| A1 | A0 | Register | Read | Write |
|----|----|----|------|-------|
| 0 | 0 | 0 | Status | Command |
| 0 | 1 | 1 | Track | Track |
| 1 | 0 | 2 | Sector | Sector |
| 1 | 1 | 3 | Data | Data |

### DRQ and INTRQ Timing

**DRQ (Data Request):**
- Asserted: 3ms after sector read starts
- Deasserted: After last byte read
- Purpose: Handshake for data transfer

**INTRQ (Interrupt Request):**
- Asserted: On command completion
- Deasserted: On status read
- Purpose: Signal command done

---

## PCB Considerations

### Recommended PCB Layout

**Layer stack:** 2-layer sufficient
- Top: Signal traces
- Bottom: Ground plane with signal traces

**Component placement:**
```
[SD Card]────[Black Pill]────[OLED]
                  │
            [Rotary Encoder]
                  │
         [FDC Bus Connector]
```

**Trace widths:**
- Power (3.3V, GND): 20 mil minimum
- Signals: 10 mil
- Data bus: Match length ±5mm

**Keep short (< 50mm):**
- SPI traces to SD card
- I2C traces to OLED
- Encoder traces

**Can be longer:**
- FDC bus signals (have pull-ups)
- Power traces

### Connector Suggestions

**FDC Bus:** 
- 2×17 pin header (34-pin floppy connector compatible)
- Or: 2×20 IDC for all signals + power

**Power:**
- Barrel jack (5V input) or
- USB-C connector or
- JST connector for battery

### Heat Considerations

**No heat management needed:**
- All components run cool
- STM32 ~40°C typical
- No heatsinks required

---

## Testing and Validation

### Minimal Test Setup

Before connecting to real hardware, validate:

1. **Power test:** 3.3V stable, <100mV ripple
2. **OLED test:** Run oled-test-u8g2.ino
3. **SD card test:** Run sd-card-test.ino
4. **Encoder test:** Verify rotation and button
5. **USB serial:** Verify output appears

### With Real Hardware

1. **Set TEST_MODE = 0** in code
2. **Verify voltage levels:** 3.3V logic or add level shifters
3. **Check pull-ups:** 10kΩ on all input control signals
4. **Monitor with oscilloscope:** Check data bus activity
5. **Start with read-only:** Test sector reads before writes

### Debug Tools

**Essential:**
- Multimeter (voltage check)
- USB serial monitor (Arduino IDE)

**Highly recommended:**
- Logic analyzer (check SPI, I2C, bus timing)
- Oscilloscope (verify signal quality)

**Optional:**
- ICE debugger (STLink for advanced debugging)

---

## Troubleshooting Hardware

### OLED Issues

**Symptom:** No display
- Check power: Must be 3.3V, measure at OLED pins
- Check address: Try both 0x3C and 0x3D in code
- Check driver: Must be SH1106, not SSD1306
- Check wiring: SDA=PB14, SCL=PA3

**Symptom:** Garbage display
- Wrong driver (SSD1306 vs SH1106)
- I2C too fast (use 50kHz or slower)
- Poor signal integrity (add pull-ups, shorten wires)

### SD Card Issues

**Symptom:** Not detected
- Check format: Must be FAT32
- Check card type: Use Class 4-10, avoid UHS-II
- Check power: Needs stable 3.3V, add 10µF capacitor
- Try different card: Some cards are incompatible

**Symptom:** Random failures
- Power supply noise (add capacitor)
- SPI wiring too long (keep <100mm)
- Card worn out (try new card)

### Encoder Issues

**Symptom:** No response
- Check TEST_MODE: Must not exit loop early
- Check wiring: CLK=PA0, DT=PA1, SW=PA2
- Check mechanical: Encoder may be damaged
- Add hardware debouncing (100nF capacitors)

### Bus Interface Issues

**Symptom:** Target system doesn't boot
- Voltage mismatch (need level shifters for 5V system)
- Missing pull-ups (10kΩ required)
- Incorrect R/W̅ polarity
- Data bus conflict (check tri-state logic)

---

## Bill of Materials (BOM)

| Item | Qty | Notes | Est. Cost |
|------|-----|-------|-----------|
| STM32F411 Black Pill | 1 | WeAct Studio recommended | $5 |
| SH1106 128x64 OLED | 1 | I2C, 0.96", verify SH1106 | $3 |
| SD card module | 1 | SPI interface, 3.3V | $1 |
| Rotary encoder | 1 | With push button | $1 |
| microSD card | 1 | 1-32GB, FAT32, Class 4-10 | $5 |
| 10kΩ resistors | 10 | Pull-ups for FDC bus | $0.50 |
| 4.7kΩ resistors | 2 | Optional I2C pull-ups | $0.10 |
| 100nF capacitors | 3 | Optional encoder debounce | $0.30 |
| 10µF capacitor | 1 | SD card power stability | $0.10 |
| Breadboard | 1 | For prototyping | $3 |
| Jumper wires | 40 | M-M and M-F mix | $2 |
| **Total** | | | **~$21** |

**For PCB version, add:**
- PCB fabrication (5 pcs): $5
- 34-pin FDC connector: $2
- Pin headers: $1

---

## Schematic Notes

*Note: Full KiCad schematic in development.*

**Key connections:**
```
VCC (3.3V) ────┬──> Black Pill 3V3
               ├──> OLED VCC
               ├──> SD module VCC
               └──> Encoder +

GND ───────────┬──> Black Pill GND
               ├──> OLED GND
               ├──> SD module GND
               └──> Encoder GND

Pull-ups (10kΩ each to 3.3V):
   PA8, PA9, PA10, PB15, PB9, PB12, PB13
```

---

## Mechanical Specifications

**PCB Size (recommended):** 80mm × 60mm
- Fits common project boxes
- Room for all components
- Can mount inside Timex FDD 3000 case

**Mounting holes:** M3 (3mm) in corners
**Height:** ~15mm with OLED and encoder

---

## Safety and ESD

**Static sensitive components:**
- STM32 MCU
- OLED display
- SD card

**Precautions:**
- Use ESD wrist strap when handling
- Store in anti-static bags
- Avoid touching exposed PCB traces
- Work on grounded mat if possible

**Power safety:**
- Never apply >3.6V to any STM32 pin
- OLED is 3.3V only - 5V will destroy it
- Check polarity before connecting power
- Use current-limited power supply during development

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
**Hardware Revision:** 1.0 (Tested and validated)
