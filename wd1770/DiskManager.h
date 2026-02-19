#pragma once

#include <SdFat.h>
#include "DiskImage.h"

#define MAX_DISK_IMAGES 100
#define MAX_DRIVES 2
#define LASTIMG_FILE "/lastimg.cfg"

class DiskManager {
public:
  DiskManager();
  
  // Initialization
  bool begin(SdFat32* sdCard);
  
  // Image scanning
  void scanImages();
  int getTotalImages() const { return totalImages; }
  const char* getImageName(int index) const;
  
  // Image loading/ejecting
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
  int loadedImageIndex[MAX_DRIVES];
  
  // Loaded disk data
  DiskImage disks[MAX_DRIVES];
  
  // Format detection
  bool detectFormat(DiskImage* disk, uint32_t fileSize);
  bool parseExtendedDSK(uint8_t drive, const char* filename);
};
