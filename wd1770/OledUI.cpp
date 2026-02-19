#include "OledUI.h"

#define BUTTON_DEBOUNCE_MS 50
#define DISPLAY_UPDATE_INTERVAL 100

#define OLED_FONT u8g2_font_6x10_tr

// Test mode flag - declared extern from main
extern int TEST_MODE;

OledUI::OledUI() : u8g2(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE) {
  diskManager = nullptr;
  fdcDevice = nullptr;
  uiMode = UI_MODE_NORMAL;
  tempDrive0Index = 0;
  tempDrive1Index = -1;
  tempScrollIndex = 0;
  confirmYes = true;
  lastUpPress = 0;
  lastDownPress = 0;
  lastSelectPress = 0;
  selectPressed = false;
  lastDisplayUpdate = 0;
}

bool OledUI::begin() {
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(OLED_FONT);
  u8g2.drawStr(0, 10, "WD1770 Emulator");
  u8g2.drawStr(0, 22, "Initializing...");
  u8g2.sendBuffer();
  
  return true;
}

void OledUI::setDiskManager(DiskManager* dm) {
  diskManager = dm;
}

void OledUI::setFdcDevice(FdcDevice* fdc) {
  fdcDevice = fdc;
}

void OledUI::checkInput() {
  unsigned long now = millis();
  
  // Handle UP button
  if (digitalRead(BTN_UP) == LOW) {
    if (now - lastUpPress > BUTTON_DEBOUNCE_MS) {
      lastUpPress = now;
      handleUpButton();
    }
  }
  
  // Handle DOWN button
  if (digitalRead(BTN_DOWN) == LOW) {
    if (now - lastDownPress > BUTTON_DEBOUNCE_MS) {
      lastDownPress = now;
      handleDownButton();
    }
  }
  
  // Handle SELECT button (edge detection)
  int selectState = digitalRead(BTN_SELECT);
  
  if (selectState == LOW && !selectPressed) {
    selectPressed = true;
    lastSelectPress = now;
  }
  
  if (selectState == HIGH && selectPressed) {
    unsigned long pressDuration = now - lastSelectPress;
    
    if (pressDuration >= BUTTON_DEBOUNCE_MS) {
      handleSelectButton();
    }
    
    selectPressed = false;
  }
}

void OledUI::handleUpButton() {
  if (!diskManager) return;
  
  switch (uiMode) {
    case UI_MODE_NORMAL:
      break;
      
    case UI_MODE_SELECTING_DRIVE_A:
      tempScrollIndex--;
      if (tempScrollIndex < 0) tempScrollIndex = diskManager->getTotalImages() - 1;
      Serial.print("Drive A scroll: ");
      Serial.print(tempScrollIndex);
      Serial.print(" = ");
      Serial.println(diskManager->getImageName(tempScrollIndex));
      updateDisplay();
      break;
      
    case UI_MODE_SELECTING_DRIVE_B:
      tempScrollIndex--;
      if (tempScrollIndex < -1) tempScrollIndex = diskManager->getTotalImages() - 1;
      Serial.print("Drive B scroll: ");
      Serial.print(tempScrollIndex);
      if (tempScrollIndex >= 0) {
        Serial.print(" = ");
        Serial.println(diskManager->getImageName(tempScrollIndex));
      } else {
        Serial.println(" = NONE");
      }
      updateDisplay();
      break;
      
    case UI_MODE_CONFIRM:
      confirmYes = !confirmYes;
      updateDisplay();
      break;
  }
}

void OledUI::handleDownButton() {
  if (!diskManager) return;
  
  switch (uiMode) {
    case UI_MODE_NORMAL:
      break;
      
    case UI_MODE_SELECTING_DRIVE_A:
      tempScrollIndex++;
      if (tempScrollIndex >= diskManager->getTotalImages()) tempScrollIndex = 0;
      Serial.print("Drive A scroll: ");
      Serial.print(tempScrollIndex);
      Serial.print(" = ");
      Serial.println(diskManager->getImageName(tempScrollIndex));
      updateDisplay();
      break;
      
    case UI_MODE_SELECTING_DRIVE_B:
      tempScrollIndex++;
      if (tempScrollIndex >= diskManager->getTotalImages()) tempScrollIndex = -1;
      Serial.print("Drive B scroll: ");
      Serial.print(tempScrollIndex);
      if (tempScrollIndex >= 0) {
        Serial.print(" = ");
        Serial.println(diskManager->getImageName(tempScrollIndex));
      } else {
        Serial.println(" = NONE");
      }
      updateDisplay();
      break;
      
    case UI_MODE_CONFIRM:
      confirmYes = !confirmYes;
      updateDisplay();
      break;
  }
}

void OledUI::handleSelectButton() {
  if (!diskManager) return;
  
  switch (uiMode) {
    case UI_MODE_NORMAL:
      uiMode = UI_MODE_SELECTING_DRIVE_A;
      tempScrollIndex = (diskManager->getLoadedIndex(0) >= 0) ? 
                        diskManager->getLoadedIndex(0) : 0;
      updateDisplay();
      break;
      
    case UI_MODE_SELECTING_DRIVE_A:
      tempDrive0Index = tempScrollIndex;
      Serial.print("Drive A selected: index ");
      Serial.print(tempDrive0Index);
      Serial.print(" = ");
      Serial.println(diskManager->getImageName(tempDrive0Index));
      uiMode = UI_MODE_SELECTING_DRIVE_B;
      tempScrollIndex = (diskManager->getLoadedIndex(1) >= 0) ? 
                        diskManager->getLoadedIndex(1) : -1;
      updateDisplay();
      break;
      
    case UI_MODE_SELECTING_DRIVE_B:
      tempDrive1Index = tempScrollIndex;
      Serial.print("Drive B selected: index ");
      Serial.print(tempDrive1Index);
      if (tempDrive1Index >= 0) {
        Serial.print(" = ");
        Serial.println(diskManager->getImageName(tempDrive1Index));
      } else {
        Serial.println(" = NONE");
      }
      uiMode = UI_MODE_CONFIRM;
      confirmYes = true;
      updateDisplay();
      break;
      
    case UI_MODE_CONFIRM:
      if (confirmYes) {
        loadSelectedImages();
      } else {
        uiMode = UI_MODE_SELECTING_DRIVE_A;
        tempScrollIndex = tempDrive0Index;
        updateDisplay();
      }
      break;
  }
}

void OledUI::loadSelectedImages() {
  if (!diskManager) return;
  
  // Show loading message
  showMessage("Loading images...");
  
  // Load Drive A
  Serial.print("Loading Drive A: index ");
  Serial.print(tempDrive0Index);
  Serial.print(" = ");
  Serial.println(diskManager->getImageName(tempDrive0Index));
  diskManager->loadImage(0, tempDrive0Index);
  
  // Load or eject Drive B
  if (tempDrive1Index >= 0) {
    Serial.print("Loading Drive B: index ");
    Serial.print(tempDrive1Index);
    Serial.print(" = ");
    Serial.println(diskManager->getImageName(tempDrive1Index));
    diskManager->loadImage(1, tempDrive1Index);
  } else {
    Serial.println("Loading Drive B: NONE (ejecting)");
    diskManager->ejectDrive(1);
  }
  
  // Show saving message
  showMessage("Saving config...");
  diskManager->saveConfig();
  
  delay(100);
  
  // Show completion
  showMessage("Done!");
  delay(500);
  
  uiMode = UI_MODE_NORMAL;
  updateDisplay();
}

void OledUI::showMessage(const char* msg) {
  u8g2.clearBuffer();
  u8g2.setFont(OLED_FONT);
  u8g2.drawStr(0, 32, msg);
  u8g2.sendBuffer();
}

void OledUI::updateDisplay() {
  switch (uiMode) {
    case UI_MODE_NORMAL:
      displayNormalMode();
      break;
    case UI_MODE_SELECTING_DRIVE_A:
      displaySelectingDriveA();
      break;
    case UI_MODE_SELECTING_DRIVE_B:
      displaySelectingDriveB();
      break;
    case UI_MODE_CONFIRM:
      displayConfirm();
      break;
  }
}

void OledUI::periodicUpdate() {
  unsigned long now = millis();
  if (uiMode == UI_MODE_NORMAL && now - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    displayNormalMode();
    lastDisplayUpdate = now;
  }
}

void OledUI::displayNormalMode() {
  if (!diskManager || !fdcDevice) return;
  
  char buf[32];
  u8g2.clearBuffer();
  u8g2.setFont(OLED_FONT);
  
  // Drive A info
  DiskImage* diskA = diskManager->getDisk(0);
  if (diskA && diskA->filename[0] != '\0') {
    char fname[21];
    strncpy(fname, diskA->filename, 18);
    fname[18] = '\0';
    if (strlen(diskA->filename) > 18) strcpy(fname + 15, "...");
    
    sprintf(buf, "A:%s", fname);
    u8g2.drawStr(0, 10, buf);
    
    if (fdcDevice->getActiveDrive() == 0) {
      sprintf(buf, " T:%d/%d", fdcDevice->getCurrentTrack(), diskA->tracks - 1);
    } else {
      strcpy(buf, " T:--");
    }
    u8g2.drawStr(0, 20, buf);
  } else {
    strcpy(buf, "A:(empty)");
    u8g2.drawStr(0, 10, buf);
  }
  
  // Drive B info
  DiskImage* diskB = diskManager->getDisk(1);
  if (diskB && diskB->filename[0] != '\0') {
    char fname[21];
    strncpy(fname, diskB->filename, 18);
    fname[18] = '\0';
    if (strlen(diskB->filename) > 18) strcpy(fname + 15, "...");
    
    sprintf(buf, "B:%s", fname);
    u8g2.drawStr(0, 34, buf);
    
    if (fdcDevice->getActiveDrive() == 1) {
      sprintf(buf, " T:%d/%d", fdcDevice->getCurrentTrack(), diskB->tracks - 1);
    } else {
      strcpy(buf, " T:--");
    }
    u8g2.drawStr(0, 44, buf);
  } else {
    strcpy(buf, "B:(empty)");
    u8g2.drawStr(0, 34, buf);
  }
  
  // Status line
  if (TEST_MODE) {
    u8g2.drawStr(0, 64, "TEST MODE");
    u8g2.drawStr(60, 64, "Select=Menu");
  } else {
    u8g2.drawStr(0, 64, "Press to select");
  }
  
  u8g2.sendBuffer();
}

void OledUI::displaySelectingDriveA() {
  if (!diskManager) return;
  
  char buf[32];
  u8g2.clearBuffer();
  u8g2.setFont(OLED_FONT);
  
  u8g2.drawStr(0, 8, "Select Drive A:");
  u8g2.drawHLine(0, 10, 128);
  
  // Show scrollable list
  int startIdx = max(0, tempScrollIndex - 2);
  int endIdx = min(diskManager->getTotalImages() - 1, startIdx + 4);
  
  if (endIdx - startIdx < 4 && startIdx > 0) {
    startIdx = max(0, endIdx - 4);
  }
  
  int y = 22;
  for (int i = startIdx; i <= endIdx && i < diskManager->getTotalImages(); i++) {
    char fname[24];
    const char* imgName = diskManager->getImageName(i);
    strncpy(fname, imgName, 20);
    fname[20] = '\0';
    if (strlen(imgName) > 20) strcpy(fname + 17, "...");
    
    if (i == tempScrollIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, y - 8, 128, 10);
      u8g2.setDrawColor(0);
      sprintf(buf, ">%s", fname);
      u8g2.drawStr(0, y, buf);
      u8g2.setDrawColor(1);
    } else {
      sprintf(buf, " %s", fname);
      u8g2.drawStr(0, y, buf);
    }
    y += 10;
  }
  
  u8g2.drawStr(0, 64, "Up/Down=Scroll Sel=OK");
  u8g2.sendBuffer();
}

void OledUI::displaySelectingDriveB() {
  if (!diskManager) return;
  
  char buf[32];
  u8g2.clearBuffer();
  u8g2.setFont(OLED_FONT);
  
  u8g2.drawStr(0, 8, "Select Drive B:");
  u8g2.drawHLine(0, 10, 128);
  
  // Show scrollable list including NONE option
  int startIdx = max(-1, tempScrollIndex - 2);
  int endIdx = min(diskManager->getTotalImages() - 1, startIdx + 4);
  
  if (endIdx - startIdx < 4 && startIdx > -1) {
    startIdx = max(-1, endIdx - 4);
  }
  
  int y = 22;
  
  // NONE option at index -1
  if (startIdx == -1) {
    if (tempScrollIndex == -1) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, y - 8, 128, 10);
      u8g2.setDrawColor(0);
      u8g2.drawStr(0, y, ">NONE");
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(0, y, " NONE");
    }
    y += 10;
    startIdx = 0;
  }
  
  // Regular images
  for (int i = startIdx; i <= endIdx && i < diskManager->getTotalImages(); i++) {
    char fname[24];
    const char* imgName = diskManager->getImageName(i);
    strncpy(fname, imgName, 20);
    fname[20] = '\0';
    if (strlen(imgName) > 20) strcpy(fname + 17, "...");
    
    if (i == tempScrollIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, y - 8, 128, 10);
      u8g2.setDrawColor(0);
      sprintf(buf, ">%s", fname);
      u8g2.drawStr(0, y, buf);
      u8g2.setDrawColor(1);
    } else {
      sprintf(buf, " %s", fname);
      u8g2.drawStr(0, y, buf);
    }
    y += 10;
  }
  
  u8g2.drawStr(0, 64, "Up/Down=Scroll Sel=OK");
  u8g2.sendBuffer();
}

void OledUI::displayConfirm() {
  if (!diskManager) return;
  
  char buf[32];
  u8g2.clearBuffer();
  u8g2.setFont(OLED_FONT);
  
  u8g2.drawStr(0, 8, "Load these images?");
  u8g2.drawHLine(0, 10, 128);
  
  // Drive A
  char fname[21];
  const char* imgName = diskManager->getImageName(tempDrive0Index);
  strncpy(fname, imgName, 18);
  fname[18] = '\0';
  if (strlen(imgName) > 18) strcpy(fname + 15, "...");
  sprintf(buf, "A:%s", fname);
  u8g2.drawStr(0, 22, buf);
  
  // Drive B
  if (tempDrive1Index >= 0) {
    imgName = diskManager->getImageName(tempDrive1Index);
    strncpy(fname, imgName, 18);
    fname[18] = '\0';
    if (strlen(imgName) > 18) strcpy(fname + 15, "...");
    sprintf(buf, "B:%s", fname);
  } else {
    strcpy(buf, "B:(empty)");
  }
  u8g2.drawStr(0, 32, buf);
  
  // YES/NO buttons
  if (confirmYes) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(27, 42, 35, 9);
    u8g2.setDrawColor(0);
    u8g2.drawStr(30, 50, "[YES]");
    u8g2.setDrawColor(1);
    u8g2.drawStr(75, 50, "[NO]");
  } else {
    u8g2.drawStr(30, 50, "[YES]");
    u8g2.setDrawColor(1);
    u8g2.drawBox(72, 42, 29, 9);
    u8g2.setDrawColor(0);
    u8g2.drawStr(75, 50, "[NO]");
    u8g2.setDrawColor(1);
  }
  
  u8g2.drawStr(0, 64, "Up/Down=Toggle Sel=OK");
  u8g2.sendBuffer();
}
