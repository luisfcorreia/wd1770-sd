/*
 * wd1770-sd RP2350B - Disk Manager
 */

#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "disk_image.h"

#define MAX_DISK_IMAGES 100
#define MAX_DRIVES 2
#define LASTIMG_FILE "/lastimg.cfg"

typedef struct disk_manager_s {
    char disk_images[MAX_DISK_IMAGES][64];
    int total_images;
    int loaded_image_index[MAX_DRIVES];
    disk_image_t disks[MAX_DRIVES];
    bool sd_mounted;
} disk_manager_t;

/* Initialize disk manager */
void disk_mgr_init(disk_manager_t *mgr);

/* Scan SD card for disk images */
void disk_mgr_scan(disk_manager_t *mgr);

/* Get total number of images found */
int disk_mgr_get_total(disk_manager_t *mgr);

/* Get image name at index */
const char *disk_mgr_get_image_name(disk_manager_t *mgr, int index);

/* Load disk image metadata for a drive */
bool disk_mgr_load_image(disk_manager_t *mgr, uint8_t drive, int image_index);

/* Eject a drive */
void disk_mgr_eject(disk_manager_t *mgr, uint8_t drive);

/* Save/Load configuration */
void disk_mgr_save_config(disk_manager_t *mgr);
void disk_mgr_load_config(disk_manager_t *mgr);

/* Get disk image struct for a drive */
disk_image_t *disk_mgr_get_disk(disk_manager_t *mgr, uint8_t drive);

/* Get loaded image index for a drive */
int disk_mgr_get_loaded_index(disk_manager_t *mgr, uint8_t drive);

/* Sector I/O via FatFs */
bool disk_mgr_read_sector(disk_manager_t *mgr, disk_image_t *disk,
                          uint32_t offset, uint8_t *buf, uint16_t len);
bool disk_mgr_write_sector(disk_manager_t *mgr, disk_image_t *disk,
                           uint32_t offset, const uint8_t *buf, uint16_t len);

#endif /* DISK_MANAGER_H */
