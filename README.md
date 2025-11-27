# WD1770-SD: SD Card Floppy Emulator

A drop-in replacement for the Western Digital WD1770/1772 Floppy Disk Controller chip that reads disk images from SD card instead of physical floppy drives.

## Project Status

**WORK IN PROGRESS - NOT YET FUNCTIONAL**

This project is in active development. Current status:
- [ ] Basic code structure implemented
- [ ] SD card reading functional
- [ ] Register interface defined
- [ ] CPU interface timing needs work
- [ ] Real hardware testing pending
- [ ] Not yet tested with actual systems

## Project Goals

Create a hardware module that:
- Acts as a pin-compatible replacement for WD1770/1772 FDC chips
- Reads standard floppy disk images (.DSK, .ST, .IMG, HFE) from SD card
- Eliminates the need for physical floppy drives
- Supports vintage computers (Atari ST, Amiga, MSX, CPC, etc.)
- Inspired by the excellent [FlashFloppy](https://github.com/keirf/flashfloppy) project

## Hardware Requirements

### Main Components
- **STM32F411 "Black Pill"** development board (or similar STM32F4)
- **SD Card Module** (SPI interface)
- **SD Card** (FAT32 formatted, up to 32GB recommended)

### Optional Components
- **0.96" OLED Display** (I2C, 128x64) - for disk image selection
- **Rotary Encoder** with push button - for navigation
- **LED** (any color) with 330Ω resistor - activity indicator

### Why STM32F411?
- 5V tolerant I/O pins (no level shifters needed!)
- 100MHz clock speed (fast enough for FDC timing)
- Sufficient RAM for sector buffering
- Built-in SPI for SD card
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
| STEP   | PB12      | Output | Step pulse |
| DIRC   | PB13      | Output | Direction (1=in, 0=out) |
| TR00   | PB14      | Output | Track 0 indicator |
| IP     | PB15      | Output | Index Pulse |
| DDEN̅   | PB9       | Input | Double Density Enable |

### SD Card (SPI)
| Signal | STM32 Pin | Description |
|--------|-----------|-------------|
| CS     | PA4       | Chip Select |
| SCK    | PA5       | SPI Clock |
| MISO   | PA6       | Master In Slave Out |
| MOSI   | PA7       | Master Out Slave In |

### Optional UI
| Component | STM32 Pin | Description |
|-----------|-----------|-------------|
| LED       | PC13      | Activity indicator |
| Rotary CLK | PA0      | Encoder clock |
| Rotary DT  | PA1      | Encoder data |
| Rotary SW  | PA2      | Encoder button |
| OLED SDA   | PB7      | I2C Data (optional) |
| OLED SCL   | PB6      | I2C Clock (optional) |

## Wiring Notes

### Critical Hardware Requirements
1. **Pull-up resistors (10kΩ)** on: A0, A1, CS, RE, WE
2. **Decoupling capacitors (100nF)** on all STM32 VCC/GND pairs
3. **Current limiting resistor (330Ω)** for LED
4. **I2C pull-ups (4.7kΩ)** on SDA/SCL if using OLED

### 5V Tolerance
The STM32F411 has 5V tolerant inputs, so **no level shifters are required** when interfacing with 5V vintage systems. The 3.3V outputs are typically sufficient for TTL logic inputs.

## Supported Disk Image Formats

| Format | Extension | Description | Status |
|--------|-----------|-------------|--------|
| Raw Sector | .DSK, .IMG | Plain sector dump | Supported |
| Atari ST | .ST | Atari ST disk image | Supported |
| HFE | .HFE | Universal flux format | Planned |
| ADF | .ADF | Amiga disk format | Planned |

### Disk Image Sizes
The emulator auto-detects common formats:
- **720KB** (80 tracks, 9 sectors, 512 bytes) - 3.5" DD
- **360KB** (40 tracks, 9 sectors, 512 bytes) - 5.25" DD
- **160KB** (40 tracks, 8 sectors, 256 bytes) - Single density

## Installation

### 1. Prepare the Hardware
- Solder pin headers to STM32F411 Black Pill
- Connect SD card module via SPI
- Add pull-up resistors to control signals
- Add decoupling capacitors
- Wire to target system according to pin mapping

### 2. Prepare the SD Card
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
└── utility.st
```

### 3. Flash the Firmware

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

### 4. Connect to Target System
- Remove original WD1770 chip (carefully!)
- Insert into socket or use ribbon cable adapter
- Power on and test

## Testing

### Basic SD Card Test
1. Connect STM32 to USB
2. Open Serial Monitor (115200 baud)
3. Look for:
```
WD1770 SD Card Emulator
Based on FlashFloppy concept
SD Card initialized
Found: game1.st
Found: game2.dsk
Found 2 disk images
Loaded: game1.st
Ready!
```

### Register Access Test
Monitor serial output when the host system accesses registers. You should see:
```
Command: 0x00  (RESTORE)
RESTORE complete
Command: 0x80  (READ SECTOR)
READ SECTOR T:0 S:1
```

## Known Issues

1. **Timing not accurate** - May not work with all systems yet
2. **No HFE support** - Only raw sector images currently work
3. **Write operations untested** - May corrupt disk images
4. **No configuration system** - Can't change settings without reflashing
5. **Index pulse timing** - May need adjustment per system

## TODO / Roadmap

- [ ] Implement interrupt-driven data bus handling
- [ ] Add proper FDC timing delays
- [ ] HFE format parser
- [ ] Configuration file support (FF.CFG compatible)
- [ ] OLED display integration
- [ ] Multi-disk image selection via rotary encoder
- [ ] Write protection support
- [ ] PCB design (DIP-28 form factor)
- [ ] Test with real hardware (Atari ST, Amiga, MSX)
- [ ] Performance optimization
- [ ] Documentation improvements

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

## License

This project is licensed under the MIT License - see LICENSE file for details.

## Acknowledgments

- **Keir Fraser** - for FlashFloppy, the main inspiration
- **Western Digital** - for the WD1770 chip that started it all
- **STM32duino community** - for excellent Arduino support
- The vintage computing community for keeping these machines alive

## Contact

- **GitHub Issues**: [Report bugs here](https://github.com/luisfcorreia/wd1770-sd/issues)
- **Discussions**: Share ideas and ask questions in GitHub Discussions

---

**Disclaimer**: This project involves working with vintage computer hardware. Always take proper ESD precautions and backup your data. The authors are not responsible for any damage to your equipment.

**Note**: This is an educational project. While inspired by commercial products like FlashFloppy, it is built from scratch for learning purposes.
