/*
 * wd1770-sd RP2350B - Disk Image Data Structures
 */

#ifndef DISK_IMAGE_H
#define DISK_IMAGE_H

#include <stdint.h>
#include <stdbool.h>

/* Common disk format sizes (for auto-detection) */
#define SIZE_TIMEX_FDD3000_SS   163840   /* 160KB: 40T/16S/256B */
#define SIZE_TIMEX_FDD3000_DS   327680   /* 320KB: 80T/16S/256B */
#define SIZE_CPC_40T            184320   /* 180KB: 40T/9S/512B */
#define SIZE_35_DD              737280   /* 720KB: 80T/9S/512B */
#define SIZE_525_DD             368640   /* 360KB: 40T/9S/512B */

typedef struct {
    char filename[64];
    uint32_t size;
    uint8_t tracks;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    bool double_density;
    bool write_protected;
    bool is_extended_dsk;
    uint32_t header_offset;
    uint32_t track_header_size;
} disk_image_t;

#endif /* DISK_IMAGE_H */
