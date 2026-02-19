#pragma once

#include <U8g2lib.h>
#include "DiskImage.h"
#include "DiskManager.h"
#include "FdcDevice.h"

// UI Mode enumeration
typedef enum {
  UI_MODE_NORMAL,
  UI_MODE_SELECTING_DRIVE_A,
  UI_MODE_SELECTING_DRIVE_B,
  UI_MODE_CONFIRM
} UIMode;

// Button pins - declared as extern since defined in main
extern int BTN_UP;
extern int BTN_DOWN;
extern int BTN_SELECT;

// OLED pins - declared as extern
extern int OLED_SDA;
extern int OLED_SCL;

class OledUI {
public:
  OledUI();
  
  // Initialization
  bool begin();
  
  // Input handling
  void checkInput();
  
  // Display updates
  void updateDisplay();
  void periodicUpdate();
  
  // Link to other subsystems
  void setDiskManager(DiskManager* dm);
  void setFdcDevice(FdcDevice* fdc);
  
private:
  U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2;
  
  DiskManager* diskManager;
  FdcDevice* fdcDevice;
  
  // UI state
  UIMode uiMode;
  int tempDrive0Index;
  int tempDrive1Index;
  int tempScrollIndex;
  bool confirmYes;
  
  // Button debouncing
  unsigned long lastUpPress;
  unsigned long lastDownPress;
  unsigned long lastSelectPress;
  bool selectPressed;
  unsigned long lastDisplayUpdate;
  
  // Display modes
  void displayNormalMode();
  void displaySelectingDriveA();
  void displaySelectingDriveB();
  void displayConfirm();
  
  // Button handlers
  void handleUpButton();
  void handleDownButton();
  void handleSelectButton();
  
  // Helper functions
  void showMessage(const char* msg);
  void loadSelectedImages();
};
