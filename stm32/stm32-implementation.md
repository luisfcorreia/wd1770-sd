# STM32 Implementation (Retired / Reference Only)

Earlier implementation of the WD1770 emulator targeting an STM32F411 "Black Pill" with Arduino. Kept for reference; the active implementation is the RP2350B port in `rp2350b/` (see AGENTS.md).

## Original Overview

- An STM32F411 replaces the WD1770/WD1772 chip and reads disk images from an SD card instead of physical floppy drives
- Arduino IDE compatibility required; compiles with Arduino 1.8.13+
- Architecture decisions at the time:
  - WD1770 CONTROLLER replacement, NOT a floppy drive emulator
  - NO physical floppy interface pins (STEP, DIRC, TR00, INDEX, MOTOR, etc.)
  - NO hardware drive select pins (DS0/DS1) — drive switching via 3-button UI only (this changed in the RP2350B port, which does read DS0/DS1 from the host)

## Hardware Constraints (STM32)

- Target: STM32F411 "Black Pill" board
- NO SPI.setMOSI/setMISO/setSCLK calls (not supported)
- STM32F411 does NOT have PC0, PC1, PC14, PC15 pins available

## Pin Assignments (STM32 — FIXED, do not change)

**Data Bus:** PB0-PB7 (8 pins)
**CPU Interface:** PA8 (A0), PA9 (A1), PA10 (CS), PB15 (R/W)
**FDC Control:** PA15 (INTRQ), PB8 (DRQ), PB9 (DDEN)
**SD Card SPI:** PA4 (CS), PA5 (SCK), PA6 (MISO), PA7 (MOSI)
**OLED I2C:** PA3 (SCL), PB14 (SDA)
**UI:** PC13 (LED), PA0 (BTN_UP), PA1 (BTN_DOWN), PA2 (BTN_SELECT)

**Reserved/Avoid**
- USB D- PA11, USB D+ PA12 (do not use as GPIO)
- SWDIO PA13, SWDCLK PA14 (SWD programming)
- LSE_IN PC14, LSE_OUT PC15 (oscillator — causes crashes)

## Status (as of retirement)

- Code compiled successfully
- Dual drive support implemented
- OLED UI implemented with 3-button navigation
- OLED screensaver: blanks after 30s idle, any button wakes
- DEBUG_SERIAL define in Hardware.h controls all serial output
- Hardware tested and working (UI, SD card, OLED)
- FDC bus interface ready for testing with real hardware (never production-verified)

## Sketches

- Active firmware (at the time): `wd1770/wd1770.ino` (modular, multi-file)
- Legacy: `archive/wd1770-emu/wd1770-emu.ino` (monolithic sketch kept for reference only)

## Legacy Checklist (when working on the STM32 code)

1. Always check out a fresh new copy from the repo
2. Don't assume old artifacts are current
3. Always work from latest code user provides
4. Verify pin assignments haven't drifted
5. Check that diskImages[] is still declared