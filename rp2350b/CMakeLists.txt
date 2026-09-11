cmake_minimum_required(VERSION 3.13)

# Download SDK automatically if not set
if (NOT DEFINED PICO_SDK_PATH AND NOT DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_FETCH_FROM_GIT on)
endif()

include(pico_sdk_import.cmake)

project(wd1770_sd C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Use custom board definition
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR})
set(PICO_BOARD weact_rp2350b_core)

pico_sdk_init()

# --- U8g2 library ---
# Build only the C source files we need from u8g2
file(GLOB U8G2_SRC u8g2/csrc/*.c)
add_library(u8g2_lib ${U8G2_SRC})
target_include_directories(u8g2_lib PUBLIC u8g2/csrc)
target_compile_options(u8g2_lib PRIVATE
    -ffunction-sections -fdata-sections
)
set_target_properties(u8g2_lib PROPERTIES
    LINK_FLAGS "-Wl,--gc-sections"
)

# --- FatFs SD library ---
add_subdirectory(pico-fatfs-sd/src build_fatfs)

# --- Main executable ---
add_executable(wd1770_sd
    src/main.c
    src/bus_io.c
    src/fdc.c
    src/disk_manager.c
    src/oled_ui.c
    src/hw_config.c
    src/soft_i2c.c
    src/u8g2_setup.c
)

# Generate PIO header from .pio file
pico_generate_pio_header(wd1770_sd ${CMAKE_CURRENT_LIST_DIR}/pio/data_bus.pio)

target_include_directories(wd1770_sd PRIVATE
    src
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/u8g2/csrc
)

target_link_libraries(wd1770_sd PRIVATE
    pico_stdlib
    pico_stdio_usb
    hardware_spi
    hardware_pio
    hardware_gpio
    hardware_timer
    hardware_dma
    hardware_sync
    pico-fatfs-sd
    u8g2_lib
)

# Enable USB output, disable UART output
pico_enable_stdio_usb(wd1770_sd 1)
pico_enable_stdio_uart(wd1770_sd 0)

# Create UF2, BIN, HEX files
pico_add_extra_outputs(wd1770_sd)
