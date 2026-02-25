#pragma once

// Set to 1 to enable serial debug output, 0 to suppress entirely
#define DEBUG_SERIAL 1

#if DEBUG_SERIAL
  #define DBG(...)   Serial.print(__VA_ARGS__)
  #define DBGLN(...) Serial.println(__VA_ARGS__)
#else
  #define DBG(...)
  #define DBGLN(...)
#endif

// SD Card SPI pins
#define SD_CS_PIN       PA4

// WD1770 Data Bus (must be PB0-PB7 consecutive)
extern int WD_D0, WD_D1, WD_D2, WD_D3, WD_D4, WD_D5, WD_D6, WD_D7;

// WD1770 Control Signals
extern int WD_A0, WD_A1, WD_CS, WD_RW;

// WD1770 Output Signals
extern int WD_INTRQ, WD_DRQ;

// WD1770 Input Signals
extern int WD_DDEN, WD_DS0, WD_DS1;

// User Interface pins - 3 buttons
#define PIN_LED         PC13
extern int BTN_UP, BTN_DOWN, BTN_SELECT;

// OLED Display pins (Software I2C, SH1106 driver)
extern int OLED_SDA, OLED_SCL;

// Data bus pin array for efficient access
extern uint8_t dataPins[8];
