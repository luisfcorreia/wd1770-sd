# Code Refactoring Plan: Multi-File Architecture

## Overview
Split monolithic `wd1770-emu.ino` (~1900 lines) into modular, maintainable components.

---

## File Structure

```
wd1770-emu/
├── wd1770-emu.ino          # Main entry point (~150 lines)
├── OledUI.h                # UI interface (~50 lines)
├── OledUI.cpp              # UI implementation (~450 lines)
├── DiskManager.h           # Disk operations interface (~40 lines)
├── DiskManager.cpp         # Disk operations implementation (~350 lines)
├── DiskImage.h             # Disk image structure (~30 lines)
├── FdcDevice.h             # FDC interface (~60 lines)
└── FdcDevice.cpp           # FDC implementation (~900 lines)
```

---

## 1. wd1770-emu.ino (Main File)

### Purpose
Entry point, initialization, and main loop coordination.

### Contents
```cpp
#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include "OledUI.h"
#include "DiskManager.h"
#include "FdcDevice.h"

// Pin definitions (WD_, BTN_, OLED_, SD_)
#define TEST_MODE 1
// ... all pin definitions ...

// Global objects
SdFat32 SD;
OledUI ui;
DiskManager diskManager;
FdcDevice fdc;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  initPins();
  initSDCard();
  
  ui.begin();
  diskManager.begin(&SD);
  fdc.begin();
  
  diskManager.scanImages();
  diskManager.loadLastConfig();
  
  ui.updateDisplay();
  Serial.println("Ready!");
}

void loop() {
  ui.checkInput();           // Handle button presses
  
  if (fdc.isEnabled()) {     // Check WD_DDEN
    fdc.checkDriveSelect();
    fdc.handleBus();
    fdc.processStateMachine();
    fdc.updateOutputs();
  } else {
    fdc.disable();
  }
  
  ui.periodicUpdate();       // Update display every 100ms
}

void initPins() { ... }
void initSDCard() { ... }
```

### Responsibilities
- Pin configuration
- Hardware initialization (SD, SPI, pins)
- Object instantiation
- Main loop coordination
- TEST_MODE configuration

### Lines: ~150

---

## 2. OledUI.h

### Purpose
User interface abstraction - display and button input handling.

### Contents
```cpp
#ifndef OLED_UI_H
#define OLED_UI_H

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
};

#endif
```

### Responsibilities
- OLED initialization
- Display rendering (all 4 UI modes)
- Button input with debouncing
- UI state machine
- Display update timing

### Lines: ~50 header, ~450 implementation

---

## 3. DiskManager.h

### Purpose
Disk image file management, loading, and configuration persistence.

### Contents
```cpp
#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

#include <SdFat.h>
#include "DiskImage.h"

#define MAX_DISK_IMAGES 100

class DiskManager {
public:
  DiskManager();
  
  // Initialization
  bool begin(SdFat32* sd);
  
  // Image scanning
  void scanImages();
  int getTotalImages() const { return totalImages; }
  const char* getImageName(int index) const;
  
  // Image loading
  bool loadImage(uint8_t drive, int imageIndex);
  void ejectDrive(uint8_t drive);
  
  // Configuration persistence
  void saveConfig();
  void loadConfig();
  
  // Access to loaded images
  DiskImage* getDisk(uint8_t drive);
  int getLoadedIndex(uint8_t drive) const;
  
private:
  SdFat32* sd;
  
  // Image list
  char diskImages[MAX_DISK_IMAGES][64];
  int totalImages;
  int loadedImageIndex[2];
  
  // Loaded disk data
  DiskImage disks[2];
  
  // Format detection
  bool detectFormat(DiskImage* disk, uint32_t size);
  bool parseExtendedDSK(uint8_t drive, const char* filename);
};

#endif
```

### Responsibilities
- Scan SD card for disk images
- Load/eject disk images
- Format auto-detection
- Extended DSK header parsing
- Configuration file management
- Image list management

### Lines: ~40 header, ~350 implementation

---

## 4. DiskImage.h

### Purpose
Data structure for disk image metadata.

### Contents
```cpp
#ifndef DISK_IMAGE_H
#define DISK_IMAGE_H

#include <Arduino.h>

// Disk image metadata
typedef struct {
  char filename[64];
  uint32_t size;
  uint8_t tracks;
  uint8_t sectorsPerTrack;
  uint16_t sectorSize;
  bool doubleDensity;
  bool writeProtected;
  bool isExtendedDSK;
  uint16_t headerOffset;
  uint16_t trackHeaderSize;
} DiskImage;

// Common disk formats (for auto-detection)
#define SIZE_TIMEX_FDD3000_SS   163840   // 160KB: 40T/16S/256B
#define SIZE_TIMEX_FDD3000_DS   327680   // 320KB: 80T/16S/256B
#define SIZE_CPC_40T            184320   // 180KB: 40T/9S/512B
#define SIZE_35_DD              737280   // 720KB: 80T/9S/512B
#define SIZE_525_DD             368640   // 360KB: 40T/9S/512B

#endif
```

### Responsibilities
- Disk image structure definition
- Common format constants
- Shared data types

### Lines: ~30

---

## 5. FdcDevice.h

### Purpose
WD1770/1772 floppy disk controller emulation.

### Contents
```cpp
#ifndef FDC_DEVICE_H
#define FDC_DEVICE_H

#include <Arduino.h>
#include <SdFat.h>
#include "DiskImage.h"

// FDC State machine
typedef enum {
  STATE_IDLE,
  STATE_SEEKING,
  STATE_READING,
  STATE_WRITING
} FDCStateEnum;

// FDC registers and state
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

class FdcDevice {
public:
  FdcDevice();
  
  // Initialization
  void begin();
  void setDiskManager(DiskManager* dm);
  void setSD(SdFat32* sd);
  
  // FDC enable/disable
  bool isEnabled();
  void disable();
  
  // Drive selection
  void checkDriveSelect();
  
  // Bus interface
  void handleBus();
  
  // State machine
  void processStateMachine();
  
  // Output signals
  void updateOutputs();
  
  // State access
  bool isBusy() const { return fdc.busy; }
  uint8_t getCurrentTrack() const { return fdc.currentTrack; }
  
private:
  FDCState fdc;
  DiskManager* diskManager;
  SdFat32* sd;
  uint8_t activeDrive;
  
  // Bus state tracking
  bool lastCS;
  bool lastRW;
  bool dataBusDriven;
  uint32_t dataValidUntil;
  
  // Register access
  void handleRead(uint8_t addr);
  void handleWrite(uint8_t addr);
  
  // Data bus control
  void driveDataBus(uint8_t data);
  void releaseDataBus();
  uint8_t readDataBus();
  
  // Command handlers
  void cmdRestore();
  void cmdSeek();
  void cmdStep();
  void cmdStepIn();
  void cmdStepOut();
  void cmdReadSector();
  void cmdWriteSector();
  void cmdReadAddress();
  void cmdForceInterrupt();
  
  // Sector I/O
  void readSectorData();
  void writeSectorData();
  
  // Timing
  uint32_t getStepRate();
};

#endif
```

### Responsibilities
- WD1770 register emulation
- Command processing
- State machine execution
- Bus protocol (CS, R/W, data lines)
- Sector read/write operations
- INTRQ/DRQ signal generation
- Drive select handling
- Timing emulation

### Lines: ~60 header, ~900 implementation

---

## Migration Strategy

### Phase 1: Create Header Files
1. Create all .h files with class/struct declarations
2. Define interfaces and public methods
3. Set up proper include guards

### Phase 2: Extract and Test Each Module

**Order of implementation:**

1. **DiskImage.h** (no dependencies)
   - Just structure definitions
   - No implementation file needed

2. **DiskManager** (depends on DiskImage)
   - Extract scanDiskImages(), loadDiskImage(), config functions
   - Test: Verify images load correctly

3. **FdcDevice** (depends on DiskImage, DiskManager)
   - Extract all FDC logic and commands
   - Test: Verify FDC state machine still works

4. **OledUI** (depends on DiskImage, DiskManager, FdcDevice)
   - Extract all display and button handling
   - Test: Verify UI still navigates correctly

5. **wd1770-emu.ino** (depends on all)
   - Slim down to just initialization and loop
   - Test: Full integration test

### Phase 3: Refine Interfaces
- Review and optimize class interfaces
- Add getter/setter methods as needed
- Ensure proper encapsulation

### Phase 4: Documentation
- Add Doxygen-style comments
- Document each class's responsibilities
- Create architecture diagram

---

## Detailed Function Mapping

### OledUI.cpp Functions
```
From current code:
- displayNormalMode()           → OledUI::displayNormalMode()
- displaySelectingDriveA()      → OledUI::displaySelectingDriveA()
- displaySelectingDriveB()      → OledUI::displaySelectingDriveB()
- displayConfirm()              → OledUI::displayConfirm()
- checkImageSelection()         → OledUI::checkInput()
- updateDisplay()               → OledUI::updateDisplay()
```

### DiskManager.cpp Functions
```
From current code:
- scanDiskImages()              → DiskManager::scanImages()
- loadDiskImage()               → DiskManager::loadImage()
- saveLastImageConfig()         → DiskManager::saveConfig()
- loadLastImageConfig()         → DiskManager::loadConfig()
- parseExtendedDSKHeader()      → DiskManager::parseExtendedDSK()
```

### FdcDevice.cpp Functions
```
From current code:
- initFDC()                     → FdcDevice::begin()
- handleRead()                  → FdcDevice::handleRead()
- handleWrite()                 → FdcDevice::handleWrite()
- cmdRestore()                  → FdcDevice::cmdRestore()
- cmdSeek()                     → FdcDevice::cmdSeek()
- cmdStep()                     → FdcDevice::cmdStep()
- cmdStepIn()                   → FdcDevice::cmdStepIn()
- cmdStepOut()                  → FdcDevice::cmdStepOut()
- cmdReadSector()               → FdcDevice::cmdReadSector()
- cmdWriteSector()              → FdcDevice::cmdWriteSector()
- cmdReadAddress()              → FdcDevice::cmdReadAddress()
- cmdForceInterrupt()           → FdcDevice::cmdForceInterrupt()
- processStateMachine()         → FdcDevice::processStateMachine()
- updateOutputs()               → FdcDevice::updateOutputs()
- readSectorData()              → FdcDevice::readSectorData()
- writeSectorData()             → FdcDevice::writeSectorData()
- checkDriveSelect()            → FdcDevice::checkDriveSelect()
- driveDataBus()                → FdcDevice::driveDataBus()
- releaseDataBus()              → FdcDevice::releaseDataBus()
- readDataBus()                 → FdcDevice::readDataBus()
```

---

## Data Flow

```
┌─────────────┐
│wd1770-emu   │  Main entry, hardware init
│   .ino      │
└──────┬──────┘
       │ creates & coordinates
       ├─────────────────┬─────────────────┬──────────────┐
       │                 │                 │              │
┌──────▼──────┐   ┌──────▼──────┐   ┌─────▼─────┐   ┌──▼──┐
│   OledUI    │   │DiskManager  │   │FdcDevice  │   │ SD  │
│             │   │             │   │           │   │Card │
│ - Display   │   │ - Scan imgs │   │ - FDC     │   └─────┘
│ - Buttons   │◄──┤ - Load imgs │◄──┤   state   │
│ - UI state  │   │ - Config    │   │ - Commands│
└─────────────┘   └─────────────┘   └───────────┘
      │                  │                  │
      │ uses             │ uses             │ uses
      ▼                  ▼                  ▼
┌─────────────────────────────────────────────┐
│            DiskImage.h (structs)            │
└─────────────────────────────────────────────┘
```

---

## Benefits of Refactoring

### 1. Maintainability
- Each module has single responsibility
- Changes isolated to relevant files
- Easier to debug specific subsystems

### 2. Testability
- Can test FDC logic independently
- Can test UI without real hardware
- Can mock disk operations

### 3. Reusability
- FdcDevice could be used in other projects
- DiskManager portable to other systems
- OledUI adaptable to different controllers

### 4. Scalability
- Easy to add new disk formats (DiskManager)
- Easy to add new FDC commands (FdcDevice)
- Easy to add new UI modes (OledUI)

### 5. Readability
- Clear separation of concerns
- Logical file organization
- Reduced cognitive load

---

## Potential Issues and Solutions

### Issue 1: Circular Dependencies
**Problem:** OledUI needs DiskManager, DiskManager needs to trigger UI updates

**Solution:** 
- Use forward declarations
- Pass pointers in initialization, not constructors
- Use callback functions or interfaces

### Issue 2: Global State
**Problem:** Multiple modules need access to same data

**Solution:**
- Create explicit setter methods
- Pass pointers to shared resources (SD, disks)
- Use dependency injection pattern

### Issue 3: Arduino IDE Limitations
**Problem:** IDE expects .ino file in directory with same name

**Solution:**
- Keep directory name as wd1770-emu/
- Main file must be wd1770-emu.ino
- Other .cpp/.h files work normally

### Issue 4: Compilation Order
**Problem:** Arduino may compile files in wrong order

**Solution:**
- Include all headers in main .ino file
- Use proper include guards
- Test compilation after each module

---

## Testing Strategy

### Unit Testing Approach
For each module, verify:

1. **DiskManager**
   - Scan finds all images
   - Load reads correct data
   - Config saves/loads properly

2. **FdcDevice**
   - Commands execute correctly
   - State machine transitions properly
   - Timing delays accurate

3. **OledUI**
   - All display modes render
   - Button input recognized
   - Debouncing works

### Integration Testing
- Load images and verify FDC can read sectors
- Navigate UI and verify disk loads
- Power cycle and verify config persistence

---

## Implementation Checklist

- [ ] Create DiskImage.h
- [ ] Create DiskManager.h/.cpp
- [ ] Test DiskManager standalone
- [ ] Create FdcDevice.h/.cpp
- [ ] Test FdcDevice standalone
- [ ] Create OledUI.h/.cpp
- [ ] Test OledUI standalone
- [ ] Refactor wd1770-emu.ino
- [ ] Integration test
- [ ] Update documentation
- [ ] Verify memory usage unchanged
- [ ] Verify flash usage unchanged
- [ ] Full functionality test

---

## Estimated Effort

- Phase 1 (Headers): 2-3 hours
- Phase 2 (Implementation): 6-8 hours
- Phase 3 (Refinement): 2-3 hours
- Phase 4 (Documentation): 1-2 hours
- Testing: 2-3 hours

**Total:** 13-19 hours for complete refactoring

---

## Notes

- Keep existing TEST_MODE functionality
- Maintain all debug Serial output
- Preserve current behavior exactly
- Don't change pin assignments
- Keep SdFat library integration
- Maintain button debouncing logic

---

**Status:** Design Complete - Ready for Implementation
**Next Step:** Begin with DiskImage.h creation
