# WD1770-SD: SD Card Floppy Emulator

A drop-in replacement for the Western Digital WD1770/1772 Floppy Disk Controller chip that reads disk images from SD card instead of physical floppy drives.

## Project Status

**WORK IN PROGRESS - NOT YET FUNCTIONAL**

This project is in active development. Current status:
- [x] Basic code structure implemented
- [x] SD card reading functional
- [x] Register interface defined
- [x] Dual drive support (A: and B:)
- [x] OLED display integration
- [x] Rotary encoder for image selection
- [x] Multiple disk format support
- [ ] CPU interface timing needs work
- [ ] Real hardware testing pending
- [ ] Not yet tested with actual systems

## Project Goals

Create a hardware module that:
- Acts as a pin-compatible replacement for WD1770/1772 FDC chips
- Reads standard floppy disk images (.DSK, .ST, .IMG, HFE) from SD card
- Supports dual drive operation (A: and B:)
- Provides easy disk image selection via OLED and rotary encoder
- Eliminates the need for physical floppy drives
- Supports vintage computers (Atari ST, Amiga, MSX, CPC, Timex, etc.)
- Inspired by the excellent [FlashFloppy](https://github.com/keirf/flashfloppy) project

## Hardware Requirements

### Main Components
- **STM32F411 "Black Pill"** development board (or similar STM32F4)
- **SD Card Module** (SPI interface)
- **SD Card** (FAT32 formatted, up to 32GB recommended)
- **0.96" OLED Display** (I2C, 128x64, SSD1306)
- **Rotary Encoder** with push button
- **LED** (any color) with 330Ω resistor - activity indicator

### Why STM32F411?
- 5V tolerant I/O pins (no level shifters needed!)
- 100MHz clock speed (fast enough for FDC timing)
- Sufficient RAM for sector buffering
- Built-in SPI for SD card
- Hardware I2C for OLED
- Affordable (~$4-8 USD)

## Pin Mapping

### WD1770 CPU Interface
| Signal | STM32 Pin | Direction | Description |
|--------|-----------|-----------|-------------|
| D0-D7  | PB0-PB7, PB10 | Bidirectional | 8-bit data bus |
| A0     | PA8       | Input | Address bit 0 |
| A1     | PA9       | Input | Address bit 1 |
| CS̅     | PA10      | Input | Chip Select (active low) |
| R̅E̅     | PA11      | Input | Read Enable (active low) |
| W̅E̅     | PA12      | Input | Write Enable (active low) |

### WD1770 Floppy Interface
| Signal | STM32 Pin | Direction | Description |
|--------|-----------|-----------|-------------|
| INTRQ  | PA15      | Output | Interrupt Request |
| DRQ    | PB8       | Output | Data Request |

### SD Card (SPI)
| Signal | STM32 Pin | Description |
|--------|-----------|-------------|
| CS     | PA4       | Chip Select |
| SCK    | PA5       | SPI Clock |
| MISO   | PA6       | Master In Slave Out |
| MOSI   | PA7       | Master Out Slave In |

### User Interface
| Component | STM32 Pin | Description |
|-----------|-----------|-------------|
| LED       | PC13      | Activity indicator |
| Rotary CLK | PA0      | Encoder clock |
| Rotary DT  | PA1      | Encoder data |
| Rotary SW  | PA2      | Encoder button |
| OLED SDA   | PB11     | I2C Data |
| OLED SCL   | PB10     | I2C Clock |

## Wiring Notes

### Critical Hardware Requirements
1. **Pull-up resistors (10kΩ)** on: A0, A1, CS, RE, WE, DS0, DS1, DDEN
2. **Decoupling capacitors (100nF)** on all STM32 VCC/GND pairs
3. **Current limiting resistor (330Ω)** for LED
4. **I2C pull-ups (4.7kΩ)** on SDA/SCL for OLED

### 5V Tolerance
The STM32F411 has 5V tolerant inputs, so **no level shifters are required** when interfacing with 5V vintage systems. The 3.3V outputs are typically sufficient for TTL logic inputs.

## Supported Disk Image Formats

| Format | Extension | Geometry | Size | Status |
|--------|-----------|----------|------|--------|
| 3.5" DD | .DSK, .IMG | 80T/9S/512B | 720KB | Supported |
| 5.25" DD | .DSK, .IMG | 40T/9S/512B | 360KB | Supported |
| Timex FDD 3000 | .DSK | 40T/16S/256B | 160KB | Supported |
| Timex DS | .DSK | 40T/16S/256B (DS) | 320KB | Supported |
| Atari ST | .ST | Various | Various | Supported |
| HFE | .HFE | Universal flux | Various | Planned |
| ADF | .ADF | Amiga format | Various | Planned |

### Format Details
- **720KB**: Standard 3.5" double-density (80 tracks, 9 sectors/track, 512 bytes/sector)
- **360KB**: Standard 5.25" double-density (40 tracks, 9 sectors/track, 512 bytes/sector)
- **160KB**: Timex FDD 3000 single-sided (40 tracks, 16 sectors/track, 256 bytes/sector)
- **320KB**: Timex FDD 3000 double-sided (40 tracks, 16 sectors/track, 256 bytes/sector)

## User Interface

### OLED Display

**Main Screen:**
```
┌─────────────────────────┐
│ WD1770 Emulator      ● │  <- Activity indicator
├─────────────────────────┤
│*> A: game1.st          │  <- Drive indicators
│   B: system.st         │
│                         │
├─────────────────────────┤
│ Img:42  T:12  *=Act >=UI│  <- Status bar
└─────────────────────────┘
```

**Display Indicators:**
- `*` = System is currently accessing this drive (via DS0/DS1)
- `>` = UI is configuring this drive (rotary encoder controls)
- `●` = Activity indicator (blinks during disk access)

**Image Selection Menu:**
```
┌─────────────────────────┐
│ Select for Drive A      │
├─────────────────────────┤
│  game1.st              │
│  game2.st              │
│ >game3.st              │  <- Current selection
│  system.st             │
│  utility.st            │
├─────────────────────────┤
│ Turn=Sel Press=Load    │
└─────────────────────────┘
```

### Rotary Encoder Controls

- **Rotate**: Browse disk images for currently selected UI drive
- **Short Press (<1 sec)**: Load selected image into UI drive
- **Long Press (>1 sec)**: Toggle between Drive A and Drive B for UI configuration

### Independent Dual Drive Operation

The emulator maintains two separate contexts:

1. **System Active Drive** (DS0/DS1 pins)
   - Controlled by the host computer
   - Shows which drive the system is currently accessing
   - Indicated by `*` on display

2. **UI Selected Drive** (rotary encoder)
   - Controlled by you via the rotary encoder
   - Determines which drive's image you're configuring
   - Indicated by `>` on display
   - Can be different from system active drive

**Example Usage:**
1. System is reading from Drive A (DS0 active) - shows `*> A:`
2. Long-press button to switch UI to Drive B - shows `*  A:` and `  > B:`
3. Rotate encoder to browse images for Drive B (independent of system)
4. Short-press to load new image into Drive B
5. System can still be accessing Drive A throughout this process

## Installation

### 1. Required Arduino Libraries

Install via Arduino IDE Library Manager:
- **Adafruit SSD1306** (for OLED display)
- **Adafruit GFX Library** (graphics library)

### 2. Prepare the Hardware
- Solder pin headers to STM32F411 Black Pill
- Connect SD card module via SPI
- Connect OLED display via I2C
- Connect rotary encoder
- Add pull-up resistors to control signals
- Add decoupling capacitors
- Wire to target system according to pin mapping

### 3. Prepare the SD Card
```bash
# Format as FAT32
# On Linux/Mac:
sudo mkfs.vfat -F 32 /dev/sdX1

# On Windows: Use format dialog, select FAT32
```

Copy your disk images to the root directory:
```
/
├── game1.st
├── game2.dsk
├── system.img
└── utility.dsk
```

### 4. Flash the Firmware

#### Using Arduino IDE:
1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add STM32 board support:
   - File → Preferences → Additional Board Manager URLs
   - Add: `https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json`
3. Install "STM32 MCU based boards" from Board Manager
4. Select: Tools → Board → STM32 boards → Generic STM32F4 series
5. Select: Tools → Board part number → BlackPill F411CE
6. Select: Tools → Upload method → STM32CubeProgrammer (DFU)
7. Open `wd1770_emulator.ino`
8. Click Upload

#### Using PlatformIO:
```bash
pio run -t upload
```

### 5. Connect to Target System
- Remove original WD1770 chip (carefully!)
- Insert into socket or use ribbon cable adapter
- Connect DS0 and DS1 lines for dual drive support
- Power on and test

## Testing

### Basic SD Card Test
1. Connect STM32 to USB
2. Open Serial Monitor (115200 baud)
3. Look for:
```
WD1770 SD Card Emulator
Based on FlashFloppy concept
OLED display initialized
SD Card initialized
Found: game1.st
Found: game2.dsk
Found 2 disk images
Drive A: Loaded game1.st
  Tracks: 40, Sectors: 9, Size: 512
Drive B: Loaded game2.dsk
  Tracks: 80, Sectors: 9, Size: 512
Ready!
```

### OLED Display Test
- Display should show both drives and loaded images
- Rotate encoder to browse images
- Long-press to switch between Drive A and Drive B UI mode
- Short-press to load selected image

### Register Access Test
Monitor serial output when the host system accesses registers. You should see:
```
Command: 0x00  (RESTORE)
RESTORE complete
Drive switched to: A
Command: 0x80  (READ SECTOR)
READ SECTOR T:0 S:1
```

## Known Issues

1. **Timing not accurate** - May not work with all systems yet
2. **No HFE support** - Only raw sector images currently work
3. **Write operations untested** - May corrupt disk images
4. **No configuration system** - Can't change settings without reflashing
5. **Index pulse timing** - May need adjustment per system
6. **I2C pin conflict** - OLED uses PB10/PB11 which conflicts with some data bus layouts

## TODO / Roadmap

- [ ] Implement interrupt-driven data bus handling
- [ ] Add proper FDC timing delays
- [ ] HFE format parser
- [ ] Configuration file support (FF.CFG compatible)
- [ ] Write protection support via physical switch
- [ ] PCB design (DIP-28 form factor)
- [ ] Test with real hardware (Atari ST, Amiga, MSX, Timex)
- [ ] Performance optimization
- [ ] Support for more exotic formats
- [ ] Auto-detection of disk formats beyond size-based detection

## References

### WD1770 Documentation
- [WD1770 Datasheet](http://www.classiccmp.org/dunfield/r/wd1770.pdf)
- [WD177x Programming Guide](http://www.cpcwiki.eu/index.php/765_FDC)

### Similar Projects
- [FlashFloppy](https://github.com/keirf/flashfloppy) - Gotek firmware (main inspiration)
- [HxC Floppy Emulator](https://hxc2001.com/)
- [Greaseweazle](https://github.com/keirf/greaseweazle)

### STM32 Resources
- [STM32duino Documentation](https://github.com/stm32duino/wiki/wiki)
- [STM32F411 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00119316.pdf)

### Timex FDD 3000
- Timex Computer 2068/3000 documentation
- 3" Hitachi HFD305S drive specifications

## Contributing

Contributions are welcome! This is a learning project, so feel free to:
- Report bugs and issues
- Suggest improvements
- Submit pull requests
- Share test results with different systems
- Improve documentation

### Areas Needing Help
- **Hardware testing** with real vintage computers
- **Timing analysis** and optimization
- **PCB design** for professional board
- **Documentation** and tutorials
- **Format support** (HFE, ADF, etc.)
- **Timex FDD 3000** real hardware testing

## License

This project is licensed under the MIT License - see LICENSE file for details.

## Acknowledgments

- **Keir Fraser** - for FlashFloppy, the main inspiration
- **Western Digital** - for the WD1770 chip that started it all
- **STM32duino community** - for excellent Arduino support
- **Adafruit** - for the SSD1306 and GFX libraries
- The vintage computing community for keeping these machines alive

## Contact

- **GitHub Issues**: [Report bugs here](https://github.com/luisfcorreia/wd1770-sd/issues)
- **Discussions**: Share ideas and ask questions in GitHub Discussions

---

**Disclaimer**: This project involves working with vintage computer hardware. Always take proper ESD precautions and backup your data. The authors are not responsible for any damage to your equipment.

**Note**: This is an educational project. While inspired by commercial products like FlashFloppy, it is built from scratch for learning purposes.
