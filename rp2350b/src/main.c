/*
 * wd1770-sd RP2350B - Main Entry Point
 *
 * WD1770 Drop-in Replacement with SD Card Support
 * Target: WeAct RP2350B Core Board
 * Language: C (Pico SDK)
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "hardware_config.h"
#include "bus_io.h"
#include "fdc.h"
#include "disk_manager.h"
#include "oled_ui.h"

/* Test mode - simulates Timex system signals */
static int test_mode = 1;

/* Global objects */
static disk_manager_t disk_mgr;
static fdc_device_t fdc;
static oled_ui_t ui;

/* SD card init via FatFs */
static bool init_sd_card(void) {
    extern bool sd_init_driver(void);
    return sd_init_driver();
}

int main(void) {
    stdio_init_all();
    sleep_ms(2000);

    DBGLN("WD1770 SD Card Emulator");
    DBGLN("RP2350B Pure C Build");

    /* Initialize pins */
    bus_io_init();

    /* Initialize SD card */
    if (!init_sd_card()) {
        DBGLN("FATAL: SD Card initialization failed!");
        while (1) {
            gpio_put(LED_PIN, !gpio_get(LED_PIN));
            sleep_ms(100);
        }
    }

    /* Initialize UI (OLED) */
    oled_ui_init(&ui);
    oled_ui_set_test_mode(&ui, test_mode);

    /* Initialize disk manager */
    disk_mgr_init(&disk_mgr);
    disk_mgr_scan(&disk_mgr);

    /* Load last configuration or defaults */
    disk_mgr_load_config(&disk_mgr);

    if (disk_mgr_get_loaded_index(&disk_mgr, 0) == -1 && disk_mgr_get_total(&disk_mgr) > 0) {
        DBGLN("First boot - loading default images");
        disk_mgr_load_image(&disk_mgr, 0, 0);
        if (disk_mgr_get_total(&disk_mgr) > 1) {
            disk_mgr_load_image(&disk_mgr, 1, 1);
        }
        disk_mgr_save_config(&disk_mgr);
    }

    /* Initialize FDC */
    fdc_init(&fdc);
    fdc_set_disk_manager(&fdc, &disk_mgr);

    /* Initialize PIO bus */
    pio_bus_init();

    /* Link UI to subsystems */
    oled_ui_set_disk_manager(&ui, &disk_mgr);
    oled_ui_set_fdc_device(&ui, &fdc);

    /* Initial display update */
    oled_ui_update_display(&ui);

    DBGLN("Ready!");
    DBGLN("Safe to reset/power off anytime EXCEPT during 'Saving config...' message");

    /* Main loop */
    while (1) {
        /* UI always runs */
        oled_ui_check_input(&ui);

        /* Check if FDC is enabled */
        if (fdc_is_enabled(&fdc)) {
            fdc_check_drive_select(&fdc);

            /* Handle bus transactions (polling) */
            fdc_handle_bus(&fdc);

            /* Process FDC state machine */
            fdc_process_state_machine(&fdc);

            /* Update output signals */
            fdc_update_outputs(&fdc);
        } else {
            fdc_disable(&fdc);
        }

        /* Periodic display update */
        oled_ui_periodic_update(&ui);
    }

    return 0;
}
