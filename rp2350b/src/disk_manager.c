/*
 * wd1770-sd RP2350B - Disk Manager Implementation
 */

#include "disk_manager.h"
#include "hardware_config.h"

#include "ff.h"
#include "sd_card.h"
#include "f_util.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

static FATFS fs;
static bool fs_mounted = false;

/* Mount filesystem */
static bool ensure_mounted(void) {
    if (fs_mounted) return true;
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr == FR_OK) {
        fs_mounted = true;
        return true;
    }
    DBGLN("FatFs mount failed: %d", fr);
    return false;
}

void disk_mgr_init(disk_manager_t *mgr) {
    memset(mgr, 0, sizeof(disk_manager_t));
    mgr->loaded_image_index[0] = -1;
    mgr->loaded_image_index[1] = -1;
    mgr->sd_mounted = false;

    for (int i = 0; i < MAX_DRIVES; i++) {
        mgr->disks[i].filename[0] = '\0';
    }
}

void disk_mgr_scan(disk_manager_t *mgr) {
    if (!ensure_mounted()) return;

    DIR dir;
    FILINFO fno;
    FRESULT fr = f_opendir(&dir, "/");
    if (fr != FR_OK) {
        DBGLN("Failed to open root directory");
        return;
    }

    mgr->total_images = 0;
    while (1) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;
        if (fno.fattrib & AM_DIR) continue;

        /* Check extension */
        char upper[64];
        strncpy(upper, fno.fname, 63);
        upper[63] = '\0';
        for (int j = 0; upper[j]; j++) upper[j] = toupper(upper[j]);

        if (strstr(upper, ".DSK") || strstr(upper, ".IMG") ||
            strstr(upper, ".ST")  || strstr(upper, ".HFE")) {
            if (mgr->total_images < MAX_DISK_IMAGES) {
                strncpy(mgr->disk_images[mgr->total_images], fno.fname, 63);
                mgr->disk_images[mgr->total_images][63] = '\0';
                DBG("Found: ");
                DBGLN("%s", mgr->disk_images[mgr->total_images]);
                mgr->total_images++;
            }
        }
    }
    f_closedir(&dir);

    DBG("Found %d disk images\n", mgr->total_images);
}

int disk_mgr_get_total(disk_manager_t *mgr) {
    return mgr->total_images;
}

const char *disk_mgr_get_image_name(disk_manager_t *mgr, int index) {
    if (index >= 0 && index < mgr->total_images) {
        return mgr->disk_images[index];
    }
    return NULL;
}

/* Format detection by file size */
static bool detect_format(disk_image_t *disk, uint32_t file_size) {
    if (file_size == SIZE_TIMEX_FDD3000_SS) {
        disk->tracks = 40; disk->sectors_per_track = 16;
        disk->sector_size = 256; disk->double_density = false;
        DBGLN("Format: Timex FDD 3000 (40T/16S/256B)");
        return true;
    }
    if (file_size == SIZE_TIMEX_FDD3000_DS) {
        disk->tracks = 80; disk->sectors_per_track = 16;
        disk->sector_size = 256; disk->double_density = false;
        DBGLN("Format: Timex FDD 3000 DS (80T/16S/256B)");
        return true;
    }
    if (file_size == SIZE_35_DD) {
        disk->tracks = 80; disk->sectors_per_track = 9;
        disk->sector_size = 512; disk->double_density = true;
        DBGLN("Format: 3.5\" DD (80T/9S/512B)");
        return true;
    }
    if (file_size == SIZE_525_DD) {
        disk->tracks = 40; disk->sectors_per_track = 9;
        disk->sector_size = 512; disk->double_density = true;
        DBGLN("Format: 5.25\" DD (40T/9S/512B)");
        return true;
    }
    if (file_size == SIZE_CPC_40T) {
        disk->tracks = 40; disk->sectors_per_track = 9;
        disk->sector_size = 512; disk->double_density = true;
        DBGLN("Format: Amstrad/Spectrum raw (40T/9S/512B)");
        return true;
    }
    if (file_size == 174336) {
        disk->tracks = 40; disk->sectors_per_track = 9;
        disk->sector_size = 512; disk->double_density = true;
        DBGLN("Format: Extended DSK (Amstrad/Spectrum)");
        return true;
    }

    /* Guess: try 256-byte sectors first */
    uint32_t sectors256 = file_size / 256;
    if (sectors256 == 640) {
        disk->tracks = 40; disk->sectors_per_track = 16;
        disk->sector_size = 256; disk->double_density = false;
        DBGLN("Guessing: Timex format (40T/16S/256B)");
        return true;
    }
    if (sectors256 == 1280) {
        disk->tracks = 80; disk->sectors_per_track = 16;
        disk->sector_size = 256; disk->double_density = false;
        DBGLN("Guessing: Timex DS format (80T/16S/256B)");
        return true;
    }

    /* Fall back to 512-byte sectors */
    uint32_t sectors512 = file_size / 512;
    if (sectors512 < 720) {
        disk->tracks = 40;
        disk->sectors_per_track = sectors512 / 40;
    } else {
        disk->tracks = 80;
        disk->sectors_per_track = sectors512 / 80;
    }
    disk->sector_size = 512;
    disk->double_density = true;
    DBGLN("Guessing: %dT/%dS/512B", disk->tracks, disk->sectors_per_track);
    return false;
}

/* Parse Extended DSK header */
static bool parse_extended_dsk(disk_image_t *disk, const char *filename) {
    FIL fp;
    FRESULT fr = f_open(&fp, filename, FA_READ);
    if (fr != FR_OK) return false;

    uint8_t header[256];
    UINT br;
    fr = f_read(&fp, header, 256, &br);
    if (fr != FR_OK || br != 256) {
        f_close(&fp);
        return false;
    }

    /* Check signature */
    if (strncmp((char *)header, "EXTENDED CPC DSK", 16) != 0 &&
        strncmp((char *)header, "MV - CPCEMU Disk", 16) != 0) {
        f_close(&fp);
        return false;
    }

    disk->tracks = header[0x30];

    /* Read first Track Information Block */
    uint8_t track_header[256];
    fr = f_read(&fp, track_header, 256, &br);
    f_close(&fp);
    if (fr != FR_OK || br != 256) return false;

    if (strncmp((char *)track_header, "Track-Info", 10) != 0) {
        DBGLN("Warning: Invalid Track-Info signature");
        return false;
    }

    disk->sectors_per_track = track_header[0x15];
    uint8_t sector_size_code = track_header[0x14];
    disk->sector_size = 128 << sector_size_code;
    disk->is_extended_dsk = true;
    disk->header_offset = 256;
    disk->track_header_size = 256;
    disk->double_density = (disk->sector_size >= 512);

    DBGLN("Extended DSK: %dT/%dS/%dB", disk->tracks, disk->sectors_per_track, disk->sector_size);
    return true;
}

bool disk_mgr_load_image(disk_manager_t *mgr, uint8_t drive, int image_index) {
    if (drive >= MAX_DRIVES || image_index >= mgr->total_images || image_index < 0)
        return false;

    if (!ensure_mounted()) return false;

    char filename[70];
    snprintf(filename, sizeof(filename), "/%s", mgr->disk_images[image_index]);

    /* Get file size */
    FIL fp;
    FRESULT fr = f_open(&fp, filename, FA_READ);
    if (fr != FR_OK) {
        DBG("Failed to open: %s\n", filename);
        return false;
    }

    disk_image_t *disk = &mgr->disks[drive];
    strncpy(disk->filename, mgr->disk_images[image_index], 63);
    disk->filename[63] = '\0';
    disk->size = f_size(&fp);
    f_close(&fp);

    /* Detect format */
    if (!detect_format(disk, disk->size)) {
        DBGLN("Warning: Unknown disk format");
    }

    disk->write_protected = false;
    disk->is_extended_dsk = false;
    disk->header_offset = 0;
    disk->track_header_size = 0;
    mgr->loaded_image_index[drive] = image_index;

    /* Check for Extended DSK header */
    char ext_check[70];
    strncpy(ext_check, filename, 69);
    ext_check[69] = '\0';
    for (int i = 0; ext_check[i]; i++) ext_check[i] = toupper(ext_check[i]);

    if (strstr(ext_check, ".DSK") || strstr(ext_check, ".HFE")) {
        if (parse_extended_dsk(disk, filename)) {
            DBGLN("Extended DSK header parsed successfully");
        }
    }

    DBG("Drive %d: Loaded %s (%u bytes, %dT/%dS/%dB)\n",
        drive, disk->filename, disk->size, disk->tracks,
        disk->sectors_per_track, disk->sector_size);

    return true;
}

void disk_mgr_eject(disk_manager_t *mgr, uint8_t drive) {
    if (drive >= MAX_DRIVES) return;
    mgr->disks[drive].filename[0] = '\0';
    mgr->disks[drive].size = 0;
    mgr->loaded_image_index[drive] = -1;
}

void disk_mgr_save_config(disk_manager_t *mgr) {
    if (!ensure_mounted()) return;

    /* Remove existing file */
    f_unlink(LASTIMG_FILE);
    sleep_ms(50);

    FIL fp;
    FRESULT fr = f_open(&fp, LASTIMG_FILE, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        DBGLN("Warning: Could not create config file");
        return;
    }

    /* Write Drive 0 */
    if (mgr->loaded_image_index[0] >= 0 && mgr->loaded_image_index[0] < mgr->total_images) {
        f_puts(mgr->disk_images[mgr->loaded_image_index[0]], &fp);
    } else {
        f_puts("NONE", &fp);
    }
    f_puts(",", &fp);

    /* Write Drive 1 */
    if (mgr->loaded_image_index[1] >= 0 && mgr->loaded_image_index[1] < mgr->total_images) {
        f_puts(mgr->disk_images[mgr->loaded_image_index[1]], &fp);
    } else {
        f_puts("NONE", &fp);
    }
    f_puts("\r\n", &fp);

    f_sync(&fp);
    sleep_ms(20);
    f_close(&fp);
    sleep_ms(20);

    DBG("Saved config: Drive 0=%s, Drive 1=%s\n",
        mgr->loaded_image_index[0] >= 0 ? mgr->disk_images[mgr->loaded_image_index[0]] : "NONE",
        mgr->loaded_image_index[1] >= 0 ? mgr->disk_images[mgr->loaded_image_index[1]] : "NONE");
}

void disk_mgr_load_config(disk_manager_t *mgr) {
    if (!ensure_mounted()) return;

    FIL fp;
    FRESULT fr = f_open(&fp, LASTIMG_FILE, FA_READ);
    if (fr != FR_OK) {
        DBGLN("No config file found, using defaults");
        return;
    }

    char line[140];
    memset(line, 0, sizeof(line));
    UINT br;
    f_read(&fp, line, 139, &br);
    f_close(&fp);

    /* Strip newline */
    for (int i = 0; line[i]; i++) {
        if (line[i] == '\n' || line[i] == '\r') { line[i] = '\0'; break; }
    }

    /* Parse comma-separated filenames */
    char *comma = strchr(line, ',');
    if (comma) {
        *comma = '\0';
        char *fn0 = line;
        char *fn1 = comma + 1;

        DBG("Loaded config: Drive 0=%s, Drive 1=%s\n", fn0, fn1);

        /* Find Drive 0 */
        if (strcmp(fn0, "NONE") != 0) {
            for (int idx = 0; idx < mgr->total_images; idx++) {
                if (strcmp(mgr->disk_images[idx], fn0) == 0) {
                    disk_mgr_load_image(mgr, 0, idx);
                    break;
                }
            }
        }

        /* Find Drive 1 */
        if (strcmp(fn1, "NONE") != 0) {
            for (int idx = 0; idx < mgr->total_images; idx++) {
                if (strcmp(mgr->disk_images[idx], fn1) == 0) {
                    disk_mgr_load_image(mgr, 1, idx);
                    break;
                }
            }
        }
    }
}

disk_image_t *disk_mgr_get_disk(disk_manager_t *mgr, uint8_t drive) {
    if (drive >= MAX_DRIVES) return NULL;
    return &mgr->disks[drive];
}

int disk_mgr_get_loaded_index(disk_manager_t *mgr, uint8_t drive) {
    if (drive >= MAX_DRIVES) return -1;
    return mgr->loaded_image_index[drive];
}

/* Sector I/O via FatFs */
bool disk_mgr_read_sector(disk_manager_t *mgr, disk_image_t *disk,
                          uint32_t offset, uint8_t *buf, uint16_t len) {
    (void)mgr;
    if (!ensure_mounted()) return false;

    char path[70];
    snprintf(path, sizeof(path), "/%s", disk->filename);

    FIL fp;
    FRESULT fr = f_open(&fp, path, FA_READ);
    if (fr != FR_OK) return false;

    f_lseek(&fp, offset);
    UINT br;
    fr = f_read(&fp, buf, len, &br);
    f_close(&fp);

    return (fr == FR_OK && br == len);
}

bool disk_mgr_write_sector(disk_manager_t *mgr, disk_image_t *disk,
                           uint32_t offset, const uint8_t *buf, uint16_t len) {
    (void)mgr;
    if (!ensure_mounted()) return false;

    char path[70];
    snprintf(path, sizeof(path), "/%s", disk->filename);

    FIL fp;
    FRESULT fr = f_open(&fp, path, FA_WRITE | FA_OPEN_EXISTING);
    if (fr != FR_OK) return false;

    f_lseek(&fp, offset);
    UINT bw;
    fr = f_write(&fp, buf, len, &bw);
    f_sync(&fp);
    sleep_ms(10);
    f_close(&fp);
    sleep_ms(5);

    return (fr == FR_OK && bw == len);
}
