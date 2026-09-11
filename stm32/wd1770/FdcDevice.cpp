#include "FdcDevice.h"

// Pin definitions - these should come from main .ino file
// Declared as extern since they're defined in main
extern uint8_t dataPins[];
extern int WD_D0, WD_D1, WD_D2, WD_D3, WD_D4, WD_D5, WD_D6, WD_D7;
extern int WD_A0, WD_A1, WD_CS, WD_RW;
extern int WD_INTRQ, WD_DRQ;
extern int WD_DDEN, WD_DS0, WD_DS1;

FdcDevice::FdcDevice() {
  diskManager = nullptr;
  sd = nullptr;
  activeDrive = 0;
  lastCS = true;
  lastRW = true;
  dataBusDriven = false;
  dataValidUntil = 0;
  
  memset(&fdc, 0, sizeof(FDCState));
}

void FdcDevice::begin() {
  fdc.status = ST_TRACK00;
  fdc.track = 0;
  fdc.sector = 1;
  fdc.data = 0;
  fdc.command = 0;
  fdc.currentTrack = 0;
  fdc.direction = 1;
  fdc.busy = false;
  fdc.drq = false;
  fdc.intrq = false;
  fdc.doubleDensity = false;
  fdc.dataIndex = 0;
  fdc.dataLength = 0;
  fdc.stepRate = STEP_TIME_6MS;
  fdc.writeProtect = false;
  fdc.motorOn = false;
  fdc.state = STATE_IDLE;
  fdc.sectorsRemaining = 0;
  fdc.multiSector = false;
}

void FdcDevice::setDiskManager(DiskManager* dm) {
  diskManager = dm;
}

void FdcDevice::setSD(SdFat32* sdCard) {
  sd = sdCard;
}

bool FdcDevice::isEnabled() {
  return (digitalRead(WD_DDEN) == LOW);
}

void FdcDevice::disable() {
  if (dataBusDriven) {
    releaseDataBus();
  }
}

void FdcDevice::checkDriveSelect() {
  if (digitalRead(WD_DS0) == HIGH) {
    activeDrive = 0;
  } else if (digitalRead(WD_DS1) == HIGH) {
    activeDrive = 1;
  }
}

uint8_t FdcDevice::readDataBus() {
  uint8_t value = 0;
  for (int i = 0; i < 8; i++) {
    pinMode(dataPins[i], INPUT);
  }
  delayMicroseconds(1);
  for (int i = 0; i < 8; i++) {
    if (digitalRead(dataPins[i])) {
      value |= (1 << i);
    }
  }
  return value;
}

void FdcDevice::driveDataBus(uint8_t data) {
  for (int i = 0; i < 8; i++) {
    pinMode(dataPins[i], OUTPUT);
    digitalWrite(dataPins[i], (data & (1 << i)) ? HIGH : LOW);
  }
  dataBusDriven = true;
  dataValidUntil = micros() + 500;
}

void FdcDevice::releaseDataBus() {
  for (int i = 0; i < 8; i++) {
    pinMode(dataPins[i], INPUT);
  }
  dataBusDriven = false;
}

void FdcDevice::handleBus() {
  bool cs = (digitalRead(WD_CS) == LOW);
  bool rw = (digitalRead(WD_RW) == HIGH);
  
  // CS falling edge - start of transaction
  if (!lastCS && cs) {
    uint8_t addr = (digitalRead(WD_A1) << 1) | digitalRead(WD_A0);
    
    if (rw) {
      // Read operation - CPU reading from WD1770
      handleRead(addr);
    } else {
      // Write operation - CPU writing to WD1770
      uint8_t data = readDataBus();
      fdc.data = data;
      handleWrite(addr);
    }
  }
  
  // CS rising edge - end of transaction
  if (lastCS && !cs) {
    if (dataBusDriven && micros() > dataValidUntil) {
      releaseDataBus();
    }
  }
  
  lastCS = cs;
  lastRW = rw;
}

void FdcDevice::handleRead(uint8_t addr) {
  uint8_t value = 0;
  
  switch (addr) {
    case 0:  // Status register
      value = fdc.status;
      if (fdc.busy) value |= ST_BUSY;
      if (fdc.drq) value |= ST_DRQ;
      fdc.intrq = false;
      break;
      
    case 1:  // Track register
      value = fdc.track;
      break;
      
    case 2:  // Sector register
      value = fdc.sector;
      break;
      
    case 3:  // Data register
      value = fdc.data;
      if (fdc.state == STATE_READING_SECTOR && fdc.dataIndex < fdc.dataLength) {
        value = fdc.sectorBuffer[fdc.dataIndex++];
        fdc.data = value;
        if (fdc.dataIndex >= fdc.dataLength) {
          fdc.drq = false;
          fdc.state = STATE_SECTOR_READ_COMPLETE;
        }
      }
      break;
  }
  
  driveDataBus(value);
}

void FdcDevice::handleWrite(uint8_t addr) {
  switch (addr) {
    case 0:  // Command register
      fdc.command = fdc.data;
      
      // Decode and execute command
      if ((fdc.command & 0xF0) == CMD_RESTORE) {
        cmdRestore();
      } else if ((fdc.command & 0xF0) == CMD_SEEK) {
        cmdSeek();
      } else if ((fdc.command & 0xE0) == CMD_STEP) {
        cmdStep();
      } else if ((fdc.command & 0xE0) == CMD_STEP_IN) {
        cmdStepIn();
      } else if ((fdc.command & 0xE0) == CMD_STEP_OUT) {
        cmdStepOut();
      } else if ((fdc.command & 0xF0) == CMD_READ_SECTOR || 
                 (fdc.command & 0xF0) == CMD_READ_SECTORS) {
        cmdReadSector();
      } else if ((fdc.command & 0xF0) == CMD_WRITE_SECTOR ||
                 (fdc.command & 0xF0) == CMD_WRITE_SECTORS) {
        cmdWriteSector();
      } else if ((fdc.command & 0xF0) == CMD_READ_ADDRESS) {
        cmdReadAddress();
      } else if ((fdc.command & 0xF0) == CMD_FORCE_INT) {
        cmdForceInterrupt();
      }
      break;
      
    case 1:  // Track register
      fdc.track = fdc.data;
      break;
      
    case 2:  // Sector register
      fdc.sector = fdc.data;
      break;
      
    case 3:  // Data register
      if (fdc.state == STATE_WAITING_FOR_DATA_IN && fdc.dataIndex < fdc.dataLength) {
        fdc.sectorBuffer[fdc.dataIndex++] = fdc.data;
        if (fdc.dataIndex >= fdc.dataLength) {
          fdc.drq = false;
          fdc.state = STATE_WRITING_SECTOR;
          writeSectorData();
        }
      }
      break;
  }
}

uint32_t FdcDevice::getStepRate() {
  uint8_t rateCode = fdc.command & 0x03;
  switch (rateCode) {
    case 0: return STEP_TIME_6MS;
    case 1: return STEP_TIME_12MS;
    case 2: return STEP_TIME_20MS;
    case 3: return STEP_TIME_30MS;
  }
  return STEP_TIME_6MS;
}

void FdcDevice::cmdRestore() {
  fdc.busy = true;
  fdc.status = ST_BUSY;
  fdc.currentTrack = 0;
  fdc.track = 0;
  fdc.direction = -1;
  fdc.state = STATE_SEEKING;
  fdc.stepRate = getStepRate();
  fdc.operationStartTime = micros();
}

void FdcDevice::cmdSeek() {
  fdc.busy = true;
  fdc.status = ST_BUSY;
  fdc.direction = (fdc.data > fdc.currentTrack) ? 1 : -1;
  fdc.state = STATE_SEEKING;
  fdc.stepRate = getStepRate();
  fdc.operationStartTime = micros();
}

void FdcDevice::cmdStep() {
  fdc.busy = true;
  fdc.status = ST_BUSY;
  fdc.state = STATE_SEEKING;
  fdc.stepRate = getStepRate();
  fdc.operationStartTime = micros();
}

void FdcDevice::cmdStepIn() {
  fdc.busy = true;
  fdc.status = ST_BUSY;
  fdc.direction = 1;
  fdc.state = STATE_SEEKING;
  fdc.stepRate = getStepRate();
  fdc.operationStartTime = micros();
}

void FdcDevice::cmdStepOut() {
  fdc.busy = true;
  fdc.status = ST_BUSY;
  fdc.direction = -1;
  fdc.state = STATE_SEEKING;
  fdc.stepRate = getStepRate();
  fdc.operationStartTime = micros();
}

void FdcDevice::cmdReadSector() {
  if (!diskManager) {
    fdc.status = ST_RNF;
    fdc.intrq = true;
    return;
  }
  
  DiskImage* currentDisk = diskManager->getDisk(activeDrive);
  if (!currentDisk || currentDisk->size == 0) {
    fdc.status = ST_RNF;
    fdc.intrq = true;
    return;
  }
  
  fdc.busy = true;
  fdc.status = ST_BUSY;
  fdc.multiSector = ((fdc.command & 0xF0) == CMD_READ_SECTORS);
  fdc.sectorsRemaining = fdc.multiSector ? currentDisk->sectorsPerTrack : 1;
  fdc.state = STATE_READING_SECTOR;
  fdc.operationStartTime = micros();
  
  readSectorData();
}

void FdcDevice::cmdWriteSector() {
  if (!diskManager) {
    fdc.status = ST_RNF;
    fdc.intrq = true;
    return;
  }
  
  DiskImage* currentDisk = diskManager->getDisk(activeDrive);
  if (!currentDisk || currentDisk->size == 0) {
    fdc.status = ST_RNF;
    fdc.intrq = true;
    return;
  }
  
  if (currentDisk->writeProtected) {
    fdc.status = ST_WRITE_PROTECT;
    fdc.intrq = true;
    return;
  }
  
  fdc.busy = true;
  fdc.status = ST_BUSY;
  fdc.multiSector = ((fdc.command & 0xF0) == CMD_WRITE_SECTORS);
  fdc.sectorsRemaining = fdc.multiSector ? currentDisk->sectorsPerTrack : 1;
  fdc.dataIndex = 0;
  fdc.dataLength = currentDisk->sectorSize;
  fdc.drq = true;
  fdc.state = STATE_WAITING_FOR_DATA_IN;
  fdc.operationStartTime = micros();
}

void FdcDevice::cmdReadAddress() {
  fdc.sectorBuffer[0] = fdc.currentTrack;
  fdc.sectorBuffer[1] = 0;
  fdc.sectorBuffer[2] = 1;
  fdc.sectorBuffer[3] = 2;
  fdc.sectorBuffer[4] = 0;
  fdc.sectorBuffer[5] = 0;
  
  fdc.dataIndex = 0;
  fdc.dataLength = 6;
  fdc.drq = true;
  fdc.busy = true;
  fdc.status = ST_BUSY;
  fdc.state = STATE_READING_SECTOR;
}

void FdcDevice::cmdForceInterrupt() {
  fdc.busy = false;
  fdc.drq = false;
  fdc.intrq = true;
  fdc.state = STATE_IDLE;
  fdc.status = 0;
}

void FdcDevice::readSectorData() {
  if (!diskManager || !sd) return;
  
  DiskImage* currentDisk = diskManager->getDisk(activeDrive);
  if (!currentDisk || currentDisk->size == 0) {
    fdc.status = ST_RNF;
    fdc.busy = false;
    fdc.intrq = true;
    fdc.state = STATE_IDLE;
    return;
  }
  
  // Validate sector
  if (fdc.sector < 1 || fdc.sector > currentDisk->sectorsPerTrack) {
    fdc.status = ST_RNF;
    fdc.busy = false;
    fdc.intrq = true;
    fdc.state = STATE_IDLE;
    return;
  }
  
  // Calculate offset
  uint32_t offset;
  char filename[70];
  snprintf(filename, sizeof(filename), "/%s", currentDisk->filename);
  
  if (currentDisk->isExtendedDSK) {
    uint32_t trackSize = currentDisk->trackHeaderSize + 
                         (currentDisk->sectorsPerTrack * currentDisk->sectorSize);
    offset = currentDisk->headerOffset +
             (fdc.currentTrack * trackSize) +
             currentDisk->trackHeaderSize +
             ((fdc.sector - 1) * currentDisk->sectorSize);
  } else {
    offset = (fdc.currentTrack * currentDisk->sectorsPerTrack + (fdc.sector - 1)) * 
             currentDisk->sectorSize;
  }
  
  // Read sector
  File32 imageFile = sd->open(filename, O_READ);
  if (!imageFile) {
    fdc.status = ST_RNF;
    fdc.busy = false;
    fdc.intrq = true;
    fdc.state = STATE_IDLE;
    return;
  }
  
  imageFile.seek(offset);
  size_t bytesRead = imageFile.read(fdc.sectorBuffer, currentDisk->sectorSize);
  imageFile.close();
  
  if (bytesRead != currentDisk->sectorSize) {
    fdc.status = ST_RNF;
    fdc.busy = false;
    fdc.intrq = true;
    fdc.state = STATE_IDLE;
    return;
  }
  
  fdc.dataIndex = 0;
  fdc.dataLength = currentDisk->sectorSize;
  fdc.drq = true;
  fdc.status = ST_BUSY | ST_DRQ;
  fdc.state = STATE_READING_SECTOR;
}

void FdcDevice::writeSectorData() {
  if (!diskManager || !sd) return;
  
  DiskImage* currentDisk = diskManager->getDisk(activeDrive);
  if (!currentDisk || currentDisk->size == 0) {
    fdc.status = ST_RNF;
    fdc.busy = false;
    fdc.intrq = true;
    fdc.state = STATE_IDLE;
    return;
  }
  
  // Calculate offset
  uint32_t offset;
  char filename[70];
  snprintf(filename, sizeof(filename), "/%s", currentDisk->filename);
  
  if (currentDisk->isExtendedDSK) {
    uint32_t trackSize = currentDisk->trackHeaderSize +
                         (currentDisk->sectorsPerTrack * currentDisk->sectorSize);
    offset = currentDisk->headerOffset +
             (fdc.currentTrack * trackSize) +
             currentDisk->trackHeaderSize +
             ((fdc.sector - 1) * currentDisk->sectorSize);
  } else {
    offset = (fdc.currentTrack * currentDisk->sectorsPerTrack + (fdc.sector - 1)) *
             currentDisk->sectorSize;
  }
  
  // Write sector
  File32 imageFile = sd->open(filename, O_WRITE);
  if (!imageFile) {
    fdc.status = ST_WRITE_PROTECT;
    fdc.busy = false;
    fdc.intrq = true;
    fdc.state = STATE_IDLE;
    return;
  }
  
  imageFile.seek(offset);
  imageFile.write(fdc.sectorBuffer, currentDisk->sectorSize);
  imageFile.flush();
  delay(10);
  imageFile.close();
  delay(5);
  
  fdc.state = STATE_SECTOR_WRITE_COMPLETE;
}

void FdcDevice::processStateMachine() {
  uint32_t now = micros();
  
  switch (fdc.state) {
    case STATE_IDLE:
      // Nothing to do
      break;
      
    case STATE_SEEKING:
      if (now - fdc.operationStartTime >= fdc.stepRate) {
        if ((fdc.command & 0xF0) == CMD_RESTORE) {
          fdc.currentTrack = 0;
          fdc.track = 0;
          fdc.status = ST_TRACK00;
          fdc.busy = false;
          fdc.intrq = true;
          fdc.state = STATE_IDLE;
        } else if ((fdc.command & 0xF0) == CMD_SEEK) {
          fdc.currentTrack = fdc.data;
          if (fdc.command & 0x10) {
            fdc.track = fdc.currentTrack;
          }
          fdc.status = (fdc.currentTrack == 0) ? ST_TRACK00 : 0;
          fdc.busy = false;
          fdc.intrq = true;
          fdc.state = STATE_IDLE;
        } else {
          // STEP, STEP_IN, STEP_OUT
          fdc.currentTrack += fdc.direction;
          if (fdc.currentTrack < 0) fdc.currentTrack = 0;
          if (fdc.currentTrack > MAX_TRACKS) fdc.currentTrack = MAX_TRACKS;
          
          if (fdc.command & 0x10) {
            fdc.track = fdc.currentTrack;
          }
          fdc.status = (fdc.currentTrack == 0) ? ST_TRACK00 : 0;
          fdc.busy = false;
          fdc.intrq = true;
          fdc.state = STATE_IDLE;
        }
      }
      break;
      
    case STATE_READING_SECTOR:
      // Wait for CPU to read all data via DRQ
      break;
      
    case STATE_SECTOR_READ_COMPLETE:
      if (fdc.multiSector && fdc.sectorsRemaining > 1) {
        fdc.sectorsRemaining--;
        fdc.sector++;
        readSectorData();
      } else {
        fdc.busy = false;
        fdc.drq = false;
        fdc.intrq = true;
        fdc.status = 0;
        fdc.state = STATE_IDLE;
      }
      break;
      
    case STATE_WAITING_FOR_DATA_IN:
      // Wait for CPU to write all data via DRQ
      break;
      
    case STATE_WRITING_SECTOR:
      // Writing handled in writeSectorData()
      break;
      
    case STATE_SECTOR_WRITE_COMPLETE:
      if (fdc.multiSector && fdc.sectorsRemaining > 1) {
        fdc.sectorsRemaining--;
        fdc.sector++;
        fdc.dataIndex = 0;
        fdc.drq = true;
        fdc.state = STATE_WAITING_FOR_DATA_IN;
      } else {
        fdc.busy = false;
        fdc.drq = false;
        fdc.intrq = true;
        fdc.status = 0;
        fdc.state = STATE_IDLE;
      }
      break;
  }
}

void FdcDevice::updateOutputs() {
  digitalWrite(WD_INTRQ, fdc.intrq ? HIGH : LOW);
  digitalWrite(WD_DRQ, fdc.drq ? HIGH : LOW);
}
