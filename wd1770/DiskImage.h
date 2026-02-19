#pragma once
#include <Arduino.h>

// Common disk format sizes (for auto-detection)
#define SIZE_TIMEX_FDD3000_SS   163840   // 160KB: 40T/16S/256B
#define SIZE_TIMEX_FDD3000_DS   327680   // 320KB: 80T/16S/256B
#define SIZE_CPC_40T            184320   // 180KB: 40T/9S/512B
#define SIZE_35_DD              737280   // 720KB: 80T/9S/512B
#define SIZE_525_DD             368640   // 360KB: 40T/9S/512B

// Disk image metadata structure
typedef struct {
  char filename[64];        // Image filename
  uint32_t size;            // Total file size in bytes
  uint8_t tracks;           // Number of tracks
  uint8_t sectorsPerTrack;  // Sectors per track
  uint16_t sectorSize;      // Bytes per sector
  bool doubleDensity;       // Single vs double density
  bool writeProtected;      // Write protection flag
  bool isExtendedDSK;       // True if Extended DSK format with headers
  uint16_t headerOffset;    // Offset to skip headers (256 bytes typically)
  uint16_t trackHeaderSize; // Track Information Block size (256 bytes)
} DiskImage;
