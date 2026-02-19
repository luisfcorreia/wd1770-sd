/*
   WD1770 Drop-in Replacement with SD Card Support
   (Implemented with STM32F411ECU6 "Black Pill")

   LIBRARY REQUIREMENTS:
   (installed via Arduino Library Manager)
   - U8g2 library 
   - SdFat library
   - Display driver: SH1106 128x64
   
   ARCHITECTURE:
   - Modular design with separate files for each subsystem
   - DiskImage.h: Disk image data structures
   - DiskManager: Disk file operations and format detection
   - FdcDevice: WD1770 emulation logic
   - OledUI: User interface and display
   
   TEST MODE:
   - Set TEST_MODE=1 to simulate FDC signals without connecting to real hardware
   - Set TEST_MODE=0 when connecting to actual Timex or other system
*/

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>

// Include all modules
#include "Hardware.h"
#include "DiskImage.h"
#include "DiskManager.h"
#include "FdcDevice.h"
#include "OledUI.h"

// ===================== CONFIGURATION =====================

// Test mode - simulates Timex system signals
int TEST_MODE = 1;  // Set to 0 when connecting to real hardware

// ===================== GLOBAL OBJECTS =====================

SdFat32 SD;
DiskManager diskManager;
FdcDevice fdcDevice;
OledUI ui;

// ===================== INITIALIZATION =====================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("WD1770 SD Card Emulator");
  Serial.println("Modular Architecture");
  
  // Initialize pins first
  initPins();
  
  // Initialize SD card
  if (!initSDCard()) {
    Serial.println("FATAL: SD Card initialization failed!");
    while (1) {
      digitalWrite(PIN_LED, !digitalRead(PIN_LED));
      delay(100);
    }
  }
  
  // Initialize UI (OLED)
  if (!ui.begin()) {
    Serial.println("Warning: OLED initialization failed");
  }
  
  // Initialize disk manager
  diskManager.begin(&SD);
  diskManager.scanImages();
  
  // Load last configuration or defaults
  diskManager.loadConfig();
  
  // If no images loaded, load defaults
  if (diskManager.getLoadedIndex(0) == -1 && diskManager.getTotalImages() > 0) {
    Serial.println("First boot - loading default images");
    diskManager.loadImage(0, 0);
    if (diskManager.getTotalImages() > 1) {
      diskManager.loadImage(1, 1);
    }
    diskManager.saveConfig();
  }
  
  // Initialize FDC
  fdcDevice.begin();
  fdcDevice.setDiskManager(&diskManager);
  fdcDevice.setSD(&SD);
  
  // Link UI to subsystems
  ui.setDiskManager(&diskManager);
  ui.setFdcDevice(&fdcDevice);
  
  // Initial display update
  ui.updateDisplay();
  
  Serial.println("Ready!");
  Serial.println("Safe to reset/power off anytime EXCEPT during 'Saving config...' message");
}

// ===================== MAIN LOOP =====================

void loop() {
  // UI always runs regardless of FDC enable state
  ui.checkInput();
  
  // Check if FDC is enabled (DDEN signal)
  if (fdcDevice.isEnabled()) {
    // Update which drive is selected
    fdcDevice.checkDriveSelect();
    
    // Handle WD1770 bus transactions
    fdcDevice.handleBus();
    
    // Process FDC state machine
    fdcDevice.processStateMachine();
    
    // Update output signals (INTRQ, DRQ)
    fdcDevice.updateOutputs();
  } else {
    // FDC disabled - release data bus if needed
    fdcDevice.disable();
  }
  
  // Periodic display update (100ms interval)
  ui.periodicUpdate();
}

// ===================== PIN INITIALIZATION =====================

void initPins() {
  // LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  
  // Data bus (start as inputs)
  for (int i = 0; i < 8; i++) {
    pinMode(dataPins[i], INPUT);
  }
  
  // Address lines
  pinMode(WD_A0, INPUT);
  pinMode(WD_A1, INPUT);
  
  // Control signals
  pinMode(WD_CS, INPUT);
  pinMode(WD_RW, INPUT);
  
  // Output signals
  pinMode(WD_INTRQ, OUTPUT);
  pinMode(WD_DRQ, OUTPUT);
  digitalWrite(WD_INTRQ, LOW);
  digitalWrite(WD_DRQ, LOW);
  
  // Input signals (with pull-downs for test mode)
  if (TEST_MODE) {
    pinMode(WD_DDEN, INPUT_PULLDOWN);  // Simulate enabled
    pinMode(WD_DS0, INPUT_PULLUP);     // Simulate drive 0 selected
    pinMode(WD_DS1, INPUT_PULLDOWN);   // Simulate drive 1 not selected
  } else {
    pinMode(WD_DDEN, INPUT);
    pinMode(WD_DS0, INPUT);
    pinMode(WD_DS1, INPUT);
  }
  
  // Button inputs
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
}

// ===================== SD CARD INITIALIZATION =====================

bool initSDCard() {
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(10);
  
  SPI.begin();
  delay(250);
  
  Serial.println("Initializing SD card...");
  
  if (!SD.begin(SD_CS_PIN, SD_SCK_MHZ(16))) {
    Serial.println("SD Card initialization failed!");
    return false;
  }
  
  Serial.println("SD Card initialized (LFN support enabled)");
  return true;
}
