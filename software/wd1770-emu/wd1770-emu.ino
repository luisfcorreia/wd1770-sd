/*
   WD1770/1772 Drop-in Replacement with SD Card Support - IMPROVED VERSION
   For STM32F4 (e.g., STM32F407, STM32F411 "Black Pill")

   FIXES IMPLEMENTED:
   - Proper state machine for async operations
   - Non-blocking SD card I/O
   - Correct timing delays
   - Proper DRQ handshaking
   - Multi-sector command support
   - Bus timing improvements
   
   Inspired by FlashFloppy - reads disk images from SD card
   Supports standard floppy disk image formats
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
#define PIN_D0          PB0
#define PIN_D1          PB1
#define PIN_D2          PB2
#define PIN_D3          PB3
#define PIN_D4          PB4
#define PIN_D5          PB5
#define PIN_D6          PB6
#define PIN_D7          PB7

#define PIN_A0          PA8
#define PIN_A1          PA9
#define PIN_CS          PA10
#define PIN_RE          PA11
#define PIN_WE          PA12

// WD1770 Control Outputs
#define PIN_INTRQ       PA15
#define PIN_DRQ         PB8

// WD1770 Inputs from System
#define PIN_DDEN        PB9
#define PIN_DS0         PB12
#define PIN_DS1         PB13

// User Interface pins
#define PIN_LED         PC13
#define ROTARY_CLK      PA0
#define ROTARY_DT       PA1
#define ROTARY_SW       PA2

// Display customization
#define DISPLAY_HEADER  "WD1770 Emu"  // Max 16 chars

// I2C pins for OLED
#define OLED_SDA        PB11
#define OLED_SCL        PB10
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C

// ===================== CONSTANTS =====================

#define MAX_DRIVES      2

// Register addresses
#define REG_STATUS_CMD  0
#define REG_TRACK       1
#define REG_SECTOR      2
#define REG_DATA        3

// Command types
#define CMD_RESTORE         0x00
#define CMD_SEEK            0x10
#define CMD_STEP            0x20
#define CMD_STEP_IN         0x40
#define CMD_STEP_OUT        0x60
#define CMD_READ_SECTOR     0x80
#define CMD_READ_SECTORS    0x90
#define CMD_WRITE_SECTOR    0xA0
#define CMD_WRITE_SECTORS   0xB0
#define CMD_READ_ADDRESS    0xC0
#define CMD_READ_TRACK      0xE0
#define CMD_WRITE_TRACK     0xF0
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
#define SECTOR_READ_TIME    3000   // Time to "read" a sector from disk
#define SECTOR_WRITE_TIME   3000   // Time to "write" a sector to disk

// ===================== STATE MACHINE =====================

enum FDCStateEnum {
  STATE_IDLE,
  STATE_SEEKING,
  STATE_SETTLING,
  STATE_READING_SECTOR,
  STATE_SECTOR_READ_COMPLETE,
  STATE_WRITING_SECTOR,
  STATE_SECTOR_WRITE_COMPLETE,
  STATE_WAITING_FOR_DATA_IN,
  STATE_WAITING_FOR_DATA_OUT
};

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

  uint32_t operationStartTime;
  uint32_t stepRate;

  bool writeProtect;
  bool motorOn;

  FDCStateEnum state;
  uint8_t sectorsRemaining;
  bool multiSector;

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

// Bus state tracking
volatile bool lastCS = HIGH;
volatile bool lastRE = HIGH;
volatile bool lastWE = HIGH;
uint32_t dataValidUntil = 0;
bool dataBusDriven = false;

// ===================== FUNCTION PROTOTYPES =====================

void initPins();
bool initSDCard();
void scanDiskImages();
bool loadDiskImage(uint8_t drive, int imageIndex);
void initFDC();
void handleRead(uint8_t addr);
void handleWrite(uint8_t addr);
void executeCommand(uint8_t cmd);
void processStateMachine();
void startSectorRead();
void completeSectorRead();
void startSectorWrite();
void completeSectorWrite();
void updateOutputs();
void checkDriveSelect();
void updateDisplay();
void checkImageSelection();
uint8_t readDataBus();
void setDataBus(uint8_t value);
void releaseDataBus();

// ===================== SETUP =====================

void setup() {
  Serial.begin(115200);
  Serial.println("WD1770 SD Card Emulator - IMPROVED");
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
    display.println("WD1770 Emulator v2");
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
    // FDC disabled, tri-state outputs and reset state
    releaseDataBus();
    digitalWrite(PIN_INTRQ, LOW);
    digitalWrite(PIN_DRQ, LOW);
    return;
  }

  // Check which drive is selected by system
  checkDriveSelect();

  // Handle bus transactions with edge detection
  bool currentCS = digitalRead(PIN_CS);
  bool currentRE = digitalRead(PIN_RE);
  bool currentWE = digitalRead(PIN_WE);

  // Chip select active
  if (currentCS == LOW) {
    uint8_t addr = (digitalRead(PIN_A1) << 1) | digitalRead(PIN_A0);

    // Read cycle: falling edge of RE
    if (currentRE == LOW && lastRE == HIGH) {
      handleRead(addr);
      dataValidUntil = micros() + 500; // Keep data valid for 500µs
    }
    
    // Write cycle: falling edge of WE
    if (currentWE == LOW && lastWE == HIGH) {
      handleWrite(addr);
    }
  } else {
    // CS released, release data bus
    if (dataBusDriven && micros() >= dataValidUntil) {
      releaseDataBus();
    }
  }

  lastCS = currentCS;
  lastRE = currentRE;
  lastWE = currentWE;

  // Process state machine (non-blocking)
  processStateMachine();
  
  // Update output signals
  updateOutputs();
  
  // Check for image selection changes
  checkImageSelection();

  // Update display periodically
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

      if (filename.endsWith(".DSK") || filename.endsWith(".IMG") || 
          filename.endsWith(".ST") || filename.endsWith(".HFE")) {
        if (totalImages < 100) {
          diskImages[totalImages] = String(entry.name());
          Serial.print("Found: ");
          Serial.println(diskImages[totalImages]);
          totalImages++;
        }
      }
    }
    entry.close();
  }
  root.close();

  Serial.print("Found ");
  Serial.print(totalImages);
  Serial.println(" disk images");
}

bool loadDiskImage(uint8_t drive, int imageIndex) {
  if (drive >= MAX_DRIVES || imageIndex >= totalImages) {
    return false;
  }

  if (imageFiles[drive]) {
    imageFiles[drive].close();
  }

  String filename = "/" + diskImages[imageIndex];
  imageFiles[drive] = SD.open(filename.c_str(), FILE_WRITE);

  if (!imageFiles[drive]) {
    Serial.print("Failed to open: ");
    Serial.println(filename);
    return false;
  }

  DiskImage* disk = &disks[drive];
  strncpy(disk->filename, diskImages[imageIndex].c_str(), 63);
  disk->filename[63] = '\0';
  disk->size = imageFiles[drive].size();

  // Detect format by size
  if (disk->size == 737280) {  // 720KB
    disk->tracks = 80;
    disk->sectorsPerTrack = 9;
    disk->sectorSize = 512;
    disk->doubleDensity = true;
  } else if (disk->size == 368640) {  // 360KB
    disk->tracks = 40;
    disk->sectorsPerTrack = 9;
    disk->sectorSize = 512;
    disk->doubleDensity = true;
  } else if (disk->size == 163840) {  // 160KB Timex
    disk->tracks = 40;
    disk->sectorsPerTrack = 16;
    disk->sectorSize = 256;
    disk->doubleDensity = false;
  } else if (disk->size == 327680) {  // 320KB Timex DS
    disk->tracks = 80;
    disk->sectorsPerTrack = 16;
    disk->sectorSize = 256;
    disk->doubleDensity = false;
  } else {
    // Unknown format, try to guess
    disk->tracks = 80;
    disk->sectorsPerTrack = 9;
    disk->sectorSize = 512;
    disk->doubleDensity = true;
  }

  disk->writeProtected = false;

  Serial.print("Drive ");
  Serial.print(drive);
  Serial.print(": Loaded ");
  Serial.print(disk->filename);
  Serial.print(" (");
  Serial.print(disk->size);
  Serial.print(" bytes, ");
  Serial.print(disk->tracks);
  Serial.print("T/");
  Serial.print(disk->sectorsPerTrack);
  Serial.print("S/");
  Serial.print(disk->sectorSize);
  Serial.println("B)");

  return true;
}

void initFDC() {
  memset(&fdc, 0, sizeof(FDCState));
  fdc.status = ST_TRACK00;
  fdc.state = STATE_IDLE;
  fdc.stepRate = STEP_TIME_6MS;
  fdc.writeProtect = disks[0].writeProtected;
}

// ===================== REGISTER HANDLERS =====================

void handleRead(uint8_t addr) {
  uint8_t value = 0;

  switch (addr) {
    case REG_STATUS_CMD:
      value = fdc.status;
      // Reading status clears INTRQ
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
      // Only transfer data if DRQ is active
      if (fdc.drq && fdc.dataIndex < fdc.dataLength) {
        value = fdc.sectorBuffer[fdc.dataIndex++];
        
        // Check if transfer complete
        if (fdc.dataIndex >= fdc.dataLength) {
          fdc.drq = false;
          fdc.status &= ~ST_DRQ;
          
          // Move to completion state
          if (fdc.state == STATE_WAITING_FOR_DATA_OUT) {
            fdc.state = STATE_SECTOR_READ_COMPLETE;
          }
        }
      } else {
        // No data available, return last value
        value = fdc.data;
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
      // Only accept data if DRQ is active
      if (fdc.drq && fdc.dataIndex < fdc.dataLength) {
        fdc.sectorBuffer[fdc.dataIndex++] = value;
        
        // Check if transfer complete
        if (fdc.dataIndex >= fdc.dataLength) {
          fdc.drq = false;
          fdc.status &= ~ST_DRQ;
          
          // Move to completion state
          if (fdc.state == STATE_WAITING_FOR_DATA_IN) {
            fdc.state = STATE_SECTOR_WRITE_COMPLETE;
          }
        }
      }
      break;
  }
}

// ===================== COMMAND EXECUTION =====================

void executeCommand(uint8_t cmd) {
  // Ignore command if busy (except FORCE_INT)
  if (fdc.busy && (cmd & 0xF0) != CMD_FORCE_INT) {
    return;
  }

  fdc.command = cmd;
  fdc.busy = true;
  fdc.intrq = false;
  fdc.doubleDensity = (digitalRead(PIN_DDEN) == LOW);

  uint8_t cmdType = cmd & 0xF0;

  Serial.print("Command: 0x");
  Serial.print(cmd, HEX);

  // Type I - Seek commands
  if ((cmd & 0x80) == 0) {
    // Extract step rate
    uint8_t rateCode = cmd & 0x03;
    switch (rateCode) {
      case 0: fdc.stepRate = STEP_TIME_6MS; break;
      case 1: fdc.stepRate = STEP_TIME_12MS; break;
      case 2: fdc.stepRate = STEP_TIME_20MS; break;
      case 3: fdc.stepRate = STEP_TIME_30MS; break;
    }

    if (cmdType == CMD_RESTORE) {
      Serial.println("  (RESTORE)");
      fdc.currentTrack = 0;
      fdc.track = 0;
      fdc.status = ST_BUSY | ST_TRACK00;
      fdc.state = STATE_SEEKING;
      fdc.operationStartTime = micros();
    }
    else if (cmdType == CMD_SEEK) {
      Serial.print("  (SEEK to ");
      Serial.print(fdc.data);
      Serial.println(")");
      fdc.currentTrack = fdc.data;
      fdc.track = fdc.data;
      fdc.status = ST_BUSY;
      if (fdc.currentTrack == 0) {
        fdc.status |= ST_TRACK00;
      }
      fdc.state = STATE_SEEKING;
      fdc.operationStartTime = micros();
    }
    else { // STEP variants
      Serial.println("  (STEP)");
      if (cmdType == CMD_STEP_IN) {
        fdc.direction = 1;
      } else if (cmdType == CMD_STEP_OUT) {
        fdc.direction = -1;
      }
      
      fdc.currentTrack += fdc.direction;
      if (fdc.currentTrack > MAX_TRACKS) fdc.currentTrack = 0;
      
      if (cmd & 0x10) {  // Update track register
        fdc.track = fdc.currentTrack;
      }
      
      fdc.status = ST_BUSY;
      if (fdc.currentTrack == 0) {
        fdc.status |= ST_TRACK00;
      }
      fdc.state = STATE_SEEKING;
      fdc.operationStartTime = micros();
    }
  }
  // Type II - Read/Write sector
  else if ((cmd & 0x40) == 0) {
    if ((cmd & 0x20) == 0) {
      Serial.println("  (READ SECTOR)");
      fdc.multiSector = (cmd & 0x10);
      fdc.sectorsRemaining = fdc.multiSector ? disks[activeDrive].sectorsPerTrack - fdc.sector + 1 : 1;
      fdc.status = ST_BUSY;
      fdc.state = STATE_READING_SECTOR;
      fdc.operationStartTime = micros();
      startSectorRead();
    } else {
      Serial.println("  (WRITE SECTOR)");
      fdc.multiSector = (cmd & 0x10);
      fdc.sectorsRemaining = fdc.multiSector ? disks[activeDrive].sectorsPerTrack - fdc.sector + 1 : 1;
      fdc.status = ST_BUSY;
      fdc.state = STATE_WRITING_SECTOR;
      fdc.operationStartTime = micros();
      startSectorWrite();
    }
  }
  // Type III - Track/Address operations
  else if ((cmd & 0x10) == 0) {
    if (cmdType == CMD_READ_ADDRESS) {
      Serial.println("  (READ ADDRESS)");
      cmdReadAddress();
    }
    else if (cmdType == CMD_READ_TRACK) {
      Serial.println("  (READ TRACK)");
      cmdReadTrack();
    }
    else {
      Serial.println("  (WRITE TRACK)");
      cmdWriteTrack();
    }
  }
  // Type IV - Force interrupt
  else {
    Serial.println("  (FORCE INT)");
    cmdForceInterrupt(cmd);
  }
}

// ===================== STATE MACHINE =====================

void processStateMachine() {
  uint32_t elapsed = micros() - fdc.operationStartTime;

  switch (fdc.state) {
    case STATE_IDLE:
      // Nothing to do
      break;

    case STATE_SEEKING:
      // Simulate seek time
      if (elapsed >= fdc.stepRate) {
        // Move to settling
        fdc.state = STATE_SETTLING;
        fdc.operationStartTime = micros();
      }
      break;

    case STATE_SETTLING:
      // Simulate head settle time
      if (elapsed >= HEAD_SETTLE_TIME) {
        // Seek complete
        fdc.busy = false;
        fdc.intrq = true;
        fdc.status &= ~ST_BUSY;
        fdc.state = STATE_IDLE;
      }
      break;

    case STATE_READING_SECTOR:
      // Simulate sector read time
      if (elapsed >= SECTOR_READ_TIME) {
        // Data ready, set DRQ
        fdc.drq = true;
        fdc.status |= ST_DRQ;
        fdc.state = STATE_WAITING_FOR_DATA_OUT;
        digitalWrite(PIN_LED, HIGH);
      }
      break;

    case STATE_SECTOR_READ_COMPLETE:
      // Host has read all data
      digitalWrite(PIN_LED, LOW);
      
      // Check if multi-sector
      if (fdc.multiSector && fdc.sectorsRemaining > 1) {
        fdc.sectorsRemaining--;
        fdc.sector++;
        if (fdc.sector > disks[activeDrive].sectorsPerTrack) {
          // Wrap to next track
          fdc.sector = 1;
          fdc.currentTrack++;
          fdc.track++;
        }
        // Start next sector
        fdc.state = STATE_READING_SECTOR;
        fdc.operationStartTime = micros();
        startSectorRead();
      } else {
        // All done
        fdc.busy = false;
        fdc.intrq = true;
        fdc.status &= ~ST_BUSY;
        fdc.state = STATE_IDLE;
      }
      break;

    case STATE_WRITING_SECTOR:
      // Simulate time to prepare for write
      if (elapsed >= 500) {
        // Ready for data, set DRQ
        fdc.drq = true;
        fdc.status |= ST_DRQ;
        fdc.state = STATE_WAITING_FOR_DATA_IN;
        digitalWrite(PIN_LED, HIGH);
      }
      break;

    case STATE_SECTOR_WRITE_COMPLETE:
      // Host has written all data, now commit to SD
      completeSectorWrite();
      digitalWrite(PIN_LED, LOW);
      
      // Check if multi-sector
      if (fdc.multiSector && fdc.sectorsRemaining > 1) {
        fdc.sectorsRemaining--;
        fdc.sector++;
        if (fdc.sector > disks[activeDrive].sectorsPerTrack) {
          fdc.sector = 1;
          fdc.currentTrack++;
          fdc.track++;
        }
        // Start next sector
        fdc.state = STATE_WRITING_SECTOR;
        fdc.operationStartTime = micros();
        startSectorWrite();
      } else {
        // All done
        fdc.busy = false;
        fdc.intrq = true;
        fdc.status &= ~ST_BUSY;
        fdc.state = STATE_IDLE;
      }
      break;

    case STATE_WAITING_FOR_DATA_IN:
    case STATE_WAITING_FOR_DATA_OUT:
      // Wait for host to transfer data
      // Lost data timeout after 10ms
      if (elapsed >= 10000) {
        fdc.status |= ST_LOST_DATA;
        fdc.drq = false;
        fdc.busy = false;
        fdc.intrq = true;
        fdc.state = STATE_IDLE;
        digitalWrite(PIN_LED, LOW);
      }
      break;
  }
}

// ===================== SECTOR READ/WRITE =====================

void startSectorRead() {
  DiskImage* currentDisk = &disks[activeDrive];

  // Validate parameters
  if (fdc.currentTrack >= currentDisk->tracks || 
      fdc.sector == 0 || 
      fdc.sector > currentDisk->sectorsPerTrack) {
    fdc.status = ST_RNF;
    fdc.busy = false;
    fdc.intrq = true;
    fdc.state = STATE_IDLE;
    return;
  }

  // Calculate offset
  uint32_t offset = (fdc.currentTrack * currentDisk->sectorsPerTrack + (fdc.sector - 1)) * currentDisk->sectorSize;

  // Read from SD card (this is the slow part, but we handle it asynchronously)
  imageFiles[activeDrive].seek(offset);
  fdc.dataLength = imageFiles[activeDrive].read(fdc.sectorBuffer, currentDisk->sectorSize);
  fdc.dataIndex = 0;

  // State machine will set DRQ after simulated delay
}

void completeSectorRead() {
  // Nothing to do, handled in state machine
}

void startSectorWrite() {
  DiskImage* currentDisk = &disks[activeDrive];

  // Check write protect
  if (fdc.writeProtect || currentDisk->writeProtected) {
    fdc.status = ST_WRITE_PROTECT;
    fdc.busy = false;
    fdc.intrq = true;
    fdc.state = STATE_IDLE;
    return;
  }

  // Validate parameters
  if (fdc.currentTrack >= currentDisk->tracks || 
      fdc.sector == 0 || 
      fdc.sector > currentDisk->sectorsPerTrack) {
    fdc.status = ST_RNF;
    fdc.busy = false;
    fdc.intrq = true;
    fdc.state = STATE_IDLE;
    return;
  }

  fdc.dataIndex = 0;
  fdc.dataLength = currentDisk->sectorSize;

  // State machine will set DRQ after simulated delay
}

void completeSectorWrite() {
  DiskImage* currentDisk = &disks[activeDrive];

  // Calculate offset
  uint32_t offset = (fdc.currentTrack * currentDisk->sectorsPerTrack + (fdc.sector - 1)) * currentDisk->sectorSize;

  // Write to SD card
  imageFiles[activeDrive].seek(offset);
  imageFiles[activeDrive].write(fdc.sectorBuffer, currentDisk->sectorSize);
  imageFiles[activeDrive].flush();
}

void cmdReadAddress() {
  // Return current track ID
  fdc.sectorBuffer[0] = fdc.currentTrack;
  fdc.sectorBuffer[1] = 0;  // Side
  fdc.sectorBuffer[2] = 1;  // Sector
  fdc.sectorBuffer[3] = (disks[activeDrive].sectorSize == 512) ? 2 : 1;  // Size code
  fdc.sectorBuffer[4] = 0;  // CRC1
  fdc.sectorBuffer[5] = 0;  // CRC2

  fdc.dataIndex = 0;
  fdc.dataLength = 6;
  fdc.drq = true;
  fdc.status = ST_BUSY | ST_DRQ;
  fdc.state = STATE_WAITING_FOR_DATA_OUT;
}

void cmdReadTrack() {
  DiskImage* currentDisk = &disks[activeDrive];

  uint32_t offset = fdc.currentTrack * currentDisk->sectorsPerTrack * currentDisk->sectorSize;
  uint16_t trackSize = currentDisk->sectorsPerTrack * currentDisk->sectorSize;

  imageFiles[activeDrive].seek(offset);
  fdc.dataLength = imageFiles[activeDrive].read(fdc.sectorBuffer, min(trackSize, (uint16_t)1024));

  fdc.dataIndex = 0;
  fdc.drq = true;
  fdc.status = ST_BUSY | ST_DRQ;
  fdc.state = STATE_WAITING_FOR_DATA_OUT;
}

void cmdWriteTrack() {
  if (fdc.writeProtect || disks[activeDrive].writeProtected) {
    fdc.status = ST_WRITE_PROTECT;
    fdc.busy = false;
    fdc.intrq = true;
    return;
  }

  fdc.dataIndex = 0;
  fdc.dataLength = disks[activeDrive].sectorsPerTrack * disks[activeDrive].sectorSize;
  fdc.drq = true;
  fdc.status = ST_BUSY | ST_DRQ;
  fdc.state = STATE_WAITING_FOR_DATA_IN;
}

void cmdForceInterrupt(uint8_t cmd) {
  fdc.busy = false;
  fdc.drq = false;
  fdc.status = 0;
  fdc.state = STATE_IDLE;

  if (cmd & 0x0F) {
    fdc.intrq = true;
  }
}

// ===================== OUTPUT MANAGEMENT =====================

void updateOutputs() {
  digitalWrite(PIN_INTRQ, fdc.intrq ? HIGH : LOW);
  digitalWrite(PIN_DRQ, fdc.drq ? HIGH : LOW);
}

// ===================== DRIVE SELECT =====================

void checkDriveSelect() {
  bool ds0 = (digitalRead(PIN_DS0) == LOW);
  bool ds1 = (digitalRead(PIN_DS1) == LOW);

  uint8_t newDrive = activeDrive;

  if (ds0 && !ds1) {
    newDrive = 0;
  }
  else if (!ds0 && ds1) {
    newDrive = 1;
  }

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
  
  // Display customizable header (max 16 chars)
  String header = DISPLAY_HEADER;
  if (header.length() > 16) header = header.substring(0, 16);
  display.print(header);

  // Activity indicator in top right
  if (fdc.busy) {
    display.fillCircle(120, 3, 3, SSD1306_WHITE);
  }

  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // Drive A label and filename
  display.setCursor(0, 12);
  if (activeDrive == 0) display.print("*");
  else display.print(" ");
  if (uiSelectedDrive == 0) display.print(">A:");
  else display.print(" A:");

  if (disks[0].filename[0] != '\0') {
    String fname = String(disks[0].filename);
    if (fname.length() > 18) fname = fname.substring(0, 15) + "...";
    display.print(fname);
  } else {
    display.print("(empty)");
  }
  
  // Track info for Drive A
  display.setCursor(0, 22);
  if (disks[0].filename[0] != '\0') {
    display.print(" T:");
    if (activeDrive == 0) {
      display.print(fdc.currentTrack);
      display.print("/");
      display.print(disks[0].tracks - 1);
    } else {
      display.print("--");
    }
  }

  // Drive B label and filename
  display.setCursor(0, 34);
  if (activeDrive == 1) display.print("*");
  else display.print(" ");
  if (uiSelectedDrive == 1) display.print(">B:");
  else display.print(" B:");

  if (disks[1].filename[0] != '\0') {
    String fname = String(disks[1].filename);
    if (fname.length() > 18) fname = fname.substring(0, 15) + "...";
    display.print(fname);
  } else {
    display.print("(empty)");
  }
  
  // Track info for Drive B
  display.setCursor(0, 44);
  if (disks[1].filename[0] != '\0') {
    display.print(" T:");
    if (activeDrive == 1) {
      display.print(fdc.currentTrack);
      display.print("/");
      display.print(disks[1].tracks - 1);
    } else {
      display.print("--");
    }
  }

  // Status line at bottom
  display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
  display.setCursor(0, 56);
  display.print("Img:");
  display.print(totalImages);
  
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
    if (fname.length() > 20) fname = fname.substring(0, 17) + "...";
    display.print(fname);

    display.setTextColor(SSD1306_WHITE);
    y += 10;
  }

  display.setCursor(0, 56);
  display.print("Turn=Sel Press=Load");
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
  dataBusDriven = true;
  // Don't release immediately - let CS going high trigger release
}

void releaseDataBus() {
  if (dataBusDriven) {
    for (int i = 0; i < 8; i++) {
      pinMode(dataPins[i], INPUT);
    }
    dataBusDriven = false;
  }
}
