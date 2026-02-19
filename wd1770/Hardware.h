#pragma once

// SD Card SPI pins
#define SD_CS_PIN       PA4

// WD1770 Data Bus (must be PB0-PB7 consecutive)
int WD_D0 = PB0;
int WD_D1 = PB1;
int WD_D2 = PB2;
int WD_D3 = PB3;
int WD_D4 = PB4;
int WD_D5 = PB5;
int WD_D6 = PB6;
int WD_D7 = PB7;

// WD1770 Control Signals
int WD_A0 = PA8;
int WD_A1 = PA9;
int WD_CS = PA10;
int WD_RW = PB15;

// WD1770 Output Signals
int WD_INTRQ = PA15;
int WD_DRQ = PB8;

// WD1770 Input Signals
int WD_DDEN = PB9;
int WD_DS0 = PB12;
int WD_DS1 = PB13;

// User Interface pins - 3 buttons
#define PIN_LED         PC13
int BTN_UP = PA0;
int BTN_DOWN = PA1;
int BTN_SELECT = PA2;

// OLED Display pins (Software I2C, SH1106 driver)
int OLED_SDA = PB14;
int OLED_SCL = PA3;

// Data bus pin array for efficient access
uint8_t dataPins[] = {WD_D0, WD_D1, WD_D2, WD_D3, WD_D4, WD_D5, WD_D6, WD_D7};
