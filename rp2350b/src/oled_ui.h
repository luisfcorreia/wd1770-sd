/*
 * wd1770-sd RP2350B - OLED User Interface
 */

#ifndef OLED_UI_H
#define OLED_UI_H

#include <stdint.h>
#include <stdbool.h>
#include "disk_manager.h"
#include "fdc.h"
#include "u8g2.h"

#define BUTTON_DEBOUNCE_MS  50
#define DISPLAY_UPDATE_INTERVAL 100
#define SCREENSAVER_TIMEOUT_MS  30000

typedef enum {
    UI_MODE_NORMAL,
    UI_MODE_SELECTING_DRIVE_A,
    UI_MODE_SELECTING_DRIVE_B,
    UI_MODE_CONFIRM,
    UI_MODE_SCREENSAVER
} ui_mode_t;

typedef struct {
    u8g2_t u8g2;
    disk_manager_t *disk_mgr;
    fdc_device_t *fdc;
    ui_mode_t ui_mode;
    int temp_drive0_index;
    int temp_drive1_index;
    int temp_scroll_index;
    bool confirm_yes;
    uint64_t last_up_press;
    uint64_t last_down_press;
    uint64_t last_select_press;
    bool select_pressed;
    uint64_t last_display_update;
    uint64_t last_activity_time;
    int test_mode;
} oled_ui_t;

/* Initialize UI */
void oled_ui_init(oled_ui_t *ui);

/* Link subsystems */
void oled_ui_set_disk_manager(oled_ui_t *ui, disk_manager_t *dm);
void oled_ui_set_fdc_device(oled_ui_t *ui, fdc_device_t *fdc);

/* Check button input */
void oled_ui_check_input(oled_ui_t *ui);

/* Update display */
void oled_ui_update_display(oled_ui_t *ui);

/* Periodic update (screensaver, auto-refresh) */
void oled_ui_periodic_update(oled_ui_t *ui);

/* Set test mode flag */
void oled_ui_set_test_mode(oled_ui_t *ui, int mode);

#endif /* OLED_UI_H */
