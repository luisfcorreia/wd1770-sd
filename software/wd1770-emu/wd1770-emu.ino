/*
 * WD1770/1772 Drop-in Replacement with SD Card Support
 * For STM32F4 (e.g., STM32F407, STM32F411 "Black Pill")
 * 
 * Inspired by FlashFloppy - reads disk images from SD card
 * Supports standard floppy disk image formats
 * 
 * Hardware Requirements:
 * - STM32F4 board (e.g., Black Pill STM32F411)
 * - SD card module (SPI interface)
 * - Level shifters if interfacing with 5V systems
 * - OLED display (optional, I2C)
 * - Rotary encoder (optional, for image selection)
 * 
 * Pin-compatible with WD1770 28-pin DIP package
 * 
 * Required Libraries:
 * - Adafruit SSD1306
 * - Adafruit GFX Library
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

// WD1770 CPU Interface Pins (matches 28-pin DIP)
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

// WD1770 Floppy Interface Pins
#define PIN_INTRQ       PA15  // Interrupt Request (output)
#define PIN_DRQ         PB8   // Data Request (output)
#define PIN_DDEN        PB9   // Double Density Enable (input)
#define PIN_WG          PB10  // Write Gate (output)
#define PIN_WD          PB11  // Write Data (output)
#define PIN_STEP        PB12  // Step pulse (output)
#define PIN_DIRC        PB13  // Direction (output, 0=out, 1=in)
#define PIN_TR00        PB14  // Track 00 (input from emulation)
#define PIN_IP          PB15  // Index Pulse (output)
#define PIN_WPT         PC13  // Write Protect (input)
#define PIN_RDY         PC14  // Ready (input)
#define PIN_MO          PC15  // Motor On (input)

// Drive Select Pins (system selects which drive is active)
#define PIN_DS0         PC0   // Drive Select 0 (Drive A:)
#define PIN_DS1         PC1   // Drive Select 1 (Drive B:)

// Optional UI pins
#define PIN_LED         PC13  // Activity LED
#define ROTARY_CLK      PA0   // Rotary encoder
#define ROTARY_DT       PA1
#define ROTARY_SW       PA2   // Rotary switch

// I2C pins for OLED (hardware I2C)
// Note: STM32F411 uses PB6=SCL, PB7=SDA for I2C1
// But these conflict with data bus, so use software I2C or I2C2
#define OLED_SDA        PB11  // Changed to avoid conflicts
#define OLED_SCL        PB10  // Changed to avoid conflicts
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1    // Reset pin not used
#define SCREEN_ADDRESS  0x3C  // Common I2C address for SSD1306

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
#define ST_RNF              0x10  // Record Not Found
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
#define SECTOR_SIZE_SD      256   // Single density
#define SECTOR_SIZE_DD      512   // Double density
#define TRACK_SIZE_SD       (9 * 256)
#define TRACK_SIZE_DD       (9 * 512)

// Timing constants (in microseconds)
#define STEP_TIME_6MS       6000
#define STEP_TIME_12MS      12000
#define STEP_TIME_20MS      20000
#define STEP_TIME_30MS      30000
#define HEAD_SETTLE_TIME    15000
#define MOTOR_SPINUP_TIME   1000000  // 1 second
#define INDEX_PULSE_TIME    200000   // 200ms (5 revs/sec)

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
    uint8_t sectorBuffer[1024];  // Max sector size
    
    uint32_t lastStepTime;
    uint32_t lastIndexTime;
    uint32_t stepRate;
    
    bool writeProtect;
    bool motorOn;
    bool indexPulse;
    
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
DiskImage disks[MAX_DRIVES];      // Two separate disk images (Drive A: and B:)
File imageFiles[MAX_DRIVES];      // Two open file handles
uint8_t activeDrive = 0;          // Currently selected drive (0 or 1)
uint8_t dataPins[] = {PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7};

String diskImages[100];
int currentImageIndex[MAX_DRIVES] = {0, 0};  // Selected image for each drive
int totalImages = 0;
bool oledPresent = false;
uint8_t uiSelectedDrive = 0;  // Drive being configured via UI (independent from activeDrive)

// OLED Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===================== SETUP =====================

void setup() {
    Serial.begin(115200);
    Serial.println("WD1770 SD Card Emulator");
    Serial.println("Based on FlashFloppy concept");
    
    // Initialize pins
    initPins();
    
    // Initialize I2C for OLED
    Wire.begin();
    Wire.setClock(400000);  // 400kHz I2C
    
    // Initialize OLED display
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
    
    // Initialize SD card
    if (!initSDCard()) {
        Serial.println("SD Card initialization failed!");
        if (oledPresent) {
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("ERROR:");
            display.println("SD Card Failed!");
            display.display();
        }
        while(1) {
            digitalWrite(PIN_LED, !digitalRead(PIN_LED));
            delay(100);
        }
    }
    
    // Scan for disk images
    scanDiskImages();
    
    // Load first images into both drives
    if (totalImages > 0) {
        loadDiskImage(0, 0);  // Drive A: first image
        if (totalImages > 1) {
            loadDiskImage(1, 1);  // Drive B: second image (if available)
        }
    }
    
    // Initialize FDC state
    initFDC();
    
    // Update display
    updateDisplay();
    
    Serial.println("Ready!");
}

// ===================== MAIN LOOP =====================

void loop() {
    // Check drive select lines to determine active drive
    checkDriveSelect();
    
    // Check for chip select
    if (digitalRead(PIN_CS) == LOW) {
        uint8_t addr = (digitalRead(PIN_A1) << 1) | digitalRead(PIN_A0);
        
        // Read operation
        if (digitalRead(PIN_RE) == LOW) {
            handleRead(addr);
            delayMicroseconds(1);
        }
        // Write operation
        else if (digitalRead(PIN_WE) == LOW) {
            handleWrite(addr);
            delayMicroseconds(1);
        }
    }
    
    // Update outputs
    updateOutputs();
    
    // Process command execution
    processCommand();
    
    // Generate index pulse
    generateIndexPulse();
    
    // Check for image selection (rotary encoder)
    checkImageSelection();
    
    // Update display periodically
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate > 100) {  // Update every 100ms
        updateDisplay();
        lastDisplayUpdate = millis();
    }
}

// ===================== PIN INITIALIZATION =====================

void initPins() {
    // Data bus - starts as inputs
    for (int i = 0; i < 8; i++) {
        pinMode(dataPins[i], INPUT);
    }
    
    // Address and control inputs
    pinMode(PIN_A0, INPUT);
    pinMode(PIN_A1, INPUT);
    pinMode(PIN_CS, INPUT);
    pinMode(PIN_RE, INPUT);
    pinMode(PIN_WE, INPUT);
    pinMode(PIN_DDEN, INPUT);
    pinMode(PIN_MO, INPUT);
    pinMode(PIN_WPT, INPUT_PULLUP);
    
    // Drive Select inputs
    pinMode(PIN_DS0, INPUT_PULLUP);
    pinMode(PIN_DS1, INPUT_PULLUP);
    
    // Control outputs
    pinMode(PIN_INTRQ, OUTPUT);
    pinMode(PIN_DRQ, OUTPUT);
    pinMode(PIN_WG, OUTPUT);
    pinMode(PIN_WD, OUTPUT);
    pinMode(PIN_STEP, OUTPUT);
    pinMode(PIN_DIRC, OUTPUT);
    pinMode(PIN_TR00, OUTPUT);
    pinMode(PIN_IP, OUTPUT);
    
    // UI
    pinMode(PIN_LED, OUTPUT);
    pinMode(ROTARY_CLK, INPUT_PULLUP);
    pinMode(ROTARY_DT, INPUT_PULLUP);
    pinMode(ROTARY_SW, INPUT_PULLUP);
    
    // Set initial states
    digitalWrite(PIN_INTRQ, LOW);
    digitalWrite(PIN_DRQ, LOW);
    digitalWrite(PIN_WG, LOW);
    digitalWrite(PIN_STEP, LOW);
    digitalWrite(PIN_IP, LOW);
    digitalWrite(PIN_TR00, HIGH);  // Track 0 initially
}

// ===================== SD CARD FUNCTIONS =====================

bool initSDCard() {
    SPI.setMOSI(SD_MOSI_PIN);
    SPI.setMISO(SD_MISO_PIN);
    SPI.setSCLK(SD_SCK_PIN);
    
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
            
            // Check for supported formats
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
    
    // Close previous image
    if (imageFiles[driveNum]) {
        imageFiles[driveNum].close();
    }
    
    // Open new image
    String path = "/" + diskImages[index];
    imageFiles[driveNum] = SD.open(path.c_str(), FILE_READ);
    
    if (!imageFiles[driveNum]) {
        Serial.print("Failed to open: ");
        Serial.println(path);
        return false;
    }
    
    // Analyze image format
    currentImageIndex[driveNum] = index;
    strncpy(disks[driveNum].filename, diskImages[index].c_str(), 63);
    disks[driveNum].size = imageFiles[driveNum].size();
    
    // Detect format based on size
    if (disks[driveNum].size == 737280) {  // 720KB - DD 3.5"
        disks[driveNum].tracks = 80;
        disks[driveNum].sectorsPerTrack = 9;
        disks[driveNum].sectorSize = 512;
        disks[driveNum].doubleDensity = true;
    }
    else if (disks[driveNum].size == 368640) {  // 360KB - DD 5.25"
        disks[driveNum].tracks = 40;
        disks[driveNum].sectorsPerTrack = 9;
        disks[driveNum].sectorSize = 512;
        disks[driveNum].doubleDensity = true;
    }
    else if (disks[driveNum].size == 163840) {  // 160KB - Timex FDD 3000 / Single Density
        disks[driveNum].tracks = 40;
        disks[driveNum].sectorsPerTrack = 16;
        disks[driveNum].sectorSize = 256;
        disks[driveNum].doubleDensity = false;
    }
    else if (disks[driveNum].size == 327680) {  // 320KB - Timex FDD 3000 both sides
        disks[driveNum].tracks = 40;
        disks[driveNum].sectorsPerTrack = 16;
        disks[driveNum].sectorSize = 256;
        disks[driveNum].doubleDensity = false;
    }
    else {
        // Default to 720KB format
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
    Serial.print("  Tracks: ");
    Serial.print(disks[driveNum].tracks);
    Serial.print(", Sectors: ");
    Serial.print(disks[driveNum].sectorsPerTrack);
    Serial.print(", Size: ");
    Serial.println(disks[driveNum].sectorSize);
    
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
    fdc.lastIndexTime = 0;
    fdc.stepRate = STEP_TIME_6MS;
    
    fdc.writeProtect = false;  // Will be updated per drive
    fdc.motorOn = false;
    fdc.indexPulse = false;
}

// ===================== DRIVE SELECT =====================

void checkDriveSelect() {
    // Check which drive select line is active (active low)
    bool ds0 = (digitalRead(PIN_DS0) == LOW);
    bool ds1 = (digitalRead(PIN_DS1) == LOW);
    
    uint8_t newDrive = activeDrive;
    
    if (ds0 && !ds1) {
        newDrive = 0;  // Drive A: selected
    } 
    else if (!ds0 && ds1) {
        newDrive = 1;  // Drive B: selected
    }
    // If both or neither are active, keep current drive
    
    if (newDrive != activeDrive) {
        activeDrive = newDrive;
        // Update write protect status for newly selected drive
        fdc.writeProtect = disks[activeDrive].writeProtected;
        
        Serial.print("Drive switched to: ");
        Serial.println((char)('A' + activeDrive));
        
        // Update display immediately on drive change
        updateDisplay();
    }
}

// ===================== OLED DISPLAY =====================

void updateDisplay() {
    if (!oledPresent) return;
    
    display.clearDisplay();
    
    // Title bar
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("WD1770 Emulator");
    
    // Status indicator
    if (fdc.busy) {
        display.fillCircle(120, 3, 3, SSD1306_WHITE);  // Activity indicator
    }
    
    // Draw separator line
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    
    // Drive A information
    display.setCursor(0, 14);
    display.setTextSize(1);
    
    // Show which drive is active by system (DS lines) with asterisk
    // Show which drive is selected for UI configuration with ">"
    if (activeDrive == 0) {
        display.print("*");  // System is accessing this drive
    } else {
        display.print(" ");
    }
    
    if (uiSelectedDrive == 0) {
        display.print("> A:");  // UI is configuring this drive
    } else {
        display.print("  A:");
    }
    
    display.setCursor(0, 24);
    if (disks[0].filename[0] != '\0') {
        // Show filename (truncate if too long)
        String fname = String(disks[0].filename);
        if (fname.length() > 21) {
            fname = fname.substring(0, 18) + "...";
        }
        display.print(fname);
    } else {
        display.print("(empty)");
    }
    
    // Drive B information
    display.setCursor(0, 36);
    
    if (activeDrive == 1) {
        display.print("*");  // System is accessing this drive
    } else {
        display.print(" ");
    }
    
    if (uiSelectedDrive == 1) {
        display.print("> B:");  // UI is configuring this drive
    } else {
        display.print("  B:");
    }
    
    display.setCursor(0, 46);
    if (disks[1].filename[0] != '\0') {
        // Show filename (truncate if too long)
        String fname = String(disks[1].filename);
        if (fname.length() > 21) {
            fname = fname.substring(0, 18) + "...";
        }
        display.print(fname);
    } else {
        display.print("(empty)");
    }
    
    // Bottom status bar
    display.drawLine(0, 56, 127, 56, SSD1306_WHITE);
    display.setCursor(0, 58);
    display.setTextSize(1);
    display.print("Img:");
    display.print(totalImages);
    
    // Show current track on system active drive
    if (disks[activeDrive].filename[0] != '\0') {
        display.setCursor(50, 58);
        display.print("T:");
        display.print(fdc.currentTrack);
    }
    
    // Legend
    display.setCursor(85, 58);
    display.print("*=Act >=UI");
    
    display.display();
}

void showImageSelectionMenu() {
    if (!oledPresent) return;
    
    display.clearDisplay();
    
    // Title - show which drive we're selecting for
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Select for Drive ");
    display.print((char)('A' + uiSelectedDrive));
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    
    // Show current selection and surrounding entries
    int startIdx = max(0, currentImageIndex[uiSelectedDrive] - 2);
    int endIdx = min(totalImages - 1, startIdx + 4);
    
    // Adjust start if we're near the end
    if (endIdx - startIdx < 4 && startIdx > 0) {
        startIdx = max(0, endIdx - 4);
    }
    
    int y = 14;
    for (int i = startIdx; i <= endIdx && i < totalImages; i++) {
        display.setCursor(0, y);
        
        // Highlight current selection
        if (i == currentImageIndex[uiSelectedDrive]) {
            display.fillRect(0, y, 128, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            display.print(">");
        } else {
            display.setTextColor(SSD1306_WHITE);
            display.print(" ");
        }
        
        // Show filename (truncate if needed)
        String fname = diskImages[i];
        if (fname.length() > 20) {
            fname = fname.substring(0, 17) + "...";
        }
        display.print(fname);
        
        display.setTextColor(SSD1306_WHITE);  // Reset color
        y += 10;
    }
    
    // Instructions
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

// ===================== READ/WRITE HANDLERS =====================

void handleRead(uint8_t addr) {
    uint8_t value = 0;
    
    switch (addr) {
        case REG_STATUS_CMD:
            value = fdc.status;
            fdc.intrq = false;  // Clear interrupt on status read
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
        // Type I commands
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
        // Type II commands
        if ((cmd & 0x20) == 0) {
            cmdReadSector(cmd);
        } else {
            cmdWriteSector(cmd);
        }
    }
    else if ((cmd & 0x10) == 0) {
        // Type III commands
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
        // Type IV - Force Interrupt
        cmdForceInterrupt(cmd);
    }
}

// Type I Commands
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
    if (fdc.currentTrack >= disk.tracks) {
        fdc.currentTrack = disk.tracks - 1;
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
    
    if (fdc.currentTrack >= disk.tracks) {
        fdc.currentTrack = disk.tracks - 1;
    }
    
    if (cmd & 0x10) {  // Update flag
        fdc.track = fdc.currentTrack;
    }
    
    fdc.status = (fdc.currentTrack == 0) ? ST_TRACK00 : 0;
    fdc.busy = false;
    fdc.intrq = true;
}

// Type II Commands
void cmdReadSector(uint8_t cmd) {
    DiskImage* currentDisk = &disks[activeDrive];
    
    if (fdc.currentTrack >= currentDisk->tracks || fdc.sector == 0 || fdc.sector > currentDisk->sectorsPerTrack) {
        fdc.status = ST_RNF;
        fdc.busy = false;
        fdc.intrq = true;
        return;
    }
    
    // Calculate sector position in file
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
    
    // Auto-increment for multi-sector read
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
    
    // Calculate sector position
    uint32_t offset = (fdc.currentTrack * currentDisk->sectorsPerTrack + (fdc.sector - 1)) * currentDisk->sectorSize;
    
    imageFiles[activeDrive].seek(offset);
    imageFiles[activeDrive].write(fdc.sectorBuffer, currentDisk->sectorSize);
    imageFiles[activeDrive].flush();
    
    fdc.busy = false;
    fdc.intrq = true;
    fdc.status = 0;
    
    digitalWrite(PIN_LED, LOW);
    
    // Auto-increment for multi-sector write
    if ((fdc.command & 0x10) && fdc.sector < currentDisk->sectorsPerTrack) {
        fdc.sector++;
        cmdWriteSector(fdc.command);
    }
}

// Type III Commands
void cmdReadAddress(uint8_t cmd) {
    fdc.sectorBuffer[0] = fdc.currentTrack;
    fdc.sectorBuffer[1] = 0;  // Side
    fdc.sectorBuffer[2] = 1;  // First sector
    fdc.sectorBuffer[3] = (disks[activeDrive].sectorSize == 512) ? 2 : 1;  // Sector size code
    fdc.sectorBuffer[4] = 0;  // CRC1
    fdc.sectorBuffer[5] = 0;  // CRC2
    
    fdc.dataIndex = 0;
    fdc.dataLength = 6;
    fdc.drq = true;
    fdc.status = ST_BUSY | ST_DRQ;
}

void cmdReadTrack(uint8_t cmd) {
    DiskImage* currentDisk = &disks[activeDrive];
    
    // Read entire track
    uint32_t offset = fdc.currentTrack * currentDisk->sectorsPerTrack * currentDisk->sectorSize;
    uint16_t trackSize = currentDisk->sectorsPerTrack * currentDisk->sectorSize;
    
    imageFiles[activeDrive].seek(offset);
    fdc.dataLength = imageFiles[activeDrive].read(fdc.sectorBuffer, min(trackSize, 1024));
    
    fdc.dataIndex = 0;
    fdc.drq = true;
    fdc.status = ST_BUSY | ST_DRQ;
}

void cmdWriteTrack(uint8_t cmd) {
    // Format track - simplified implementation
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
    digitalWrite(PIN_TR00, (fdc.currentTrack == 0) ? HIGH : LOW);
}

void generateIndexPulse() {
    uint32_t now = micros();
    
    if (now - fdc.lastIndexTime >= INDEX_PULSE_TIME) {
        digitalWrite(PIN_IP, HIGH);
        fdc.indexPulse = true;
        fdc.lastIndexTime = now;
    }
    else if (fdc.indexPulse && (now - fdc.lastIndexTime > 1000)) {
        digitalWrite(PIN_IP, LOW);
        fdc.indexPulse = false;
    }
}

void processCommand() {
    // Handle any ongoing command processing
    // This is where you'd implement delays for realistic timing
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
    
    // Rotary encoder rotation - always works on uiSelectedDrive
    if (clk != lastCLK && (millis() - lastDebounce > 5)) {
        if (digitalRead(ROTARY_DT) != clk) {
            // Clockwise
            currentImageIndex[uiSelectedDrive]++;
            if (currentImageIndex[uiSelectedDrive] >= totalImages) {
                currentImageIndex[uiSelectedDrive] = 0;
            }
        } else {
            // Counter-clockwise
            currentImageIndex[uiSelectedDrive]--;
            if (currentImageIndex[uiSelectedDrive] < 0) {
                currentImageIndex[uiSelectedDrive] = totalImages - 1;
            }
        }
        
        Serial.print("UI Drive ");
        Serial.print((char)('A' + uiSelectedDrive));
        Serial.print(" - Browsing: ");
        Serial.println(diskImages[currentImageIndex[uiSelectedDrive]]);
        
        // Show selection menu when rotating
        inSelectionMode = true;
        showImageSelectionMenu();
        
        lastDebounce = millis();
    }
    
    lastCLK = clk;
    
    // Return to normal display after 3 seconds of inactivity
    if (inSelectionMode && (millis() - lastDebounce > 3000)) {
        inSelectionMode = false;
        updateDisplay();
    }
    
    // Button press detection
    if (sw == LOW && !buttonPressed) {
        buttonPressed = true;
        buttonPressTime = millis();
    }
    
    // Button released
    if (sw == HIGH && buttonPressed) {
        unsigned long pressDuration = millis() - buttonPressTime;
        
        if (pressDuration >= 1000) {
            // LONG PRESS: Toggle UI selected drive (A <-> B)
            uiSelectedDrive = 1 - uiSelectedDrive;
            
            Serial.print("UI switched to Drive ");
            Serial.println((char)('A' + uiSelectedDrive));
            
            // Show drive switch message briefly
            showDriveSwitchMessage();
            delay(500);
            updateDisplay();
            
        } else if (pressDuration >= 50) {
            // SHORT PRESS: Load selected image into uiSelectedDrive
            Serial.print("Loading image into Drive ");
            Serial.print((char)('A' + uiSelectedDrive));
            Serial.print(": ");
            Serial.println(diskImages[currentImageIndex[uiSelectedDrive]]);
            
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
    
    // Return to input after a short delay
    for (int i = 0; i < 8; i++) {
        pinMode(dataPins[i], INPUT);
    }
}
