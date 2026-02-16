# WD1770-SD: SD Card Floppy Emulator

A drop-in replacement for the Western Digital WD1770/1772 Floppy Disk Controller chip that reads disk images from SD card instead of physical floppy drives.

**Primary Target:** Timex FDD 3000 / Timex Computer 2068/3000

## Features

- Pin-compatible WD1770/1772 replacement
- Dual drive support (Drive 0 and Drive 1)
- SD card storage - Load .DSK images from FAT32 SD card
- OLED display - Visual feedback and image selection
- Rotary encoder - Easy disk image browsing
- State machine - Proper FDC timing and command handling
- Safe filesystem - Open/close per operation, hot-swap safe
- Persistent config - Remembers last loaded images
- Multiple formats - Timex, Amstrad, Spectrum, standard DD/HD

## Project Status

**IN DEVELOPMENT - CODE COMPLETE, HARDWARE TESTING PENDING**

- Complete code implementation with proper timing
- Extended DSK format support with header parsing
- Safe SD card handling (open/close per operation)
- Better encoder debouncing (20ms)
- Display improvements with track info
- **Awaiting real hardware testing**

## Hardware

- **MCU:** STM32F411 "Black Pill" development board
- **Storage:** SD card module (SPI), FAT32 formatted
- **Display:** 0.96" OLED (128x64, SSD1306, I2C)
- **Input:** Rotary encoder with push button
- **LED:** Activity indicator

See [HARDWARE.md](HARDWARE.md) for complete pinout, wiring, and specifications.

## Supported Disk Formats

| Format              | Size      | Tracks | Sec/Trk | Sec Size | Status      |
|---------------------|-----------|--------|---------|----------|-------------|
| **Timex FDD 3000 SS** | **160KB** | **40** | **16** | **256B** | **Primary** |
| Timex FDD 3000 DS   | 320KB     | 80     | 16      | 256B     | Supported   |
| Amstrad/Spectrum    | 170-180KB | 40     | 9       | 512B     | Supported   |
| 3.5" DD             | 720KB     | 80     | 9       | 512B     | Supported   |
| 5.25" DD            | 360KB     | 40     | 9       | 512B     | Supported   |

**Note:** Extended DSK format (with headers) supported for all formats.

## Quick Start

### 1. Hardware Assembly
See [HARDWARE.md](HARDWARE.md) for complete wiring instructions.

### 2. Prepare SD Card
```bash
# Format as FAT32
# Copy your .DSK files to the root directory
cp *.dsk /media/sdcard/
```

### 3. Flash Firmware
```bash
# Using Arduino IDE with STM32duino
# Board: Generic STM32F4 → BlackPill F411CE
# Upload Method: STM32CubeProgrammer (DFU)
```

### 4. Install
- Remove original WD1770 chip from your vintage computer
- Insert emulator (via socket or ribbon cable)
- Power on and use rotary encoder to select disk images

## Display Layout

```
┌──────────────────┐
│WD1770 Emu      ● │ ← Activity indicator
├──────────────────┤
│*>A:GAME.dsk      │ ← Drive A (* = active, > = UI)
│ T:12/79          │ ← Track info
│                  │
│  B:DATA.dsk      │ ← Drive B
│ T:--             │ ← Track (-- when inactive)
├──────────────────┤
│Img:42            │ ← Total images on SD
└──────────────────┘
```

## Controls

- **Rotate encoder:** Browse disk images
- **Short press (<1s):** Load selected image
- **Long press (>1s):** Switch between Drive A and Drive B

## File Organization

```
/
├── GAME1.DSK        ← Timex disk images
├── GAME2.DSK
├── SYSTEM.DSK
└── lastimg.cfg      ← Auto-created, stores last selection
```

## Compatible Systems

- **Timex Computer 2068/3000** (primary target)
- Amstrad CPC
- ZX Spectrum +3
- MSX
- Atari ST
- Any system using WD1770/1772 FDC

## Key Improvements Over Original Code

1. **Proper State Machine** - Non-blocking operations with realistic FDC timing
2. **Safe Filesystem** - Files closed immediately after operations
3. **Extended DSK Support** - Reads geometry from headers for all formats
4. **Better Debouncing** - 20ms encoder debounce vs 5ms
5. **Persistent Config** - `lastimg.cfg` remembers your selection
6. **Black Pill Compatible** - Fixed PB11 pin issue

## Documentation

- [HARDWARE.md](HARDWARE.md) - Complete hardware details, pinout, wiring
- [IMPROVEMENTS.md](IMPROVEMENTS.md) - Technical improvements implemented
- [TIMEX_FDD_3000.md](TIMEX_FDD_3000.md) - Timex format specifications
- [EXTENDED_DSK_FORMAT.md](EXTENDED_DSK_FORMAT.md) - Extended DSK details
- [ENCODER_DEBOUNCING.md](ENCODER_DEBOUNCING.md) - Encoder implementation
- [FILESYSTEM_SAFETY.md](FILESYSTEM_SAFETY.md) - Safe SD card handling

## Building

### Requirements
- Arduino IDE 2.x or PlatformIO
- STM32duino board support
- Libraries: Adafruit_SSD1306, Adafruit_GFX

### Installation
```bash
# Arduino IDE
1. Install STM32duino from Board Manager
2. Install Adafruit SSD1306 and GFX libraries
3. Select: Generic STM32F4 → BlackPill F411CE
4. Upload method: STM32CubeProgrammer (DFU)
5. Open wd1770-emu-improved.ino
6. Click Upload
```

## Troubleshooting

### SD Card Not Detected
- Check wiring (especially CS, SCK, MISO, MOSI)
- Verify SD card formatted as FAT32
- Try different SD card (some don't work well with SPI)

### OLED Not Working
- Check I2C address (0x3C or 0x3D)
- Verify SDA on PB14, SCL on PB10 (NOT PB11!)
- Add 4.7kΩ pull-up resistors if needed

### Display Shows "Record Not Found"
- Check disk image format matches your system
- Verify .DSK file is not corrupted
- Check serial output for geometry detection

### No Disk Activity
- Verify DS0/DS1 signals connected
- Check DDEN signal (should be LOW to enable)
- Monitor serial output for command reception

## Development

**Contributions welcome!** Areas needing help:
- Real hardware testing with vintage computers
- PCB design for production board
- Additional format support
- Performance optimization
- Documentation improvements

## References

- [WD1770 Datasheet](http://www.classiccmp.org/dunfield/r/wd1770.pdf)
- [FlashFloppy Project](https://github.com/keirf/flashfloppy) - Main inspiration
- [Extended DSK Specification](http://www.cpcwiki.eu/index.php/Format:DSK_disk_image_file_format)

## License

MIT License - See LICENSE file for details

## Acknowledgments

- **Keir Fraser** - FlashFloppy project inspiration
- **Western Digital** - WD1770 FDC chip
- **Timex Computer** - FDD 3000 floppy system
- Vintage computing community

## Author

Created with assistance from Claude (Anthropic AI)

---

**Status:** Code complete, awaiting hardware testing  
**Last Updated:** 2024  
**Version:** 2.0 (Improved)
