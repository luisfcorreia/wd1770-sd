/*
 * wd1770-sd RP2350B - FatFs Hardware Configuration
 */

#include "sd_card.h"
#include "hardware_config.h"

/* SPI configuration for SD card */
static spi_t spi = {
    .hw_inst = SD_SPI_INST,
    .miso_gpio = SD_MISO_PIN,
    .mosi_gpio = SD_MOSI_PIN,
    .sck_gpio = SD_SCK_PIN,
    .baud_rate = 16000000,  /* 16 MHz */
    .spi_mode = 0,
    .no_miso_gpio_pull_up = false,
    .set_drive_strength = false,
    .mosi_gpio_drive_strength = 0,
    .sck_gpio_drive_strength = 0,
};

/* SPI interface for SD card */
static sd_spi_if_t spi_if = {
    .spi = &spi,
    .ss_gpio = SD_CS_PIN,
    .set_drive_strength = false,
    .ss_gpio_drive_strength = 0,
};

/* SD card instance */
static sd_card_t sd_card = {
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if,
    .use_card_detect = false,
};

size_t sd_get_num(void) {
    return 1;
}

sd_card_t *sd_get_by_num(size_t num) {
    return (num == 0) ? &sd_card : NULL;
}
