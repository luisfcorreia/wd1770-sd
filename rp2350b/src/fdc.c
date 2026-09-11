/*
 * wd1770-sd RP2350B - WD1770 FDC Emulation Implementation
 */

#include "fdc.h"
#include "disk_manager.h"
#include "bus_io.h"
#include "hardware_config.h"
#include "hardware/timer.h"

#include <string.h>
#include <stdio.h>

/* Forward declarations of internal command handlers */
static void fdc_handle_read(fdc_device_t *dev, uint8_t addr);
static void fdc_handle_write(fdc_device_t *dev, uint8_t addr);
static void cmd_restore(fdc_device_t *dev);
static void cmd_seek(fdc_device_t *dev);
static void cmd_step(fdc_device_t *dev);
static void cmd_step_in(fdc_device_t *dev);
static void cmd_step_out(fdc_device_t *dev);
static void cmd_read_sector(fdc_device_t *dev);
static void cmd_write_sector(fdc_device_t *dev);
static void cmd_read_address(fdc_device_t *dev);
static void cmd_force_interrupt(fdc_device_t *dev);
static void read_sector_data(fdc_device_t *dev);
static void write_sector_data(fdc_device_t *dev);
static uint32_t get_step_rate(fdc_device_t *dev);

/* Bus access wrappers */
static void drive_data_bus_dev(fdc_device_t *dev, uint8_t data);
static void release_data_bus_dev(fdc_device_t *dev);
static uint8_t read_data_bus(void);

/* Initialize FDC */
void fdc_init(fdc_device_t *dev) {
    memset(dev, 0, sizeof(fdc_device_t));
    dev->fdc.status = ST_TRACK00;
    dev->fdc.track = 0;
    dev->fdc.sector = 1;
    dev->fdc.current_track = 0;
    dev->fdc.direction = 1;
    dev->fdc.step_rate = STEP_TIME_6MS;
    dev->fdc.state = STATE_IDLE;
    dev->last_cs = true;
    dev->last_rw = true;
}

void fdc_set_disk_manager(fdc_device_t *dev, disk_manager_t *dm) {
    dev->disk_mgr = dm;
}

bool fdc_is_enabled(fdc_device_t *dev) {
    return bus_dden_active();
}

void fdc_disable(fdc_device_t *dev) {
    if (dev->data_bus_driven) {
        release_data_bus_dev(dev);
    }
}

void fdc_check_drive_select(fdc_device_t *dev) {
    uint8_t ds = bus_read_drive_select();
    if (ds == 0) dev->active_drive = 0;
    else if (ds == 1) dev->active_drive = 1;
}

uint8_t fdc_get_active_drive(fdc_device_t *dev) {
    return dev->active_drive;
}

bool fdc_is_busy(fdc_device_t *dev) {
    return dev->fdc.busy;
}

uint8_t fdc_get_current_track(fdc_device_t *dev) {
    return dev->fdc.current_track;
}

fdc_state_t fdc_get_state(fdc_device_t *dev) {
    return dev->fdc.state;
}

/* Bus access wrappers */
static uint8_t read_data_bus(void) {
    return bus_read_data();
}

static void drive_data_bus_dev(fdc_device_t *dev, uint8_t data) {
    bus_write_data(data);
    dev->data_bus_driven = true;
    dev->data_valid_until = time_us_64() + 500;
}

static void release_data_bus_dev(fdc_device_t *dev) {
    bus_release();
    dev->data_bus_driven = false;
}

/* Bus transaction handler */
void fdc_handle_bus(fdc_device_t *dev) {
    bool cs = bus_cs_active();
    bool rw = bus_rw_read();

    /* CS active edge - start of transaction (CS goes LOW = active) */
    if (!dev->last_cs && cs) {
        uint8_t addr = bus_read_address();

        if (rw) {
            /* Read: CPU reading from FDC */
            fdc_handle_read(dev, addr);
        } else {
            /* Write: CPU writing to FDC */
            uint8_t data = read_data_bus();
            dev->fdc.data = data;
            fdc_handle_write(dev, addr);
        }
    }

    /* CS rising edge - end of transaction */
    if (dev->last_cs && !cs) {
        if (dev->data_bus_driven && time_us_64() > dev->data_valid_until) {
            release_data_bus_dev(dev);
        }
    }

    dev->last_cs = cs;
    dev->last_rw = rw;
}

static void fdc_handle_read(fdc_device_t *dev, uint8_t addr) {
    uint8_t value = 0;

    switch (addr) {
        case 0:  /* Status register */
            value = dev->fdc.status;
            if (dev->fdc.busy) value |= ST_BUSY;
            if (dev->fdc.drq) value |= ST_DRQ;
            dev->fdc.intrq = false;
            break;
        case 1:  /* Track register */
            value = dev->fdc.track;
            break;
        case 2:  /* Sector register */
            value = dev->fdc.sector;
            break;
        case 3:  /* Data register */
            value = dev->fdc.data;
            if (dev->fdc.state == STATE_READING_SECTOR && dev->fdc.data_index < dev->fdc.data_length) {
                value = dev->fdc.sector_buffer[dev->fdc.data_index++];
                dev->fdc.data = value;
                if (dev->fdc.data_index >= dev->fdc.data_length) {
                    dev->fdc.drq = false;
                    dev->fdc.state = STATE_SECTOR_READ_COMPLETE;
                }
            }
            break;
    }

    drive_data_bus_dev(dev, value);
}

static void fdc_handle_write(fdc_device_t *dev, uint8_t addr) {
    switch (addr) {
        case 0:  /* Command register */
            dev->fdc.command = dev->fdc.data;
            if ((dev->fdc.command & 0xF0) == CMD_RESTORE) {
                cmd_restore(dev);
            } else if ((dev->fdc.command & 0xF0) == CMD_SEEK) {
                cmd_seek(dev);
            } else if ((dev->fdc.command & 0xE0) == CMD_STEP) {
                cmd_step(dev);
            } else if ((dev->fdc.command & 0xE0) == CMD_STEP_IN) {
                cmd_step_in(dev);
            } else if ((dev->fdc.command & 0xE0) == CMD_STEP_OUT) {
                cmd_step_out(dev);
            } else if ((dev->fdc.command & 0xF0) == CMD_READ_SECTOR ||
                       (dev->fdc.command & 0xF0) == CMD_READ_SECTORS) {
                cmd_read_sector(dev);
            } else if ((dev->fdc.command & 0xF0) == CMD_WRITE_SECTOR ||
                       (dev->fdc.command & 0xF0) == CMD_WRITE_SECTORS) {
                cmd_write_sector(dev);
            } else if ((dev->fdc.command & 0xF0) == CMD_READ_ADDRESS) {
                cmd_read_address(dev);
            } else if ((dev->fdc.command & 0xF0) == CMD_FORCE_INT) {
                cmd_force_interrupt(dev);
            }
            break;
        case 1:  /* Track register */
            dev->fdc.track = dev->fdc.data;
            break;
        case 2:  /* Sector register */
            dev->fdc.sector = dev->fdc.data;
            break;
        case 3:  /* Data register */
            if (dev->fdc.state == STATE_WAITING_FOR_DATA_IN && dev->fdc.data_index < dev->fdc.data_length) {
                dev->fdc.sector_buffer[dev->fdc.data_index++] = dev->fdc.data;
                if (dev->fdc.data_index >= dev->fdc.data_length) {
                    dev->fdc.drq = false;
                    dev->fdc.state = STATE_WRITING_SECTOR;
                    write_sector_data(dev);
                }
            }
            break;
    }
}

static uint32_t get_step_rate(fdc_device_t *dev) {
    uint8_t rate_code = dev->fdc.command & 0x03;
    switch (rate_code) {
        case 0: return STEP_TIME_6MS;
        case 1: return STEP_TIME_12MS;
        case 2: return STEP_TIME_20MS;
        case 3: return STEP_TIME_30MS;
    }
    return STEP_TIME_6MS;
}

/* Command handlers */
static void cmd_restore(fdc_device_t *dev) {
    dev->fdc.busy = true;
    dev->fdc.status = ST_BUSY;
    dev->fdc.current_track = 0;
    dev->fdc.track = 0;
    dev->fdc.direction = -1;
    dev->fdc.state = STATE_SEEKING;
    dev->fdc.step_rate = get_step_rate(dev);
    dev->fdc.operation_start_time = time_us_64();
}

static void cmd_seek(fdc_device_t *dev) {
    dev->fdc.busy = true;
    dev->fdc.status = ST_BUSY;
    dev->fdc.direction = (dev->fdc.data > dev->fdc.current_track) ? 1 : -1;
    dev->fdc.state = STATE_SEEKING;
    dev->fdc.step_rate = get_step_rate(dev);
    dev->fdc.operation_start_time = time_us_64();
}

static void cmd_step(fdc_device_t *dev) {
    dev->fdc.busy = true;
    dev->fdc.status = ST_BUSY;
    dev->fdc.state = STATE_SEEKING;
    dev->fdc.step_rate = get_step_rate(dev);
    dev->fdc.operation_start_time = time_us_64();
}

static void cmd_step_in(fdc_device_t *dev) {
    dev->fdc.busy = true;
    dev->fdc.status = ST_BUSY;
    dev->fdc.direction = 1;
    dev->fdc.state = STATE_SEEKING;
    dev->fdc.step_rate = get_step_rate(dev);
    dev->fdc.operation_start_time = time_us_64();
}

static void cmd_step_out(fdc_device_t *dev) {
    dev->fdc.busy = true;
    dev->fdc.status = ST_BUSY;
    dev->fdc.direction = -1;
    dev->fdc.state = STATE_SEEKING;
    dev->fdc.step_rate = get_step_rate(dev);
    dev->fdc.operation_start_time = time_us_64();
}

static void cmd_read_sector(fdc_device_t *dev) {
    if (!dev->disk_mgr) {
        dev->fdc.status = ST_RNF;
        dev->fdc.intrq = true;
        return;
    }

    disk_image_t *current_disk = disk_mgr_get_disk(dev->disk_mgr, dev->active_drive);
    if (!current_disk || current_disk->size == 0) {
        dev->fdc.status = ST_RNF;
        dev->fdc.intrq = true;
        return;
    }

    dev->fdc.busy = true;
    dev->fdc.status = ST_BUSY;
    dev->fdc.multi_sector = ((dev->fdc.command & 0xF0) == CMD_READ_SECTORS);
    dev->fdc.sectors_remaining = dev->fdc.multi_sector ? current_disk->sectors_per_track : 1;
    dev->fdc.state = STATE_READING_SECTOR;
    dev->fdc.operation_start_time = time_us_64();

    read_sector_data(dev);
}

static void cmd_write_sector(fdc_device_t *dev) {
    if (!dev->disk_mgr) {
        dev->fdc.status = ST_RNF;
        dev->fdc.intrq = true;
        return;
    }

    disk_image_t *current_disk = disk_mgr_get_disk(dev->disk_mgr, dev->active_drive);
    if (!current_disk || current_disk->size == 0) {
        dev->fdc.status = ST_RNF;
        dev->fdc.intrq = true;
        return;
    }

    if (current_disk->write_protected) {
        dev->fdc.status = ST_WRITE_PROTECT;
        dev->fdc.intrq = true;
        return;
    }

    dev->fdc.busy = true;
    dev->fdc.status = ST_BUSY;
    dev->fdc.multi_sector = ((dev->fdc.command & 0xF0) == CMD_WRITE_SECTORS);
    dev->fdc.sectors_remaining = dev->fdc.multi_sector ? current_disk->sectors_per_track : 1;
    dev->fdc.data_index = 0;
    dev->fdc.data_length = current_disk->sector_size;
    dev->fdc.drq = true;
    dev->fdc.state = STATE_WAITING_FOR_DATA_IN;
    dev->fdc.operation_start_time = time_us_64();
}

static void cmd_read_address(fdc_device_t *dev) {
    dev->fdc.sector_buffer[0] = dev->fdc.current_track;
    dev->fdc.sector_buffer[1] = 0;
    dev->fdc.sector_buffer[2] = 1;
    dev->fdc.sector_buffer[3] = 2;
    dev->fdc.sector_buffer[4] = 0;
    dev->fdc.sector_buffer[5] = 0;

    dev->fdc.data_index = 0;
    dev->fdc.data_length = 6;
    dev->fdc.drq = true;
    dev->fdc.busy = true;
    dev->fdc.status = ST_BUSY;
    dev->fdc.state = STATE_READING_SECTOR;
}

static void cmd_force_interrupt(fdc_device_t *dev) {
    dev->fdc.busy = false;
    dev->fdc.drq = false;
    dev->fdc.intrq = true;
    dev->fdc.state = STATE_IDLE;
    dev->fdc.status = 0;
}

/* Sector I/O */
static void read_sector_data(fdc_device_t *dev) {
    if (!dev->disk_mgr) return;

    disk_image_t *current_disk = disk_mgr_get_disk(dev->disk_mgr, dev->active_drive);
    if (!current_disk || current_disk->size == 0) {
        dev->fdc.status = ST_RNF;
        dev->fdc.busy = false;
        dev->fdc.intrq = true;
        dev->fdc.state = STATE_IDLE;
        return;
    }

    /* Validate sector */
    if (dev->fdc.sector < 1 || dev->fdc.sector > current_disk->sectors_per_track) {
        dev->fdc.status = ST_RNF;
        dev->fdc.busy = false;
        dev->fdc.intrq = true;
        dev->fdc.state = STATE_IDLE;
        return;
    }

    /* Calculate offset */
    uint32_t offset;
    if (current_disk->is_extended_dsk) {
        uint32_t track_size = current_disk->track_header_size +
                              (current_disk->sectors_per_track * current_disk->sector_size);
        offset = current_disk->header_offset +
                 (dev->fdc.current_track * track_size) +
                 current_disk->track_header_size +
                 ((dev->fdc.sector - 1) * current_disk->sector_size);
    } else {
        offset = (dev->fdc.current_track * current_disk->sectors_per_track + (dev->fdc.sector - 1)) *
                 current_disk->sector_size;
    }

    /* Read sector via FatFs */
    if (!disk_mgr_read_sector(dev->disk_mgr, current_disk, offset, dev->fdc.sector_buffer, current_disk->sector_size)) {
        dev->fdc.status = ST_RNF;
        dev->fdc.busy = false;
        dev->fdc.intrq = true;
        dev->fdc.state = STATE_IDLE;
        return;
    }

    dev->fdc.data_index = 0;
    dev->fdc.data_length = current_disk->sector_size;
    dev->fdc.drq = true;
    dev->fdc.status = ST_BUSY | ST_DRQ;
    dev->fdc.state = STATE_READING_SECTOR;
}

static void write_sector_data(fdc_device_t *dev) {
    if (!dev->disk_mgr) return;

    disk_image_t *current_disk = disk_mgr_get_disk(dev->disk_mgr, dev->active_drive);
    if (!current_disk || current_disk->size == 0) {
        dev->fdc.status = ST_RNF;
        dev->fdc.busy = false;
        dev->fdc.intrq = true;
        dev->fdc.state = STATE_IDLE;
        return;
    }

    /* Calculate offset */
    uint32_t offset;
    if (current_disk->is_extended_dsk) {
        uint32_t track_size = current_disk->track_header_size +
                              (current_disk->sectors_per_track * current_disk->sector_size);
        offset = current_disk->header_offset +
                 (dev->fdc.current_track * track_size) +
                 current_disk->track_header_size +
                 ((dev->fdc.sector - 1) * current_disk->sector_size);
    } else {
        offset = (dev->fdc.current_track * current_disk->sectors_per_track + (dev->fdc.sector - 1)) *
                 current_disk->sector_size;
    }

    /* Write sector via FatFs */
    if (!disk_mgr_write_sector(dev->disk_mgr, current_disk, offset, dev->fdc.sector_buffer, current_disk->sector_size)) {
        dev->fdc.status = ST_WRITE_PROTECT;
        dev->fdc.busy = false;
        dev->fdc.intrq = true;
        dev->fdc.state = STATE_IDLE;
        return;
    }

    dev->fdc.state = STATE_SECTOR_WRITE_COMPLETE;
}

/* State machine */
void fdc_process_state_machine(fdc_device_t *dev) {
    uint64_t now = time_us_64();

    switch (dev->fdc.state) {
        case STATE_IDLE:
            break;

        case STATE_SEEKING:
            if (now - dev->fdc.operation_start_time >= dev->fdc.step_rate) {
                if ((dev->fdc.command & 0xF0) == CMD_RESTORE) {
                    dev->fdc.current_track = 0;
                    dev->fdc.track = 0;
                    dev->fdc.status = ST_TRACK00;
                    dev->fdc.busy = false;
                    dev->fdc.intrq = true;
                    dev->fdc.state = STATE_IDLE;
                } else if ((dev->fdc.command & 0xF0) == CMD_SEEK) {
                    dev->fdc.current_track = dev->fdc.data;
                    if (dev->fdc.command & 0x10) {
                        dev->fdc.track = dev->fdc.current_track;
                    }
                    dev->fdc.status = (dev->fdc.current_track == 0) ? ST_TRACK00 : 0;
                    dev->fdc.busy = false;
                    dev->fdc.intrq = true;
                    dev->fdc.state = STATE_IDLE;
                } else {
                    /* STEP, STEP_IN, STEP_OUT */
                    dev->fdc.current_track += dev->fdc.direction;
                    if (dev->fdc.current_track < 0) dev->fdc.current_track = 0;
                    if (dev->fdc.current_track > MAX_TRACKS) dev->fdc.current_track = MAX_TRACKS;
                    if (dev->fdc.command & 0x10) {
                        dev->fdc.track = dev->fdc.current_track;
                    }
                    dev->fdc.status = (dev->fdc.current_track == 0) ? ST_TRACK00 : 0;
                    dev->fdc.busy = false;
                    dev->fdc.intrq = true;
                    dev->fdc.state = STATE_IDLE;
                }
            }
            break;

        case STATE_READING_SECTOR:
            /* Wait for CPU to read all data via DRQ */
            break;

        case STATE_SECTOR_READ_COMPLETE:
            if (dev->fdc.multi_sector && dev->fdc.sectors_remaining > 1) {
                dev->fdc.sectors_remaining--;
                dev->fdc.sector++;
                read_sector_data(dev);
            } else {
                dev->fdc.busy = false;
                dev->fdc.drq = false;
                dev->fdc.intrq = true;
                dev->fdc.status = 0;
                dev->fdc.state = STATE_IDLE;
            }
            break;

        case STATE_WAITING_FOR_DATA_IN:
            /* Wait for CPU to write all data via DRQ */
            break;

        case STATE_WRITING_SECTOR:
            /* Writing handled in write_sector_data() */
            break;

        case STATE_SECTOR_WRITE_COMPLETE:
            if (dev->fdc.multi_sector && dev->fdc.sectors_remaining > 1) {
                dev->fdc.sectors_remaining--;
                dev->fdc.sector++;
                dev->fdc.data_index = 0;
                dev->fdc.drq = true;
                dev->fdc.state = STATE_WAITING_FOR_DATA_IN;
            } else {
                dev->fdc.busy = false;
                dev->fdc.drq = false;
                dev->fdc.intrq = true;
                dev->fdc.status = 0;
                dev->fdc.state = STATE_IDLE;
            }
            break;

        default:
            break;
    }
}

void fdc_update_outputs(fdc_device_t *dev) {
    bus_set_intrq(dev->fdc.intrq);
    bus_set_drq(dev->fdc.drq);
}
