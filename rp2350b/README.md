# WD1770-SD RP2350B Port

WD1770 floppy disk controller emulator ported to WeAct RP2350B Core Board.
Pure C, Pico SDK, PIO for timing-critical bus access.

## Prerequisites

- ARM GCC cross-compiler: `gcc-arm-none-eabi`
- CMake 3.13+
- Python 3
- Raspberry Pi Pico SDK (vendored in `pico-sdk/`)

On Ubuntu/Debian:
```bash
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi python3 build-essential
```

## Building

```bash
cd rp2350b
mkdir -p build && cd build
cmake -DPICO_SDK_PATH=$(pwd)/../pico-sdk ..
make -j$(nproc)
```

The output UF2 file will be at: `build/wd1770_sd.uf2`

## Flashing

1. Hold BOOTSEL button on WeAct RP2350B Core while connecting USB
2. Copy UF2 to the RPI-RP2/RP2350 drive:
   ```bash
   cp build/wd1770_sd.uf2 /media/$USER/RP2350/
   ```
3. Board resets automatically

## Project Structure

```
rp2350b/
├── CMakeLists.txt              # Root CMake build
├── weact_rp2350b_core.h        # Custom board definition
├── pico_sdk_import.cmake       # SDK auto-download
├── pio/
│   └── data_bus.pio            # PIO program for 8-bit bus
├── src/
│   ├── main.c                  # Entry point
│   ├── hardware_config.h       # Pin definitions
│   ├── bus_io.h / .c           # GPIO + PIO bus access
│   ├── fdc.h / .c              # WD1770 emulation
│   ├── disk_manager.h / .c     # SD card image management
│   ├── disk_image.h            # Disk format data structures
│   ├── oled_ui.h / .c          # OLED display + buttons
│   ├── u8g2_setup.h / .c       # U8g2 SH1106 OLED driver glue
│   ├── soft_i2c.h / .c         # Software I2C (utility)
│   └── hw_config.c             # FatFs hardware config
├── pico-sdk/                   # Pico SDK (vendored)
├── pico-fatfs-sd/              # FatFs + SD card driver
└── u8g2/                       # U8g2 display library
```

## Pin Mapping

| Function | GPIO | Notes |
|---|---|---|
| WD_D0-D7 | GP1-GP8 | 8-bit data bus (PIO-assisted) |
| WD_A0 | GP9 | Register address |
| WD_A1 | GP10 | Register address |
| WD_CS | GP11 | Chip select (active low) |
| WD_RW | GP12 | Read/Write |
| WD_INTRQ | GP13 | Interrupt output |
| WD_DRQ | GP14 | Data request output |
| WD_DDEN | GP15 | Density enable |
| WD_DS0 | GP16 | Drive select 0 |
| WD_DS1 | GP17 | Drive select 1 |
| SD_SCK | GP18 | SPI clock |
| SD_MOSI | GP19 | SPI data out (SPI0 TX) |
| SD_MISO | GP20 | SPI data in (SPI0 RX) |
| SD_CS | GP21 | SPI chip select |
| OLED_SCL | GP22 | Software I2C |
| BTN_SELECT | GP23 | On-board KEY button |
| OLED_SDA | GP24 | Software I2C |
| LED | GP25 | On-board blue LED |
| BTN_UP | GP26 | Active low |
| BTN_DOWN | GP27 | Active low |

GP0 is reserved (optional PSRAM/flash CS footprint on the WeAct board). GP28/GP29 are SWD. See `HARDWARE.md` for full details.

## Architecture

- **Language:** Pure C (C11)
- **Framework:** Raspberry Pi Pico SDK
- **Build system:** CMake
- **PIO:** 8-bit parallel bus capture (PIO0 SM0)
- **SD Card:** FatFs via SPI0 with DMA
- **OLED:** U8g2 with software I2C bit-bang
- **Debug:** USB CDC printf

## Differences from STM32 Version

- Arduino C++ → Pure C
- SdFat → FatFs
- Bit-banged GPIO → PIO-assisted bus capture
- `digitalRead`/`digitalWrite` → `gpio_get`/`gpio_put`
- `Serial` → USB CDC `printf`
- U8g2 C++ → U8g2 C (same library, direct C API)

## Debug Mode

`test_mode = 1` in main.c enables test mode:
- DDEN pulled LOW (FDC enabled)
- DS0 pulled HIGH (drive 0 selected)
- DS1 pulled LOW (drive 1 not selected)

Set to 0 when connecting to real Timex FDD 3000 hardware.
