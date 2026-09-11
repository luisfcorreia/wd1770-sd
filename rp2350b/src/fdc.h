/*
 * wd1770-sd RP2350B - WD1770 FDC Emulation
 */

#ifndef FDC_H
#define FDC_H

#include <stdint.h>
#include <stdbool.h>
#include "disk_image.h"

/* Command codes */
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

/* Status bits */
#define ST_BUSY             0x01
#define ST_DRQ              0x02
#define ST_LOST_DATA        0x04
#define ST_CRC_ERROR        0x08
#define ST_RNF              0x10
#define ST_RECORD_TYPE      0x20
#define ST_WRITE_PROTECT    0x40
#define ST_NOT_READY        0x80

/* Type I specific status */
#define ST_TRACK00          0x04

/* Disk geometry */
#define MAX_TRACKS          84
#define MAX_SECTORS         18
#define SECTOR_SIZE_SD      256
#define SECTOR_SIZE_DD      512

/* Timing constants (microseconds) */
#define STEP_TIME_6MS       6000
#define STEP_TIME_12MS      12000
#define STEP_TIME_20MS      20000
#define STEP_TIME_30MS      30000
#define HEAD_SETTLE_TIME    15000
#define SECTOR_READ_TIME    3000
#define SECTOR_WRITE_TIME   3000

/* FDC State Machine */
typedef enum {
    STATE_IDLE,
    STATE_SEEKING,
    STATE_SETTLING,
    STATE_READING_SECTOR,
    STATE_SECTOR_READ_COMPLETE,
    STATE_WRITING_SECTOR,
    STATE_SECTOR_WRITE_COMPLETE,
    STATE_WAITING_FOR_DATA_IN,
    STATE_WAITING_FOR_DATA_OUT
} fdc_state_t;

/* FDC Registers and State */
typedef struct {
    uint8_t status;
    uint8_t track;
    uint8_t sector;
    uint8_t data;
    uint8_t command;
    uint8_t current_track;
    int8_t direction;
    bool busy;
    bool drq;
    bool intrq;
    bool double_density;
    uint16_t data_index;
    uint16_t data_length;
    uint8_t sector_buffer[1024];
    uint64_t operation_start_time;
    uint32_t step_rate;
    bool write_protect;
    bool motor_on;
    fdc_state_t state;
    uint8_t sectors_remaining;
    bool multi_sector;
} fdc_state_internal_t;

/* Forward declaration */
typedef struct disk_manager_s disk_manager_t;

/* FDC Device */
typedef struct {
    fdc_state_internal_t fdc;
    disk_manager_t *disk_mgr;
    uint8_t active_drive;
    bool last_cs;
    bool last_rw;
    bool data_bus_driven;
    uint64_t data_valid_until;
} fdc_device_t;

/* Initialize FDC */
void fdc_init(fdc_device_t *dev);

/* Link subsystems */
void fdc_set_disk_manager(fdc_device_t *dev, disk_manager_t *dm);

/* FDC enable/disable */
bool fdc_is_enabled(fdc_device_t *dev);
void fdc_disable(fdc_device_t *dev);

/* Drive selection */
void fdc_check_drive_select(fdc_device_t *dev);
uint8_t fdc_get_active_drive(fdc_device_t *dev);

/* Bus interface */
void fdc_handle_bus(fdc_device_t *dev);

/* State machine */
void fdc_process_state_machine(fdc_device_t *dev);

/* Output signals */
void fdc_update_outputs(fdc_device_t *dev);

/* State access */
bool fdc_is_busy(fdc_device_t *dev);
uint8_t fdc_get_current_track(fdc_device_t *dev);
fdc_state_t fdc_get_state(fdc_device_t *dev);

#endif /* FDC_H */
