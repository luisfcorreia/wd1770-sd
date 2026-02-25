# WD1770-SD Floppy Disk Controller Emulator

SD card-based drop-in replacement for WD1770/1772 floppy disk controller chips, with 3-button UI and OLED display.

## Status: Hardware Tested & Working

- [WORKING] USB serial communication
- [WORKING] SWD programming
- [WORKING] OLED display (SH1106 driver)
- [WORKING] SD card (FAT32, multiple disk formats)
- [WORKING] 3-button UI with image selection
- [WORKING] Config file persistence (filename-based)
- [TESTING] FDC bus interface (ready for testing with real hardware)

## Primary Target System

**Timex FDD 3000** - Floppy disk drive for ZX Spectrum
- Format: 40 tracks, 16 sectors/track, 256 bytes/sector
- Capacity: 160KB single-sided, 320KB double-sided
- Also supports: Amstrad CPC, ZX Spectrum, Atari ST, MSX, and other systems

## Hardware Requirements

### Bill of Materials
- **STM32F411 "Black Pill"** development board
- **Micro SD card module** (SPI interface)
- **SH1106 128x64 OLED display** (I2C)
- **3x push buttons** (UP, DOWN, SELECT)
- Micro SD card (FAT32 formatted, <=32GB recommended)
- Breadboard and jumper wires for prototyping

### Pin Connections
See [PIN_ASSIGNMENTS.md](PIN_ASSIGNMENTS.md) for complete wiring details.

**Critical pins:**
- Data bus: PB0-PB7 (must be consecutive for parallel I/O)
- R/W: PB15 (single pin - HIGH=read, LOW=write)
- OLED: PB14 (SDA), PA3 (SCL) - software I2C
- SD card: PA4-PA7 (SPI1)
- Buttons: PA0 (UP), PA1 (DOWN), PA2 (SELECT)

**Avoid these pins:**
- PA11/PA12: USB D-/D+ (breaks serial)
- PA13/PA14: SWD programming pins
- PC14/PC15: LSE oscillator (causes system crashes)

## Software Setup

### Arduino IDE Configuration

1. **Install STM32 board support:**
   - Tools -> Board Manager -> Search "STM32"
   - Install "STM32 MCU based boards" by STMicroelectronics

2. **Board settings:**
   - Board: "Generic STM32F4 series" -> "BlackPill F411CE"
   - USB support: "CDC (generic Serial supersede U(S)ART)"
   - Upload method: "STM32CubeProgrammer (SWD)" or your programmer

3. **Install libraries:**
   - Sketch -> Include Library -> Manage Libraries
   - Search "U8g2" -> Install "U8g2 by oliver"
   - Search "SdFat" -> Install "SdFat by Bill Greiman"

### Upload Process

**Using SWD programmer:**
1. Connect ST-Link or compatible programmer
2. Upload sketch
3. Press RESET on Black Pill

**Using DFU mode (if bootloader configured):**
1. Hold BOOT0 button
2. Press and release RESET
3. Release BOOT0
4. Upload sketch

## Quick Start

### 1. Prepare SD Card
```
1. Format as FAT32
2. Copy .DSK or .IMG disk image files to root directory
3. Insert into SD card module
```

### 2. Test Mode (No FDC Connected)
```cpp
// In wd1770/wd1770.ino:
int TEST_MODE = 1;  // Set to 0 when connecting to real hardware
```
This allows testing UI and disk images without connecting to actual hardware.

### 3. Debug Serial Output
```cpp
// In wd1770/Hardware.h:
#define DEBUG_SERIAL 1  // Set to 0 to suppress all serial output
```

### 4. Upload Code
```
1. Open wd1770/wd1770.ino
2. Verify board settings
3. Upload
4. Open Serial Monitor (115200 baud)
```

### 5. Using the UI

**Normal mode:**
- Display shows current Drive A and Drive B images
- Display blanks after 30 seconds of inactivity (OLED screensaver)
- Any button press wakes the display

**Image selection:**
- Press SELECT -> Enter Drive A selection
- UP/DOWN -> Browse disk images
- Press SELECT -> Confirm Drive A, move to Drive B selection
- UP/DOWN -> Browse for Drive B (includes "NONE" option)
- Press SELECT -> Move to confirm screen
- UP/DOWN -> Toggle YES/NO
- Press SELECT -> Load images and return to normal mode

## Disk Image Support

### Formats Detected Automatically
- **Timex FDD 3000:** 163,840 bytes (160KB, 40T/16S/256B) - Priority format
- **Timex DS:** 327,680 bytes (320KB, 80T/16S/256B)
- **Extended DSK:** Parses geometry from header (supports Timex, Amstrad, Spectrum)
- **3.5" DD:** 737,280 bytes (720KB, 80T/9S/512B)
- **5.25" DD:** 368,640 bytes (360KB, 40T/9S/512B)
- **Amstrad CPC:** 184,320 bytes (40T/9S/512B)

### Extended DSK Format
The code fully supports Extended DSK format with header parsing:
- Reads actual geometry from Track Information Block
- Works for both standard Timex and Amstrad formats
- Auto-detects format from header signature

## Configuration Persistence

Images are saved to `/lastimg.cfg` on SD card:
- **Format:** `filename0,filename1` (actual filenames, not indices)
- **Auto-loads** on power-up
- **Survives** file renaming and reordering on SD card
- **Handles missing files** gracefully

## Serial Monitor Output

```
WD1770 SD Card Emulator
Modular Architecture
Initializing SD card...
SD Card initialized (LFN support enabled)
Found: CPM.DSK
Found: TOS.DSK
Found: GAME.DSK
Found 3 disk images
Loaded config: Drive 0=CPM.DSK, Drive 1=TOS.DSK
  Drive 0 found at index 0
  Drive 1 found at index 1
Drive 0: Loaded CPM.DSK (174080 bytes, 40T/16S/256B)
  Format: Extended DSK (Timex FDD 3000)
Drive 1: Loaded TOS.DSK (174336 bytes, 40T/16S/256B)
  Format: Extended DSK (Timex FDD 3000)
Ready!
Safe to reset/power off anytime EXCEPT during 'Saving config...' message
```

## Connecting to Real Hardware

1. **Set TEST_MODE to 0** in wd1770.ino
2. **Wire FDC bus signals** per PIN_ASSIGNMENTS.md
3. **Add pull-up resistors** (10kOhm) on control signals:
   - A0, A1, CS, R/W, DDEN, DS0, DS1
4. **Power considerations:**
   - All signals are 3.3V logic
   - Target system must be 3.3V compatible or use level shifters

### Bus Protocol
- **CS (active low):** Chip select
- **R/W:** HIGH = CPU reading, LOW = CPU writing
- **A0, A1:** Register select (00=Status/Cmd, 01=Track, 10=Sector, 11=Data)
- **D0-D7:** Bidirectional data bus
- **INTRQ:** Interrupt request output (active high)
- **DRQ:** Data request output (active high)

## Troubleshooting

### OLED Shows Nothing
- Verify SH1106 chip (not SSD1306)
- Check pins: SDA=PB14, SCL=PA3
- Verify 3.3V power (NOT 5V!)
- Check DEBUG_SERIAL output for "Warning: OLED initialization failed"

### SD Card Not Detected
- Check FAT32 format (not exFAT)
- Verify wiring: CS=PA4, SCK=PA5, MISO=PA6, MOSI=PA7
- Try different SD card (older/slower cards often more reliable)

### Serial Port Not Appearing
- Verify "USB support: CDC" in Arduino IDE
- Check USB cable (must be data-capable, not charge-only)
- On Linux: Check `dmesg` for USB enumeration errors

### No Serial Output
- Verify `DEBUG_SERIAL 1` in Hardware.h

### Compile Errors
- "PIN_A0 redefined" -> Old code, update to WD_A0/WD_A1
- U8g2 errors -> Install U8g2 library
- SdFat errors -> Install SdFat library
- STM32 errors -> Update STM32 board package to latest

## Project Structure

```
wd1770/
├── wd1770.ino          - Main firmware (open this in Arduino IDE)
├── Hardware.h          - Pin definitions and DEBUG_SERIAL control
├── Hardware.cpp        - Pin variable definitions
├── DiskImage.h         - Disk image data structures
├── DiskManager.h/.cpp  - SD card file operations and format detection
├── FdcDevice.h/.cpp    - WD1770 bus emulation logic
└── OledUI.h/.cpp       - OLED display and button UI

wd1770-emu/
└── wd1770-emu.ino      - Legacy monolithic sketch (reference only)

documentation/
├── timex-fdd.md        - Timex FDD 3000 technical reference
└── timex-interface.md  - Timex bus interface details

PIN_ASSIGNMENTS.md      - Complete pinout reference
HARDWARE.md             - Hardware documentation and BOM
```

## Technical Details

### State Machine
- Non-blocking operations with proper FDC timing
- Realistic delays: 6-30ms step rates, 15ms head settle, 3ms sector I/O
- Multi-sector read/write support

### Filesystem Safety
- Files opened/closed per operation (no persistent file handles)
- SD card hot-swap safe when idle
- Power-loss resistant

### Performance
- Parallel data bus on PB0-PB7 for fast I/O
- Software I2C at ~50-100kHz (adequate for OLED)
- SPI at hardware speeds for SD card

## Known Limitations

### Not Implemented (Target System Doesn't Need)
- CRC calculation (most software ignores it)
- INDEX pulse generation
- DMA support
- Write track formatting

### To Be Tested on Real Hardware
- Bus timing with actual CPU
- Multi-sector commands in real use
- Write operations
- Different disk formats
- Fast motor speed handling

## Future Enhancements

- [ ] PCB design for compact assembly
- [ ] 3D printed enclosure
- [ ] More disk format support (HFE, etc.)
- [ ] Write protection switch
- [ ] Drive activity LED patterns
- [ ] Configuration menu (write protect, motor control)

## Credits

**Inspired by:**
- FlashFloppy - Keir Fraser's floppy emulator firmware
- Gotek hardware modifications community

**Hardware:**
- STM32F411 "Black Pill" by WeAct Studio
- U8g2 display library by Olikraus

**Target System:**
- Timex Computer 2068/3000 and FDD 3000 floppy drive

## License

This project is provided as-is for educational and hobbyist use.

## Support

For issues, questions, or contributions, refer to the documentation files included with the project.

**Last updated:** February 2026
**Version:** 1.0 - Hardware tested and working
