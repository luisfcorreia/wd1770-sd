/*
   WD1770/1772 Drop-in Replacement with SD Card Support
   For STM32F4 (e.g., STM32F407, STM32F411 "Black Pill")

   Inspired by FlashFloppy - reads disk images from SD card
   Supports standard floppy disk image formats

   Hardware Requirements:
   - STM32F4 board (e.g., Black Pill STM32F411)
   - SD card module (SPI interface)
   - OLED display (I2C)
   - Rotary encoder (for image selection)

   Required Libraries:
   - Adafruit SSD1306
   - Adafruit GFX Library
*/

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===================== CONFIGURATION =====================

// SD Card SPI pins
#define SD_CS_PIN       PA4
#define SD_MOSI_PIN     PA7
#define SD_MISO_PIN     PA6
#define SD_SCK_PIN      PA5

// WD1770 CPU Interface Pins
#define PIN_D0          PB0   // Data bus bit 0
#define PIN_D1          PB1
#define PIN_D2          PB2
#define PIN_D3          PB3
#define PIN_D4          PB4
#define PIN_D5          PB5
#define PIN_D6          PB6
#define PIN_D7          PB7

#define PIN_A0          PA8   // Address bit 0
#define PIN_A1          PA9   // Address bit 1
#define PIN_CS          PA10  // Chip Select (active low)
#define PIN_RE          PA11  // Read Enable (active low)
#define PIN_WE          PA12  // Write Enable (active low)

// WD1770 Control Outputs
#define PIN_INTRQ       PA15  // Interrupt Request (output)
#define PIN_DRQ         PB8   // Data Request (output)

// WD1770 Inputs from System
#define PIN_DDEN        PB9   // Double Density Enable / FDC Enable (active low)
#define PIN_DS0         PB12  // Drive Select 0 (active low)
#define PIN_DS1         PB13  // Drive Select 1 (active low)

// User Interface pins
#define PIN_LED         PC13  // Activity LED
#define ROTARY_CLK      PA0   // Rotary encoder
#define ROTARY_DT       PA1
#define ROTARY_SW       PA2   // Rotary switch

// I2C pins for OLED
#define OLED_SDA        PB11  // I2C Data
#define OLED_SCL        PB10  // I2C Clock
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C

// ===================== CONSTANTS =====================

// Maximum number of drives
#define MAX_DRIVES      2

// Register addresses
#define REG_STATUS_CMD  0
#define REG_TRACK       1
#define REG_SECTOR      2
#define REG_DATA        3

// Command types (Type I)
#define CMD_RESTORE     0x00
#define CMD_SEEK        0x10
#define CMD_STEP        0x20
#define CMD_STEP_IN     0x40
#define CMD_STEP_OUT    0x60

// Command types (Type II)
#define CMD_READ_SECTOR     0x80
#define CMD_READ_SECTORS    0x90
#define CMD_WRITE_SECTOR    0xA0
#define CMD_WRITE_SECTORS   0xB0

// Command types (Type III)
#define CMD_READ_ADDRESS    0xC0
#define CMD_READ_TRACK      0xE0
#define CMD_WRITE_TRACK     0xF0

// Command types (Type IV)
#define CMD_FORCE_INT       0xD0

// Status bits
#define ST_BUSY             0x01
#define ST_DRQ              0x02
#define ST_LOST_DATA        0x04
#define ST_CRC_ERROR        0x08
#define ST_RNF              0x10
#define ST_RECORD_TYPE      0x20
#define ST_WRITE_PROTECT    0x40
#define ST_NOT_READY        0x80

// Type I specific status
#define ST_INDEX            0x02
#define ST_TRACK00          0x04
#define ST_SEEK_ERROR       0x10
#define ST_HEAD_LOADED      0x20

// Disk geometry
#define MAX_TRACKS          84
#define MAX_SECTORS         18
#define SECTOR_SIZE_SD      256
#define SECTOR_SIZE_DD      512

// Timing constants (in microseconds)
#define STEP_TIME_6MS       6000
#define STEP_TIME_12MS      12000
#define STEP_TIME_20MS      20000
#define STEP_TIME_30MS      30000
#define HEAD_SETTLE_TIME    15000

// ===================== STRUCTURES =====================

typedef struct {
  uint8_t status;
  uint8_t track;
  uint8_t sector;
  uint8_t data;
  uint8_t command;

  uint8_t currentTrack;
  int8_t direction;

  bool busy;
  bool drq;
  bool intrq;
  bool doubleDensity;

  uint16_t dataIndex;
  uint16_t dataLength;
  uint8_t sectorBuffer[1024];

  uint32_t lastStepTime;
  uint32_t stepRate;

  bool writeProtect;
  bool motorOn;

} FDCState;

typedef struct {
  char filename[64];
  uint32_t size;
  uint8_t tracks;
  uint8_t sectorsPerTrack;
  uint16_t sectorSize;
  bool doubleDensity;
  bool writeProtected;
} DiskImage;

// ===================== GLOBAL VARIABLES =====================

FDCState fdc;
DiskImage disks[MAX_DRIVES];
File imageFiles[MAX_DRIVES];
uint8_t activeDrive = 0;
uint8_t dataPins[] = {PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7};

String diskImages[100];
int currentImageIndex[MAX_DRIVES] = {0, 0};
int totalImages = 0;
bool oledPresent = false;
uint8_t uiSelectedDrive = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===================== SETUP =====================

void setup() {
  Serial.begin(115200);
  Serial.println("WD1770 SD Card Emulator");
  Serial.println("Based on FlashFloppy concept");

  initPins();

  Wire.begin();
  Wire.setClock(400000);

  if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    oledPresent = true;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("WD1770 Emulator");
    display.println("Initializing...");
    display.display();
    Serial.println("OLED display initialized");
  } else {
    Serial.println("OLED display not found");
    oledPresent = false;
  }

  if (!initSDCard()) {
    Serial.println("SD Card initialization failed!");
    if (oledPresent) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("ERROR:");
      display.println("SD Card Failed!");
      display.display();
    }
    while (1) {
      digitalWrite(PIN_LED, !digitalRead(PIN_LED));
      delay(100);
    }
  }

  scanDiskImages();

  if (totalImages > 0) {
    loadDiskImage(0, 0);
    if (totalImages > 1) {
      loadDiskImage(1, 1);
    }
  }

  initFDC();
  updateDisplay();

  Serial.println("Ready!");
}

// ===================== MAIN LOOP =====================

void loop() {
  // Check if FDC is enabled (DDEN active low)
  if (digitalRead(PIN_DDEN) == HIGH) {
    // FDC disabled, tri-state outputs
    return;
  }

  // Check which drive is selected by system
  checkDriveSelect();

  if (digitalRead(PIN_CS) == LOW) {
    uint8_t addr = (digitalRead(PIN_A1) << 1) | digitalRead(PIN_A0);

    if (digitalRead(PIN_RE) == LOW) {
      handleRead(addr);
      delayMicroseconds(1);
    }
    else if (digitalRead(PIN_WE) == LOW) {
      handleWrite(addr);
      delayMicroseconds(1);
    }
  }

  updateOutputs();
  processCommand();
  checkImageSelection();

  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 100) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
}

// ===================== PIN INITIALIZATION =====================

void initPins() {
  for (int i = 0; i < 8; i++) {
    pinMode(dataPins[i], INPUT);
  }

  pinMode(PIN_A0, INPUT);
  pinMode(PIN_A1, INPUT);
  pinMode(PIN_CS, INPUT);
  pinMode(PIN_RE, INPUT);
  pinMode(PIN_WE, INPUT);
  pinMode(PIN_DDEN, INPUT_PULLUP);

  pinMode(PIN_DS0, INPUT_PULLUP);
  pinMode(PIN_DS1, INPUT_PULLUP);

  pinMode(PIN_INTRQ, OUTPUT);
  pinMode(PIN_DRQ, OUTPUT);

  pinMode(PIN_LED, OUTPUT);
  pinMode(ROTARY_CLK, INPUT_PULLUP);
  pinMode(ROTARY_DT, INPUT_PULLUP);
  pinMode(ROTARY_SW, INPUT_PULLUP);

  digitalWrite(PIN_INTRQ, LOW);
  digitalWrite(PIN_DRQ, LOW);
}

// ===================== SD CARD FUNCTIONS =====================

bool initSDCard() {
  if (!SD.begin(SD_CS_PIN)) {
    return false;
  }

  Serial.println("SD Card initialized");
  return true;
}

void scanDiskImages() {
  File root = SD.open("/");
  if (!root) {
    Serial.println("Failed to open root directory");
    return;
  }

  totalImages = 0;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory()) {
      String filename = entry.name();
      filename.toUpperCase();

      if (filename.endsWith(".DSK") ||
          filename.endsWith(".ST") ||
          filename.endsWith(".IMG") ||
          filename.endsWith(".HFE")) {
        diskImages[totalImages++] = String(entry.name());
        Serial.print("Found: ");
        Serial.println(entry.name());

        if (totalImages >= 100) break;
      }
    }
    entry.close();
  }
  root.close();

  Serial.print("Found ");
  Serial.print(totalImages);
  Serial.println(" disk images");
}

bool loadDiskImage(int driveNum, int index) {
  if (driveNum < 0 || driveNum >= MAX_DRIVES) return false;
  if (index < 0 || index >= totalImages) return false;

  if (imageFiles[driveNum]) {
    imageFiles[driveNum].close();
  }

  String path = "/" + diskImages[index];
  imageFiles[driveNum] = SD.open(path.c_str(), FILE_READ);

  if (!imageFiles[driveNum]) {
    Serial.print("Failed to open: ");
    Serial.println(path);
    return false;
  }

  currentImageIndex[driveNum] = index;
  strncpy(disks[driveNum].filename, diskImages[index].c_str(), 63);
  disks[driveNum].size = imageFiles[driveNum].size();

  if (disks[driveNum].size == 737280) {
    disks[driveNum].tracks = 80;
    disks[driveNum].sectorsPerTrack = 9;
    disks[driveNum].sectorSize = 512;
    disks[driveNum].doubleDensity = true;
  }
  else if (disks[driveNum].size == 368640) {
    disks[driveNum].tracks = 40;
    disks[driveNum].sectorsPerTrack = 9;
    disks[driveNum].sectorSize = 512;
    disks[driveNum].doubleDensity = true;
  }
  else if (disks[driveNum].size == 163840) {
    disks[driveNum].tracks = 40;
    disks[driveNum].sectorsPerTrack = 16;
    disks[driveNum].sectorSize = 256;
    disks[driveNum].doubleDensity = false;
  }
  else if (disks[driveNum].size == 327680) {
    disks[driveNum].tracks = 40;
    disks[driveNum].sectorsPerTrack = 16;
    disks[driveNum].sectorSize = 256;
    disks[driveNum].doubleDensity = false;
  }
  else {
    disks[driveNum].tracks = 80;
    disks[driveNum].sectorsPerTrack = 9;
    disks[driveNum].sectorSize = 512;
    disks[driveNum].doubleDensity = true;
  }

  disks[driveNum].writeProtected = false;

  char driveLetter = 'A' + driveNum;
  Serial.print("Drive ");
  Serial.print(driveLetter);
  Serial.print(": Loaded ");
  Serial.println(disks[driveNum].filename);

  return true;
}

// ===================== FDC INITIALIZATION =====================

void initFDC() {
  fdc.status = 0;
  fdc.track = 0;
  fdc.sector = 1;
  fdc.data = 0;
  fdc.command = 0;

  fdc.currentTrack = 0;
  fdc.direction = 1;

  fdc.busy = false;
  fdc.drq = false;
  fdc.intrq = false;
  fdc.doubleDensity = true;

  fdc.dataIndex = 0;
  fdc.dataLength = 0;

  fdc.lastStepTime = 0;
  fdc.stepRate = STEP_TIME_6MS;

  fdc.writeProtect = false;
  fdc.motorOn = false;
}

// ===================== READ/WRITE HANDLERS =====================

void handleRead(uint8_t addr) {
  uint8_t value = 0;

  switch (addr) {
    case REG_STATUS_CMD:
      value = fdc.status;
      fdc.intrq = false;
      break;

    case REG_TRACK:
      value = fdc.track;
      break;

    case REG_SECTOR:
      value = fdc.sector;
      break;

    case REG_DATA:
      value = fdc.data;
      if (fdc.drq && fdc.dataIndex < fdc.dataLength) {
        value = fdc.sectorBuffer[fdc.dataIndex++];
        if (fdc.dataIndex >= fdc.dataLength) {
          fdc.drq = false;
          fdc.status &= ~ST_DRQ;
          completeSectorRead();
        }
      }
      break;
  }

  setDataBus(value);
}

void handleWrite(uint8_t addr) {
  uint8_t value = readDataBus();

  switch (addr) {
    case REG_STATUS_CMD:
      executeCommand(value);
      break;

    case REG_TRACK:
      fdc.track = value;
      break;

    case REG_SECTOR:
      fdc.sector = value;
      break;

    case REG_DATA:
      fdc.data = value;
      if (fdc.drq && fdc.dataIndex < fdc.dataLength) {
        fdc.sectorBuffer[fdc.dataIndex++] = value;
        if (fdc.dataIndex >= fdc.dataLength) {
          fdc.drq = false;
          fdc.status &= ~ST_DRQ;
          completeSectorWrite();
        }
      }
      break;
  }
}

// ===================== COMMAND EXECUTION =====================

void executeCommand(uint8_t cmd) {
  fdc.command = cmd;
  fdc.status = ST_BUSY;
  fdc.busy = true;
  fdc.intrq = false;
  fdc.doubleDensity = (digitalRead(PIN_DDEN) == LOW);

  uint8_t cmdType = cmd & 0xF0;

  if ((cmd & 0x80) == 0) {
    if (cmdType == CMD_RESTORE) {
      cmdRestore(cmd);
    }
    else if (cmdType == CMD_SEEK) {
      cmdSeek(cmd);
    }
    else {
      cmdStep(cmd);
    }
  }
  else if ((cmd & 0x40) == 0) {
    if ((cmd & 0x20) == 0) {
      cmdReadSector(cmd);
    } else {
      cmdWriteSector(cmd);
    }
  }
  else if ((cmd & 0x10) == 0) {
    if (cmdType == CMD_READ_ADDRESS) {
      cmdReadAddress(cmd);
    }
    else if (cmdType == CMD_READ_TRACK) {
      cmdReadTrack(cmd);
    }
    else {
      cmdWriteTrack(cmd);
    }
  }
  else {
    cmdForceInterrupt(cmd);
  }
}

void cmdRestore(uint8_t cmd) {
  fdc.currentTrack = 0;
  fdc.track = 0;
  fdc.status = ST_TRACK00;
  fdc.busy = false;
  fdc.intrq = true;

  digitalWrite(PIN_LED, HIGH);
  delay(10);
  digitalWrite(PIN_LED, LOW);
}

void cmdSeek(uint8_t cmd) {
  fdc.currentTrack = fdc.data;
  if (fdc.currentTrack >= disks[activeDrive].tracks) {
    fdc.currentTrack = disks[activeDrive].tracks - 1;
  }

  fdc.track = fdc.currentTrack;
  fdc.status = (fdc.currentTrack == 0) ? ST_TRACK00 : 0;
  fdc.busy = false;
  fdc.intrq = true;
}

void cmdStep(uint8_t cmd) {
  uint8_t cmdType = cmd & 0xE0;

  if (cmdType == CMD_STEP_IN) {
    fdc.direction = 1;
    fdc.currentTrack++;
  }
  else if (cmdType == CMD_STEP_OUT) {
    fdc.direction = -1;
    if (fdc.currentTrack > 0) fdc.currentTrack--;
  }
  else {
    fdc.currentTrack += fdc.direction;
  }

  if (fdc.currentTrack >= disks[activeDrive].tracks) {
    fdc.currentTrack = disks[activeDrive].tracks - 1;
  }

  if (cmd & 0x10) {
    fdc.track = fdc.currentTrack;
  }

  fdc.status = (fdc.currentTrack == 0) ? ST_TRACK00 : 0;
  fdc.busy = false;
  fdc.intrq = true;
}

void cmdReadSector(uint8_t cmd) {
  DiskImage* currentDisk = &disks[activeDrive];

  if (fdc.currentTrack >= currentDisk->tracks || fdc.sector == 0 || fdc.sector > currentDisk->sectorsPerTrack) {
    fdc.status = ST_RNF;
    fdc.busy = false;
    fdc.intrq = true;
    return;
  }

  uint32_t offset = (fdc.currentTrack * currentDisk->sectorsPerTrack + (fdc.sector - 1)) * currentDisk->sectorSize;

  imageFiles[activeDrive].seek(offset);
  fdc.dataLength = imageFiles[activeDrive].read(fdc.sectorBuffer, currentDisk->sectorSize);

  fdc.dataIndex = 0;
  fdc.drq = true;
  fdc.status = ST_BUSY | ST_DRQ;

  digitalWrite(PIN_LED, HIGH);
}

void completeSectorRead() {
  fdc.busy = false;
  fdc.intrq = true;
  fdc.status = 0;

  digitalWrite(PIN_LED, LOW);

  if ((fdc.command & 0x10) && fdc.sector < disks[activeDrive].sectorsPerTrack) {
    fdc.sector++;
    cmdReadSector(fdc.command);
  }
}

void cmdWriteSector(uint8_t cmd) {
  DiskImage* currentDisk = &disks[activeDrive];

  if (fdc.writeProtect) {
    fdc.status = ST_WRITE_PROTECT;
    fdc.busy = false;
    fdc.intrq = true;
    return;
  }

  if (fdc.currentTrack >= currentDisk->tracks || fdc.sector == 0 || fdc.sector > currentDisk->sectorsPerTrack) {
    fdc.status = ST_RNF;
    fdc.busy = false;
    fdc.intrq = true;
    return;
  }

  fdc.dataIndex = 0;
  fdc.dataLength = currentDisk->sectorSize;
  fdc.drq = true;
  fdc.status = ST_BUSY | ST_DRQ;

  digitalWrite(PIN_LED, HIGH);
}

void completeSectorWrite() {
  DiskImage* currentDisk = &disks[activeDrive];

  uint32_t offset = (fdc.currentTrack * currentDisk->sectorsPerTrack + (fdc.sector - 1)) * currentDisk->sectorSize;

  imageFiles[activeDrive].seek(offset);
  imageFiles[activeDrive].write(fdc.sectorBuffer, currentDisk->sectorSize);
  imageFiles[activeDrive].flush();

  fdc.busy = false;
  fdc.intrq = true;
  fdc.status = 0;

  digitalWrite(PIN_LED, LOW);

  if ((fdc.command & 0x10) && fdc.sector < currentDisk->sectorsPerTrack) {
    fdc.sector++;
    cmdWriteSector(fdc.command);
  }
}

void cmdReadAddress(uint8_t cmd) {
  fdc.sectorBuffer[0] = fdc.currentTrack;
  fdc.sectorBuffer[1] = 0;
  fdc.sectorBuffer[2] = 1;
  fdc.sectorBuffer[3] = (disks[activeDrive].sectorSize == 512) ? 2 : 1;
  fdc.sectorBuffer[4] = 0;
  fdc.sectorBuffer[5] = 0;

  fdc.dataIndex = 0;
  fdc.dataLength = 6;
  fdc.drq = true;
  fdc.status = ST_BUSY | ST_DRQ;
}

void cmdReadTrack(uint8_t cmd) {
  DiskImage* currentDisk = &disks[activeDrive];

  uint32_t offset = fdc.currentTrack * currentDisk->sectorsPerTrack * currentDisk->sectorSize;
  uint16_t trackSize = currentDisk->sectorsPerTrack * currentDisk->sectorSize;

  imageFiles[activeDrive].seek(offset);
  fdc.dataLength = imageFiles[activeDrive].read(fdc.sectorBuffer, min(trackSize, 1024));

  fdc.dataIndex = 0;
  fdc.drq = true;
  fdc.status = ST_BUSY | ST_DRQ;
}

void cmdWriteTrack(uint8_t cmd) {
  if (fdc.writeProtect) {
    fdc.status = ST_WRITE_PROTECT;
    fdc.busy = false;
    fdc.intrq = true;
    return;
  }

  fdc.dataIndex = 0;
  fdc.dataLength = disks[activeDrive].sectorsPerTrack * disks[activeDrive].sectorSize;
  fdc.drq = true;
  fdc.status = ST_BUSY | ST_DRQ;
}

void cmdForceInterrupt(uint8_t cmd) {
  fdc.busy = false;
  fdc.drq = false;
  fdc.status = 0;

  if (cmd & 0x0F) {
    fdc.intrq = true;
  }
}

// ===================== OUTPUT MANAGEMENT =====================

void updateOutputs() {
  digitalWrite(PIN_INTRQ, fdc.intrq ? HIGH : LOW);
  digitalWrite(PIN_DRQ, fdc.drq ? HIGH : LOW);
}

void processCommand() {
}

// ===================== DRIVE SELECT =====================

void checkDriveSelect() {
  bool ds0 = (digitalRead(PIN_DS0) == LOW);
  bool ds1 = (digitalRead(PIN_DS1) == LOW);

  uint8_t newDrive = activeDrive;

  if (ds0 && !ds1) {
    newDrive = 0;  // Drive 0 selected
  }
  else if (!ds0 && ds1) {
    newDrive = 1;  // Drive 1 selected
  }
  // If both or neither active, keep current drive

  if (newDrive != activeDrive) {
    activeDrive = newDrive;
    fdc.writeProtect = disks[activeDrive].writeProtected;

    Serial.print("System selected Drive ");
    Serial.println(activeDrive);

    updateDisplay();
  }
}

// ===================== OLED DISPLAY =====================

void updateDisplay() {
  if (!oledPresent) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("WD1770 Emulator");

  if (fdc.busy) {
    display.fillCircle(120, 3, 3, SSD1306_WHITE);
  }

  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 14);
  if (activeDrive == 0) {
    display.print("*");
  } else {
    display.print(" ");
  }

  if (uiSelectedDrive == 0) {
    display.print("> A:");
  } else {
    display.print("  A:");
  }

  display.setCursor(0, 24);
  if (disks[0].filename[0] != '\0') {
    String fname = String(disks[0].filename);
    if (fname.length() > 21) {
      fname = fname.substring(0, 18) + "...";
    }
    display.print(fname);
  } else {
    display.print("(empty)");
  }

  display.setCursor(0, 36);
  if (activeDrive == 1) {
    display.print("*");
  } else {
    display.print(" ");
  }

  if (uiSelectedDrive == 1) {
    display.print("> B:");
  } else {
    display.print("  B:");
  }

  display.setCursor(0, 46);
  if (disks[1].filename[0] != '\0') {
    String fname = String(disks[1].filename);
    if (fname.length() > 21) {
      fname = fname.substring(0, 18) + "...";
    }
    display.print(fname);
  } else {
    display.print("(empty)");
  }

  display.drawLine(0, 56, 127, 56, SSD1306_WHITE);
  display.setCursor(0, 58);
  display.print("Img:");
  display.print(totalImages);

  if (disks[activeDrive].filename[0] != '\0') {
    display.setCursor(50, 58);
    display.print("T:");
    display.print(fdc.currentTrack);
  }

  display.setCursor(80, 58);
  display.print("*=Act >=UI");

  display.display();
}

void showImageSelectionMenu() {
  if (!oledPresent) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Select for Drive ");
  display.print((char)('A' + uiSelectedDrive));
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  int startIdx = max(0, currentImageIndex[uiSelectedDrive] - 2);
  int endIdx = min(totalImages - 1, startIdx + 4);

  if (endIdx - startIdx < 4 && startIdx > 0) {
    startIdx = max(0, endIdx - 4);
  }

  int y = 14;
  for (int i = startIdx; i <= endIdx && i < totalImages; i++) {
    display.setCursor(0, y);

    if (i == currentImageIndex[uiSelectedDrive]) {
      display.fillRect(0, y, 128, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.print(">");
    } else {
      display.setTextColor(SSD1306_WHITE);
      display.print(" ");
    }

    String fname = diskImages[i];
    if (fname.length() > 20) {
      fname = fname.substring(0, 17) + "...";
    }
    display.print(fname);

    display.setTextColor(SSD1306_WHITE);
    y += 10;
  }

  display.setCursor(0, 56);
  display.print("Turn=Sel Press=Load");

  display.display();
}

void showDriveSwitchMessage() {
  if (!oledPresent) return;

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 20);
  display.print("Drive ");
  display.print((char)('A' + uiSelectedDrive));
  display.setTextSize(1);
  display.setCursor(15, 45);
  display.print("UI Mode Selected");
  display.display();
}

// ===================== IMAGE SELECTION =====================

void checkImageSelection() {
  static int lastCLK = HIGH;
  static unsigned long lastDebounce = 0;
  static unsigned long buttonPressTime = 0;
  static bool buttonPressed = false;
  static bool inSelectionMode = false;

  int clk = digitalRead(ROTARY_CLK);
  int sw = digitalRead(ROTARY_SW);

  if (clk != lastCLK && (millis() - lastDebounce > 5)) {
    if (digitalRead(ROTARY_DT) != clk) {
      currentImageIndex[uiSelectedDrive]++;
      if (currentImageIndex[uiSelectedDrive] >= totalImages) {
        currentImageIndex[uiSelectedDrive] = 0;
      }
    } else {
      currentImageIndex[uiSelectedDrive]--;
      if (currentImageIndex[uiSelectedDrive] < 0) {
        currentImageIndex[uiSelectedDrive] = totalImages - 1;
      }
    }

    inSelectionMode = true;
    showImageSelectionMenu();
    lastDebounce = millis();
  }

  lastCLK = clk;

  if (inSelectionMode && (millis() - lastDebounce > 3000)) {
    inSelectionMode = false;
    updateDisplay();
  }

  if (sw == LOW && !buttonPressed) {
    buttonPressed = true;
    buttonPressTime = millis();
  }

  if (sw == HIGH && buttonPressed) {
    unsigned long pressDuration = millis() - buttonPressTime;

    if (pressDuration >= 1000) {
      uiSelectedDrive = 1 - uiSelectedDrive;
      showDriveSwitchMessage();
      delay(500);
      updateDisplay();
    } else if (pressDuration >= 50) {
      loadDiskImage(uiSelectedDrive, currentImageIndex[uiSelectedDrive]);
      inSelectionMode = false;
      updateDisplay();
    }

    buttonPressed = false;
    buttonPressTime = 0;
  }
}

// ===================== DATA BUS FUNCTIONS =====================

uint8_t readDataBus() {
  uint8_t value = 0;
  for (int i = 0; i < 8; i++) {
    pinMode(dataPins[i], INPUT);
    if (digitalRead(dataPins[i]) == HIGH) {
      value |= (1 << i);
    }
  }
  return value;
}

void setDataBus(uint8_t value) {
  for (int i = 0; i < 8; i++) {
    pinMode(dataPins[i], OUTPUT);
    digitalWrite(dataPins[i], (value & (1 << i)) ? HIGH : LOW);
  }
  delayMicroseconds(1);

  for (int i = 0; i < 8; i++) {
    pinMode(dataPins[i], INPUT);
  }
}
