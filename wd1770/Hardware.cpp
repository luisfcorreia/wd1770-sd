#include <Arduino.h>
#include "Hardware.h"

int WD_D0 = PB0, WD_D1 = PB1, WD_D2 = PB2, WD_D3 = PB3;
int WD_D4 = PB4, WD_D5 = PB5, WD_D6 = PB6, WD_D7 = PB7;

int WD_A0 = PA8, WD_A1 = PA9, WD_CS = PA10, WD_RW = PB15;

int WD_INTRQ = PA15, WD_DRQ = PB8;

int WD_DDEN = PB9, WD_DS0 = PB12, WD_DS1 = PB13;

int BTN_UP = PA0, BTN_DOWN = PA1, BTN_SELECT = PA2;

int OLED_SDA = PB14, OLED_SCL = PA3;

uint8_t dataPins[8] = {PB0, PB1, PB2, PB3, PB4, PB5, PB6, PB7};
