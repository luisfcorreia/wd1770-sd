# Claude Instructions for WD1770-SD Project

## Project Overview
This is a WD1770/WD1772 Floppy Disk Controller replacement that uses an STM32F411 to emulate the chip and read disk images from SD card instead of physical floppy drives.

## Critical Rules

### Code Style
- NO emojis in any documentation or comments
- Clean, professional code and documentation only
- Minimal comments - code should be self-explanatory
- Use concise variable names where appropriate

### Hardware Constraints
- Target: STM32F411 "Black Pill" board
- Arduino IDE compatibility required
- Must compile with Arduino 1.8.13+
- NO SPI.setMOSI/setMISO/setSCLK calls (not supported)
- STM32F411 does NOT have PC0, PC1, PC14, PC15 pins available

### Architecture Decisions
- This is a WD1770 CONTROLLER replacement, NOT a floppy drive emulator
- NO physical floppy interface pins (STEP, DIRC, TR00, INDEX, MOTOR, etc.)
- NO hardware drive select pins (DS0/DS1) - drive switching via UI only
- Dual virtual drives (A: and B:) managed in software
- All drive selection done via rotary encoder UI

### Pin Assignments (FIXED - DO NOT CHANGE)
**Data Bus:** PB0-PB7 (8 pins)
**CPU Interface:** PA8 (A0), PA9 (A1), PA10 (CS), PA11 (RE), PA12 (WE)
**FDC Control:** PA15 (INTRQ), PB8 (DRQ), PB9 (DDEN)
**SD Card SPI:** PA4 (CS), PA5 (SCK), PA6 (MISO), PA7 (MOSI)
**OLED I2C:** PB10 (SCL), PB11 (SDA)
**UI:** PC13 (LED), PA0-PA2 (Rotary encoder)

### Supported Disk Formats
- 720KB: 80 tracks, 9 sectors/track, 512 bytes/sector
- 360KB: 40 tracks, 9 sectors/track, 512 bytes/sector
- 160KB Timex: 40 tracks, 16 sectors/track, 256 bytes/sector
- 320KB Timex: 40 tracks, 16 sectors/track, 256 bytes/sector (double-sided)

### Code Organization
```
Global Variables:
- FDCState fdc
- DiskImage disks[2]
- File imageFiles[2]
- String diskImages[100]  // MUST be declared
- activeDrive (which drive responds to FDC)
- uiSelectedDrive (which drive UI configures)
```

### Common Mistakes to Avoid
1. DO NOT add floppy drive interface pins
2. DO NOT use non-existent STM32 pins (PC0, PC1, PC14, PC15)
3. DO NOT forget to declare diskImages[] array
4. DO NOT use SPI pin setup functions (setMOSI, etc.)
5. DO NOT add emojis to documentation
6. DO NOT create DS0/DS1 hardware drive select

### User Preferences
- Direct, no-nonsense communication
- Focus on CODE only unless explicitly asked about documentation
- When user says "fix it" - actually fix it, don't just claim to
- Verify changes were actually applied
- User will explicitly request README updates

### Required Libraries
- Adafruit SSD1306
- Adafruit GFX Library
- Standard Arduino SD library

### Testing Checklist
Before claiming code is ready:
- [ ] Compiles without errors
- [ ] No undefined variables
- [ ] All pins are valid STM32F411 pins
- [ ] diskImages[100] is declared
- [ ] No floppy interface pins present
- [ ] No DS0/DS1 pins present

## Development Notes

### Current Status
- Code compiles successfully
- Dual drive support implemented
- OLED UI implemented
- Rotary encoder working
- NOT yet tested on real hardware

### Known Limitations
- Timing not accurate for real FDC operations
- Write operations untested
- No HFE format support yet
- Drive switching is UI-only (no hardware select)

## When Starting New Conversation
1. Always check out a fresh new copy from the repo
2. Don't assume old artifacts are current
3. Always work from latest code user provides
4. Verify pin assignments haven't drifted
5. Check that diskImages[] is still declared

## Communication Style
- Be direct and concise
- Don't apologize excessively
- When wrong, acknowledge and fix immediately
- Don't use phrases like "let me check" - just check
- Focus on solutions, not explanations of why things broke
