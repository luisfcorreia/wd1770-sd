# Hardware Implementation Details

Complete hardware specifications, pinout, and wiring instructions for the WD1770-SD emulator.

## Table of Contents
- [Hardware Components](#hardware-components)
- [Pin Mapping](#pin-mapping)
- [Wiring Instructions](#wiring-instructions)
- [Critical Requirements](#critical-requirements)
- [PCB Considerations](#pcb-considerations)

## Hardware Components

### Main Components

| Component | Specification | Notes |
|-----------|--------------|-------|
| **MCU** | STM32F411CEU6 "Black Pill" | 100MHz, 128KB Flash, 512KB RAM |
| **SD Card Module** | SPI interface | Standard module with voltage regulator |
| **SD Card** | FAT32, up to 32GB | Class 10 recommended |
| **OLED Display** | 0.96", 128x64, SSD1306 | I2C interface, 3.3V |
| **Rotary Encoder** | KY-040 or similar | With push button |
| **LED** | Any color, 3mm or 5mm | Activity indicator |
| **Resistor** | 330Ω | For LED current limiting |

### Optional Components

| Component | Value | Purpose |
|-----------|-------|---------|
| Pull-up resistors | 10kΩ | For control signals if needed |
| Decoupling caps | 100nF | VCC/GND on STM32 (often included on Black Pill) |
| I2C pull-ups | 4.7kΩ | For SDA/SCL if OLED doesn't have them |
| Encoder caps | 100nF | Hardware debouncing (optional) |

### Why STM32F411 Black Pill?

**Advantages:**
- ✅ **5V tolerant I/O** - No level shifters needed
- ✅ **100MHz clock** - Fast enough for FDC timing
- ✅ **512KB RAM** - Plenty for sector buffering
- ✅ **Affordable** - $4-8 USD from AliExpress
- ✅ **Available** - Easy to source
- ✅ **Arduino support** - STM32duino framework

**Specifications:**
- ARM Cortex-M4 @ 100MHz
- 512KB Flash, 128KB RAM
- 5V tolerant I/O (most pins)
- Hardware SPI, I2C, UART
- USB bootloader (DFU mode)

## Pin Mapping

### Complete Pin Assignment

```
┌─────────────────────────────────────────────────────────┐
│                STM32F411 Black Pill Pinout             │
│                                                         │
│  Left Side              Right Side                     │
│  5V  ●                             ● GND               │
│  3V3 ●                             ● GND               │
│  NRST●                             ● 3V3               │
│  PB9 ● DDEN                    DRQ ● PB8               │
│  PB7 ● D7                       D6 ● PB6               │
│  PB5 ● D5                       D4 ● PB4               │
│  PB3 ● D3                       D2 ● PB2               │
│  PB1 ● D1                       D0 ● PB0               │
│  PC15●                      SD MOSI ● PA7              │
│  PC14●                      SD MISO ● PA6              │
│  PC13● LED                  SD SCK  ● PA5              │
│      ●                      SD CS   ● PA4              │
│  PA15● INTRQ               (free)   ● PA3              │
│  PA12● WE                 Rotary SW ● PA2              │
│  PA11● RE                 Rotary DT ● PA1              │
│  PA10● CS                Rotary CLK ● PA0              │
│  PA9 ● A1                   (free)  ● PB15             │
│  PA8 ● A0                 OLED SDA  ● PB14             │
│                                 DS1 ● PB13             │
│                                 DS0 ● PB12             │
│           PB11 NOT EXPOSED!                            │
│                           OLED SCL  ● PB10             │
│                              BOOT1  ● PB1              │
└─────────────────────────────────────────────────────────┘
```

### WD1770 CPU Interface (Data Bus)

| Signal | Pin | Direction | Description |
|--------|-----|-----------|-------------|
| D0 | PB0 | Bidirectional | Data bus bit 0 |
| D1 | PB1 | Bidirectional | Data bus bit 1 |
| D2 | PB2 | Bidirectional | Data bus bit 2 |
| D3 | PB3 | Bidirectional | Data bus bit 3 |
| D4 | PB4 | Bidirectional | Data bus bit 4 |
| D5 | PB5 | Bidirectional | Data bus bit 5 |
| D6 | PB6 | Bidirectional | Data bus bit 6 |
| D7 | PB7 | Bidirectional | Data bus bit 7 |

### WD1770 CPU Interface (Control)

| Signal | Pin | Direction | Description |
|--------|-----|-----------|-------------|
| A0 | PA8 | Input | Address bit 0 (register select) |
| A1 | PA9 | Input | Address bit 1 (register select) |
| CS̅ | PA10 | Input | Chip Select (active low) |
| R̅E̅ | PA11 | Input | Read Enable (active low) |
| W̅E̅ | PA12 | Input | Write Enable (active low) |

### WD1770 FDC Interface

| Signal | Pin | Direction | Description |
|--------|-----|-----------|-------------|
| INTRQ | PA15 | Output | Interrupt Request |
| DRQ | PB8 | Output | Data Request |
| D̅D̅E̅N̅ | PB9 | Input | FDC Enable (active low) |
| D̅S̅0̅ | PB12 | Input | Drive Select 0 (active low) |
| D̅S̅1̅ | PB13 | Input | Drive Select 1 (active low) |

### SD Card (SPI)

| Signal | Pin | Description |
|--------|-----|-------------|
| CS | PA4 | SPI Chip Select |
| SCK | PA5 | SPI Clock |
| MISO | PA6 | SPI Master In Slave Out |
| MOSI | PA7 | SPI Master Out Slave In |

### User Interface

| Component | Pin | Description |
|-----------|-----|-------------|
| LED | PC13 | Activity indicator (onboard LED) |
| Rotary CLK | PA0 | Encoder clock signal |
| Rotary DT | PA1 | Encoder data signal |
| Rotary SW | PA2 | Encoder push button |
| OLED SDA | PB14 | I2C Data (NOT PB11!) |
| OLED SCL | PB10 | I2C Clock |

### Available Pins

| Pin | Status | Notes |
|-----|--------|-------|
| PA3 | FREE | General purpose |
| PB15 | FREE | General purpose |
| PC14 | FREE* | If not using 32kHz crystal |
| PC15 | FREE* | If not using 32kHz crystal |

## Wiring Instructions

### SD Card Module

```
SD Card Module    Black Pill
--------------    ----------
VCC          →    5V (or 3.3V, check module)
GND          →    GND
MISO         →    PA6
MOSI         →    PA7
SCK          →    PA5
CS           →    PA4
```

**Notes:**
- Most modules have onboard voltage regulator (can use 5V)
- Some modules are 3.3V only (check datasheet)
- Keep wires short (<15cm) for reliable SPI

### OLED Display

```
OLED Module    Black Pill
-----------    ----------
VCC        →   3.3V (NOT 5V!)
GND        →   GND
SCL        →   PB10
SDA        →   PB14 (NOT PB11!)
```

**Important:**
- ⚠️ **Use 3.3V only** - 5V will damage the OLED!
- PB11 is NOT exposed on Black Pill, use PB14
- Add 4.7kΩ pull-ups on SDA/SCL if needed

**Pull-up Resistors (if needed):**
```
      +3.3V
        |
       4.7kΩ
        |
  ──────┴────── SDA or SCL
```

### Rotary Encoder

```
Encoder         Black Pill
-------         ----------
GND        →    GND
+          →    3.3V
SW         →    PA2
DT         →    PA1
CLK        →    PA0
```

**Optional hardware debouncing:**
```
Encoder Pin ──┬──┐
              │  │
            100nF│
              │  │
            ──┴──┴── GND
```

### LED

```
         Black Pill PC13
               |
              ┌┴┐
           330Ω│ │
              └┬┘
               |
              ┌┴┐
           LED│ │ (Anode to resistor)
              └┬┘
               |
              GND
```

**Note:** Black Pill has onboard LED on PC13, can use that or add external.

### Connecting to Host System (WD1770 Socket)

#### Option 1: Direct Socket Replacement

Remove original WD1770 chip and insert Black Pill pins into socket:

```
WD1770 Pin    Signal    Black Pill Pin
----------    ------    --------------
1             GND       GND
2             D0        PB0
3             D1        PB1
4             D2        PB2
5             D3        PB3
6             D4        PB4
7             D5        PB5
8             D6        PB6
9             D7        PB7
10            A0        PA8
11            A1        PA9
12            CS̅        PA10
13            R̅E̅        PA11
14            W̅E̅        PA12
15            INTRQ     PA15
16            DRQ       PB8
17            D̅D̅E̅N̅      PB9
18            D̅S̅0̅       PB12
19            D̅S̅1̅       PB13
20            +5V       5V
```

**Adapter needed:** Black Pill is not DIP-28 form factor, requires adapter PCB or ribbon cable.

#### Option 2: Ribbon Cable Adapter

Use DuPont wires or custom ribbon cable:
1. Create connector that fits WD1770 socket
2. Run wires to Black Pill pins
3. More flexible for testing

## Critical Requirements

### 1. Pull-up Resistors

**Required on control signals from host:**

All input control signals need 10kΩ pull-ups to 5V:
```
Signal: CS̅, R̅E̅, W̅E̅, D̅D̅E̅N̅, D̅S̅0̅, D̅S̅1̅, A0, A1

        +5V (from host system)
         |
        10kΩ
         |
  ───────┴────── To STM32 pin
         |
    Host signal
```

**Why needed:**
- Ensures clean logic levels
- Prevents floating inputs
- Required for proper operation

### 2. Decoupling Capacitors

**Required on all VCC pins:**

Add 100nF ceramic capacitor between VCC and GND on:
- STM32 power pins (usually included on Black Pill)
- SD card module (usually included)
- OLED module (usually included)

```
VCC ──┬── Device
      │
    100nF
      │
GND ──┴──
```

### 3. Voltage Levels

**5V Tolerance:**
- ✅ All input pins (D0-D7, A0-A1, CS̅, R̅E̅, W̅E̅, D̅D̅E̅N̅, D̅S̅0̅, D̅S̅1̅) are 5V tolerant
- ✅ Can connect directly to 5V host system

**Output Levels:**
- INTRQ and DRQ output 3.3V
- Usually sufficient for TTL inputs (>2.0V)
- If needed, add level shifter for outputs

**Power:**
- Black Pill can be powered from 5V (via 5V pin or USB)
- Onboard regulator provides 3.3V for logic
- **OLED must use 3.3V** (not 5V!)

### 4. Data Bus Tristate

**Critical:** Data bus (D0-D7) must be properly tristated:
- Set to INPUT when not driving
- Set to OUTPUT only during read cycles
- Code handles this automatically

### 5. Signal Timing

**Minimum setup times:**
- Address setup before CS̅: 20ns
- Data valid after R̅E̅: 500µs (in code)
- CS̅ to R̅E̅/W̅E̅: 50ns

**Code timing:**
- Edge detection on control signals
- 500µs data hold time
- 20ms encoder debounce
- 3ms simulated sector read time

## PCB Considerations

### Layout Guidelines

**Power:**
- Wide traces for power (VCC/GND): 20mil minimum
- Decoupling caps close to IC power pins
- Star ground topology if possible

**Signal Integrity:**
- Keep data bus traces same length (±5mm)
- Route control signals away from noisy signals
- Keep SD card SPI traces short and together
- Avoid sharp bends in high-speed signals

**Grounding:**
- Large ground plane on bottom layer
- Via stitching around perimeter
- Connect all grounds at single point near power input

### Suggested PCB Form Factors

**Option 1: DIP-28 Module**
- Same footprint as WD1770 chip
- Direct socket replacement
- Requires very compact layout

**Option 2: Adapter Board**
- DIP-28 socket on one end
- Black Pill socket on board
- Easier to build and debug
- More space for components

**Option 3: Standalone Board**
- Full-size PCB with all components
- Connector to host system
- Easiest to assemble
- Best for prototyping

### Recommended Stackup

For 2-layer PCB:
```
Top Layer:     Signal traces, components
Bottom Layer:  Ground plane, return paths
```

For 4-layer PCB (better):
```
Top Layer:     Signal traces, components
Layer 2:       Ground plane
Layer 3:       Power plane (3.3V, 5V)
Bottom Layer:  Signal traces
```

## Component Sourcing

### Where to Buy

| Component | Source | Approx Price |
|-----------|--------|--------------|
| STM32F411 Black Pill | AliExpress, eBay | $4-8 |
| SD Card Module | Amazon, AliExpress | $2-5 |
| OLED Display | Amazon, AliExpress | $3-8 |
| Rotary Encoder | Amazon, AliExpress | $2-5 |
| SD Card (16GB) | Amazon, local store | $5-10 |
| Resistors, caps | Amazon, Mouser, Digikey | $5-10 |

**Total cost:** ~$25-50 USD depending on sourcing

### Alternative Components

**MCU:**
- STM32F407 (more powerful, same pins)
- STM32F103 Blue Pill (cheaper, slower, might work)
- ESP32 (different architecture, needs code changes)

**Display:**
- 1.3" OLED (128x64, larger screen)
- 0.91" OLED (128x32, smaller, cheaper)
- LCD displays (requires different driver)

**Encoder:**
- Any mechanical rotary encoder with detents
- Optical encoder (more expensive, no bounce)
- Buttons instead of encoder (less convenient)

## Power Requirements

### Power Budget

| Component | Current | Notes |
|-----------|---------|-------|
| STM32F411 | ~50-100mA | Active, varies with clock |
| SD Card | ~50-200mA | During read/write |
| OLED | ~20-30mA | Depends on pixels lit |
| LED | ~10-20mA | With 330Ω resistor |
| **Total** | **~150-350mA** | Peak during SD access |

### Power Sources

**Option 1: USB Power**
- Connect USB to Black Pill
- 5V @ 500mA available
- Sufficient for emulator
- Good for development

**Option 2: Host System 5V**
- Take 5V from host computer
- Check current capacity
- Add reverse protection diode

**Option 3: External Supply**
- 5V wall adapter
- Regulated 5V source
- Isolation from host (safer)

### Power Protection

**Recommended:**
- 5V TVS diode on power input
- 500mA polyfuse on 5V input
- Reverse protection diode

```
+5V in ──┬─── Diode ───┬─── Polyfuse ──┬─── To board
         │             │               │
        TVS           ─┴─             ─┴─
         │             │               │
GND ─────┴─────────────┴───────────────┴─────
```

## Testing and Debug

### Test Points to Include

| Signal | Purpose |
|--------|---------|
| 5V | Power supply check |
| 3.3V | Regulator output |
| GND | Ground reference |
| CS̅ | Monitor bus activity |
| R̅E̅/W̅E̅ | Check read/write cycles |
| INTRQ/DRQ | Verify FDC signals |

### Debug Headers

**Serial Debug:**
- PA9 (TX1) - Already used for A1
- PA10 (RX1) - Already used for CS̅
- Alternative: Use USB serial (Serial via USB)

**SWD Debug:**
- PA13 (SWDIO) - Keep accessible
- PA14 (SWCLK) - Keep accessible
- Add 2x5 header for ST-Link

### Common Issues

**SD Card not detected:**
- Check 3.3V power to SD module
- Verify SPI wiring
- Try different SD card
- Check CS signal

**OLED not working:**
- Verify 3.3V power (NOT 5V!)
- Check I2C address (0x3C or 0x3D)
- Test with I2C scanner
- Add pull-up resistors

**No communication with host:**
- Check all control signals connected
- Verify pull-up resistors
- Monitor signals with logic analyzer
- Check power supply

## References

### Datasheets
- [STM32F411CE Reference Manual](https://www.st.com/resource/en/reference_manual/dm00119316.pdf)
- [WD1770 Datasheet](http://www.classiccmp.org/dunfield/r/wd1770.pdf)
- [SSD1306 OLED Controller](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)

### Similar Projects
- [FlashFloppy](https://github.com/keirf/flashfloppy)
- [HxC Floppy Emulator](https://hxc2001.com/)
- [Greaseweazle](https://github.com/keirf/greaseweazle)

### Tools
- [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)
- [Arduino IDE](https://www.arduino.cc/en/software)
- [PlatformIO](https://platformio.org/)

---

**Last Updated:** 2024  
**Hardware Version:** 2.0
