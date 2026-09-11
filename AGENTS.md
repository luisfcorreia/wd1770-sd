# WD1770-SD Project - Agent Instructions

## Project Overview

This is a WD1770/WD1772 Floppy Disk Controller emulator: instead of a physical floppy drive, disk images are read from an SD card and presented over an 8-bit bus to a host computer (Timex FDD 3000 interface).

Two implementations exist:
- **Active:** RP2350B port in `rp2350b/` — pure C, Pico SDK, PIO-assisted bus, CMake build.
- **Retired:** STM32F411 "Black Pill" Arduino port (`stm32/wd1770/`) — legacy code, in compilable form. All STM32 context lives in `stm32/stm32-implementation.md`.

This is a WD1770 CONTROLLER replacement, NOT a floppy drive emulator. There are NO physical floppy interface pins (STEP, DIRC, TR00, INDEX, MOTOR). Dual virtual drives (A: and B:) are managed in software. In the RP2350B port the host selects a drive directly via the DS0/DS1 inputs (DS0 HIGH = drive 0); menus still map drive selection through the 3-button UI (BTN_UP, BTN_DOWN, BTN_SELECT).

## Critical Rules

### Code Style
- NO emojis in any documentation or comments
- Clean, professional code and documentation only
- Minimal comments - code should be self-explanatory
- Use concise variable names where appropriate

### Build Rules (RP2350B)
- Target: WeAct RP2350B Core board
- Pure C (C11), CMake, Raspberry Pi Pico SDK — NO Arduino
- Vendored deps: `pico-sdk/` (tag 2.1.1), `pico-fatfs-sd/` (FatFs + SD driver), `u8g2/` (C library, software-I2C SH1106 OLED)
- Custom board header `weact_rp2350b_core.h` selected via `PICO_BOARD=weact_rp2350b_core`. It MUST contain `// pico_cmake_set PICO_PLATFORM=rp2350` or the SDK silently targets rp2040
- `sd_init_driver()` returns `bool`; `pico-fatfs-sd` is an INTERFACE library; `hw_config.c` struct fields must match `sd_card.h`/`my_spi.h`; `u8g2_Setup_sh1106_128x64_noname_f` is used
- `test_mode = 1` in main.c pins DDEN low, DS0 high, DS1 low (bench testing without host); set to 0 for real hardware, matching the original wd1770.ino behavior

### Pin Assignments (RP2350B)
Source of truth: `rp2350b/src/hardware_config.h`. Keep README.md and HARDWARE.md pin tables in sync with it.

| Function | GPIO |
|---|---|
| Data bus D0-D7 | GP1-GP8 (consecutive, PIO IN base) |
| A0, A1, CS, R/W | GP9, GP10, GP11, GP12 |
| INTRQ, DRQ (outputs) | GP13, GP14 |
| DDEN, DS0, DS1 (inputs) | GP15, GP16, GP17 |
| SD SPI0: SCK, MOSI(TX), MISO(RX), CS | GP18, GP19, GP20, GP21 |
| OLED (soft I2C): SCL, SDA | GP22, GP24 |
| BTN_SELECT (on-board KEY), LED (on-board blue) | GP23, GP25 |
| BTN_UP, BTN_DOWN | GP26, GP27 |

**Reserved/Avoid:**
- GP0: optional PSRAM/flash CS footprint on board underside — never allocate
- GP28/GP29: SWD (SWDIO/SWDCLK)
- RP2350 USB is on dedicated pads; GP24/GP25 are ordinary GPIOs (unlike RP2040, where they double as USB D-)

### Key Technical Facts
- RP2350 SPI0 pin functions: SCK=GP18, TX/MOSI=GP19, RX/MISO=GP20
- PIO `wait pin n` is relative to the SM IN base; `wait gpio n` is absolute
- `pio/data_bus.pio` (read SM) uses `wait 0 gpio 11` — CS is hard-coded there. Changing WD_CS_PIN requires editing BOTH the .pio and hardware_config.h
- PIO SM config: read SM uses IN base = WD_DATA_BASE and JMP pin = WD_CS_PIN; write SM uses OUT base = WD_DATA_BASE
- Bus is sampled on CS falling edge; bus is released on CS high after the 500us data-valid window

### Faithful-Port Warnings (DO NOT "fix" these)
The RP2350B port is a faithful C port of the original Arduino `FdcDevice.cpp`. These behaviors are intentional:
- Command decoding in fdc.c (~line 166, masks 0xF0 / 0xE0 for RESTORE..FORCE_INT) matches the original byte-for-byte
- Drive select semantics: DS0 HIGH = drive 0, DS1 HIGH = drive 1
- `currentTrack` uint8_t STEP-OUT underflow wrap is inherited from the original
- Extended-DSK field reset in disk_mgr_load_image is intentional
- Test-mode pull directions match the original wd1770.ino

### Supported Disk Formats (both implementations)
- 720KB: 80 tracks, 9 sectors/track, 512 bytes/sector
- 360KB: 40 tracks, 9 sectors/track, 512 bytes/sector
- 160KB Timex: 40 tracks, 16 sectors/track, 256 bytes/sector
- 320KB Timex: 40 tracks, 16 sectors/track, 256 bytes/sector (double-sided)

## Development Notes

### Current Status (RP2350B port)
- Full port complete in `rp2350b/`: PIO bus capture, WD1770 core, disk manager, FatFs SD, U8g2 OLED UI, buttons, test mode
- Build currently UNVERIFIED locally (cmake/arm-none-eabi-gcc unavailable in dev environment) — has passed static review only
- Docs: `rp2350b/README.md` (build + pin table), `rp2350b/HARDWARE.md` (full pinout/wiring/troubleshooting)

## When Starting New Conversation
1. Work from the latest code on disk; don't assume built artifacts are current
2. Verify pin assignments haven't drifted from `rp2350b/src/hardware_config.h` and that README.md/HARDWARE.md match
3. Verify the `wait gpio` CS pin in `pio/data_bus.pio` matches WD_CS_PIN
4. Don't "fix" the faithful-port behaviors listed above unless the user explicitly reports a hardware bug

## Communication Style
- Be direct and concise
- Don't apologize excessively
- When wrong, acknowledge and fix immediately
- Don't use phrases like "let me check" - just check
- Focus on solutions, not explanations of why things broke

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" -> "Write tests for invalid inputs, then make them pass"
- "Fix the bug" -> "Write a test that reproduces it, then make it pass"
- "Refactor X" -> "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] -> verify: [check]
2. [Step] -> verify: [check]
3. [Step] -> verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.