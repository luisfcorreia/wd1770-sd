#include "DiskManager.h"

DiskManager::DiskManager() {
  sd = nullptr;
  totalImages = 0;
  loadedImageIndex[0] = -1;
  loadedImageIndex[1] = -1;
  
  // Initialize disk structures
  for (int i = 0; i < MAX_DRIVES; i++) {
    disks[i].filename[0] = '\0';
    disks[i].size = 0;
    disks[i].tracks = 0;
    disks[i].sectorsPerTrack = 0;
    disks[i].sectorSize = 0;
    disks[i].doubleDensity = false;
    disks[i].writeProtected = false;
    disks[i].isExtendedDSK = false;
    disks[i].headerOffset = 0;
    disks[i].trackHeaderSize = 0;
  }
}

bool DiskManager::begin(SdFat32* sdCard) {
  sd = sdCard;
  if (!sd) {
    Serial.println("DiskManager: Invalid SD card pointer");
    return false;
  }
  return true;
}

void DiskManager::scanImages() {
  File32 root = sd->open("/");
  if (!root) {
    Serial.println("Failed to open root directory");
    return;
  }

  totalImages = 0;
  while (true) {
    File32 entry = root.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory()) {
      char filename[64];
      entry.getName(filename, sizeof(filename));
      
      // Uppercase for comparison
      char upper[64];
      strncpy(upper, filename, 63);
      for (int j = 0; upper[j]; j++) upper[j] = toupper(upper[j]);

      if (strstr(upper, ".DSK") || strstr(upper, ".IMG") ||
          strstr(upper, ".ST")  || strstr(upper, ".HFE")) {
        if (totalImages < MAX_DISK_IMAGES) {
          strncpy(diskImages[totalImages], filename, 63);
          diskImages[totalImages][63] = '\0';
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

const char* DiskManager::getImageName(int index) const {
  if (index >= 0 && index < totalImages) {
    return diskImages[index];
  }
  return nullptr;
}

bool DiskManager::loadImage(uint8_t drive, int imageIndex) {
  if (drive >= MAX_DRIVES || imageIndex >= totalImages || imageIndex < 0) {
    return false;
  }

  char filename[70];
  snprintf(filename, sizeof(filename), "/%s", diskImages[imageIndex]);
  
  // Open file temporarily to get metadata
  File32 imageFile = sd->open(filename, O_READ);
  if (!imageFile) {
    Serial.print("Failed to open: ");
    Serial.println(filename);
    return false;
  }

  DiskImage* disk = &disks[drive];
  strncpy(disk->filename, diskImages[imageIndex], 63);
  disk->filename[63] = '\0';
  disk->size = imageFile.size();
  imageFile.close();

  // Detect format by size
  if (!detectFormat(disk, disk->size)) {
    Serial.println("  Warning: Unknown disk format");
  }

  disk->writeProtected = false;
  disk->isExtendedDSK = false;
  disk->headerOffset = 0;
  disk->trackHeaderSize = 0;
  loadedImageIndex[drive] = imageIndex;
  
  // Check for Extended DSK header
  char extCheck[70];
  strncpy(extCheck, filename, 69);
  for (int i = 0; extCheck[i]; i++) extCheck[i] = toupper(extCheck[i]);
  
  if (strstr(extCheck, ".DSK") || strstr(extCheck, ".HFE")) {
    if (parseExtendedDSK(drive, filename)) {
      Serial.println("  Extended DSK header parsed successfully");
    }
  }

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

void DiskManager::ejectDrive(uint8_t drive) {
  if (drive >= MAX_DRIVES) return;
  
  disks[drive].filename[0] = '\0';
  disks[drive].size = 0;
  loadedImageIndex[drive] = -1;
  
  Serial.print("Drive ");
  Serial.print(drive);
  Serial.println(" ejected");
}

bool DiskManager::detectFormat(DiskImage* disk, uint32_t fileSize) {
  // PRIORITY: Timex FDD 3000 (most common format for this project)
  if (fileSize == SIZE_TIMEX_FDD3000_SS) {  // 160KB - Single-Sided
    disk->tracks = 40;
    disk->sectorsPerTrack = 16;
    disk->sectorSize = 256;
    disk->doubleDensity = false;
    Serial.println("  Format: Timex FDD 3000 (40T/16S/256B)");
    return true;
  }
  
  if (fileSize == SIZE_TIMEX_FDD3000_DS) {  // 320KB - Double-Sided
    disk->tracks = 80;
    disk->sectorsPerTrack = 16;
    disk->sectorSize = 256;
    disk->doubleDensity = false;
    Serial.println("  Format: Timex FDD 3000 DS (80T/16S/256B)");
    return true;
  }
  
  if (fileSize == SIZE_35_DD) {  // 720KB - 3.5" DD
    disk->tracks = 80;
    disk->sectorsPerTrack = 9;
    disk->sectorSize = 512;
    disk->doubleDensity = true;
    Serial.println("  Format: 3.5\" DD (80T/9S/512B)");
    return true;
  }
  
  if (fileSize == SIZE_525_DD) {  // 360KB - 5.25" DD
    disk->tracks = 40;
    disk->sectorsPerTrack = 9;
    disk->sectorSize = 512;
    disk->doubleDensity = true;
    Serial.println("  Format: 5.25\" DD (40T/9S/512B)");
    return true;
  }
  
  if (fileSize == SIZE_CPC_40T) {  // 180KB - Amstrad/Spectrum
    disk->tracks = 40;
    disk->sectorsPerTrack = 9;
    disk->sectorSize = 512;
    disk->doubleDensity = true;
    Serial.println("  Format: Amstrad/Spectrum raw (40T/9S/512B)");
    return true;
  }
  
  if (fileSize == 174336) {  // 170KB - Extended DSK
    disk->tracks = 40;
    disk->sectorsPerTrack = 9;
    disk->sectorSize = 512;
    disk->doubleDensity = true;
    Serial.println("  Format: Extended DSK (Amstrad/Spectrum)");
    return true;
  }
  
  // Unknown format - try to guess
  Serial.print("  Warning: Unknown size ");
  Serial.print(fileSize);
  Serial.println(" bytes");
  
  // Try Timex format first (256-byte sectors)
  uint32_t sectors256 = fileSize / 256;
  if (sectors256 == 640) {  // 40 × 16
    disk->tracks = 40;
    disk->sectorsPerTrack = 16;
    disk->sectorSize = 256;
    disk->doubleDensity = false;
    Serial.println("  Guessing: Timex format (40T/16S/256B)");
    return true;
  }
  
  if (sectors256 == 1280) {  // 80 × 16
    disk->tracks = 80;
    disk->sectorsPerTrack = 16;
    disk->sectorSize = 256;
    disk->doubleDensity = false;
    Serial.println("  Guessing: Timex DS format (80T/16S/256B)");
    return true;
  }
  
  // Fall back to 512-byte sectors
  uint32_t sectors512 = fileSize / 512;
  if (sectors512 < 720) {
    disk->tracks = 40;
    disk->sectorsPerTrack = sectors512 / 40;
  } else {
    disk->tracks = 80;
    disk->sectorsPerTrack = sectors512 / 80;
  }
  disk->sectorSize = 512;
  disk->doubleDensity = true;
  Serial.print("  Guessing: ");
  Serial.print(disk->tracks);
  Serial.print("T/");
  Serial.print(disk->sectorsPerTrack);
  Serial.println("S/512B");
  
  return false;  // Return false for unknown format
}

bool DiskManager::parseExtendedDSK(uint8_t drive, const char* filename) {
  File32 imageFile = sd->open(filename, O_READ);
  if (!imageFile) {
    return false;
  }
  
  // Read first 256 bytes (Disk Information Block)
  uint8_t diskHeader[256];
  if (imageFile.read(diskHeader, 256) != 256) {
    imageFile.close();
    return false;
  }
  
  // Check for Extended DSK signature
  if (strncmp((char*)diskHeader, "EXTENDED CPC DSK", 16) != 0 &&
      strncmp((char*)diskHeader, "MV - CPCEMU Disk", 16) != 0) {
    imageFile.close();
    return false;
  }
  
  DiskImage* disk = &disks[drive];
  
  // Parse Disk Information Block
  disk->tracks = diskHeader[0x30];
  uint8_t sides = diskHeader[0x31];
  
  // Read first Track Information Block
  uint8_t trackHeader[256];
  if (imageFile.read(trackHeader, 256) != 256) {
    imageFile.close();
    return false;
  }
  imageFile.close();
  
  // Parse Track Information Block
  if (strncmp((char*)trackHeader, "Track-Info", 10) != 0) {
    Serial.println("  Warning: Invalid Track-Info signature");
    return false;
  }
  
  disk->sectorsPerTrack = trackHeader[0x15];
  uint8_t sectorSizeCode = trackHeader[0x14];
  disk->sectorSize = 128 << sectorSizeCode;
  
  // Set Extended DSK flags
  disk->isExtendedDSK = true;
  disk->headerOffset = 256;
  disk->trackHeaderSize = 256;
  disk->doubleDensity = (disk->sectorSize >= 512);
  
  Serial.print("  Extended DSK: ");
  Serial.print(disk->tracks);
  Serial.print("T/");
  Serial.print(disk->sectorsPerTrack);
  Serial.print("S/");
  Serial.print(disk->sectorSize);
  Serial.print("B, ");
  Serial.print(sides);
  Serial.print(" side(s)");
  
  // Identify likely format
  if (disk->sectorSize == 256 && disk->sectorsPerTrack == 16) {
    Serial.println(" [Timex FDD 3000]");
  } else if (disk->sectorSize == 512 && disk->sectorsPerTrack == 9) {
    Serial.println(" [Amstrad CPC/Spectrum +3]");
  } else {
    Serial.println();
  }
  
  return true;
}

void DiskManager::saveConfig() {
  // Remove existing file
  if (sd->exists(LASTIMG_FILE)) {
    sd->remove(LASTIMG_FILE);
    delay(50);
  }
  
  File32 configFile = sd->open(LASTIMG_FILE, O_WRITE);
  if (!configFile) {
    Serial.println("Warning: Could not create config file");
    return;
  }
  
  // Write format: drive0_filename,drive1_filename
  if (loadedImageIndex[0] >= 0 && loadedImageIndex[0] < totalImages) {
    configFile.print(diskImages[loadedImageIndex[0]]);
  } else {
    configFile.print("NONE");
  }
  
  configFile.print(",");
  
  if (loadedImageIndex[1] >= 0 && loadedImageIndex[1] < totalImages) {
    configFile.println(diskImages[loadedImageIndex[1]]);
  } else {
    configFile.println("NONE");
  }
  
  configFile.flush();
  delay(20);
  configFile.close();
  delay(20);
  
  Serial.print("Saved config: Drive 0=");
  Serial.print(loadedImageIndex[0] >= 0 ? diskImages[loadedImageIndex[0]] : "NONE");
  Serial.print(", Drive 1=");
  Serial.println(loadedImageIndex[1] >= 0 ? diskImages[loadedImageIndex[1]] : "NONE");
}

void DiskManager::loadConfig() {
  File32 configFile = sd->open(LASTIMG_FILE, O_READ);
  if (!configFile) {
    Serial.println("No config file found, using defaults");
    return;
  }
  
  // Read config line
  char line[140];
  memset(line, 0, sizeof(line));
  int i = 0;
  while (configFile.available() && i < 139) {
    char c = configFile.read();
    if (c == '\n' || c == '\r') break;
    line[i++] = c;
  }
  configFile.close();
  
  // Parse filenames
  char* commaPtr = strchr(line, ',');
  if (commaPtr) {
    *commaPtr = '\0';
    char* filename0 = line;
    char* filename1 = commaPtr + 1;
    
    Serial.print("Loaded config: Drive 0=");
    Serial.print(filename0);
    Serial.print(", Drive 1=");
    Serial.println(filename1);
    
    // Find Drive 0
    if (strcmp(filename0, "NONE") != 0) {
      for (int idx = 0; idx < totalImages; idx++) {
        if (strcmp(diskImages[idx], filename0) == 0) {
          loadImage(0, idx);
          Serial.print("  Drive 0 found at index ");
          Serial.println(idx);
          break;
        }
      }
    }
    
    // Find Drive 1
    if (strcmp(filename1, "NONE") != 0) {
      for (int idx = 0; idx < totalImages; idx++) {
        if (strcmp(diskImages[idx], filename1) == 0) {
          loadImage(1, idx);
          Serial.print("  Drive 1 found at index ");
          Serial.println(idx);
          break;
        }
      }
    }
  }
}

DiskImage* DiskManager::getDisk(uint8_t drive) {
  if (drive >= MAX_DRIVES) return nullptr;
  return &disks[drive];
}

int DiskManager::getLoadedIndex(uint8_t drive) const {
  if (drive >= MAX_DRIVES) return -1;
  return loadedImageIndex[drive];
}
