#pragma once

#include <Arduino.h>
#include <SdFat.h>
#include "DiskImage.h"
#include "DiskManager.h"

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
#define SECTOR_READ_TIME    3000
#define SECTOR_WRITE_TIME   3000

// FDC State machine
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
  void setSD(SdFat32* sdCard);
  
  // FDC enable/disable
  bool isEnabled();
  void disable();
  
  // Drive selection
  void checkDriveSelect();
  uint8_t getActiveDrive() const { return activeDrive; }
  
  // Bus interface
  void handleBus();
  
  // State machine
  void processStateMachine();
  
  // Output signals
  void updateOutputs();
  
  // State access
  bool isBusy() const { return fdc.busy; }
  uint8_t getCurrentTrack() const { return fdc.currentTrack; }
  FDCStateEnum getState() const { return fdc.state; }
  
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
