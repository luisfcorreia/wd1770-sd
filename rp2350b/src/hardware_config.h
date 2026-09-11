/*
 * wd1770-sd RP2350B - Pin Definitions
 *
 * All GPIO assignments for the WD1770 emulator on WeAct RP2350B Core.
 * Data bus (GP1-GP8) must be consecutive for PIO access.
 * GP0 is intentionally left unused (optional PSRAM/flash CS footprint on
 * the underside of the WeAct RP2350B Core board).
 * GP25 (on-board blue LED) is used for LED only; GP23 is the on-board KEY.
 */

#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include "pico/stdlib.h"

/* Debug output */
#define DEBUG_SERIAL 1

#if DEBUG_SERIAL
#define DBG(...)   printf(__VA_ARGS__)
#define DBGLN(...) printf(__VA_ARGS__ "\n")
#else
#define DBG(...)
#define DBGLN(...)
#endif

/* WD1770 Data Bus - GP1..GP8 (PIO-assisted, must be consecutive) */
#define WD_DATA_BASE    1
#define WD_DATA_BITS    8
#define WD_DATA_MASK    0xFF

/* WD1770 Control Signals */
#define WD_A0_PIN       9
#define WD_A1_PIN       10
#define WD_CS_PIN       11
#define WD_RW_PIN       12

/* WD1770 Output Signals */
#define WD_INTRQ_PIN    13
#define WD_DRQ_PIN      14

/* WD1770 Input Signals */
#define WD_DDEN_PIN     15
#define WD_DS0_PIN      16
#define WD_DS1_PIN      17

/* SD Card (SPI0) */
/* RP2350 SPI0 functions: SCK=GP18, TX/MOSI=GP19, RX/MISO=GP20, CSn=GP21 */
#define SD_SPI_INST     spi0
#define SD_CS_PIN       21
#define SD_SCK_PIN      18
#define SD_MOSI_PIN     19
#define SD_MISO_PIN     20

/* OLED Display (Software I2C, SH1106) */
#define OLED_SCL_PIN    22
#define OLED_SDA_PIN    24

/* User Interface - Buttons (active low, internal pull-up) */
#define BTN_UP_PIN      26
#define BTN_DOWN_PIN    27
#define BTN_SELECT_PIN  23  /* On-board KEY button */

/* Status LED (on-board blue LED) */
#define LED_PIN         25

/* PIO assignment */
#define BUS_SM_READ     0
#define BUS_SM_WRITE    1

#endif /* HARDWARE_CONFIG_H */
