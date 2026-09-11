/*
 * wd1770-sd RP2350B - OLED User Interface Implementation
 */

#include "oled_ui.h"
#include "hardware_config.h"
#include "u8g2_setup.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "u8g2.h"

#include <string.h>
#include <stdio.h>

#define OLED_FONT u8g2_font_6x10_tr

static void display_normal_mode(oled_ui_t *ui);
static void display_selecting_drive_a(oled_ui_t *ui);
static void display_selecting_drive_b(oled_ui_t *ui);
static void display_confirm(oled_ui_t *ui);
static void handle_up_button(oled_ui_t *ui);
static void handle_down_button(oled_ui_t *ui);
static void handle_select_button(oled_ui_t *ui);
static void show_message(oled_ui_t *ui, const char *msg);
static void load_selected_images(oled_ui_t *ui);

void oled_ui_init(oled_ui_t *ui) {
    memset(ui, 0, sizeof(oled_ui_t));
    ui->ui_mode = UI_MODE_NORMAL;
    ui->temp_drive0_index = 0;
    ui->temp_drive1_index = -1;
    ui->temp_scroll_index = 0;
    ui->confirm_yes = true;
    ui->test_mode = 1;

    /* Initialize U8g2 for SH1106 OLED */
    u8g2_setup_init(&ui->u8g2);

    u8g2_ClearBuffer(&ui->u8g2);
    u8g2_SetFont(&ui->u8g2, OLED_FONT);
    u8g2_DrawStr(&ui->u8g2, 0, 10, "WD1770 Emulator");
    u8g2_DrawStr(&ui->u8g2, 0, 22, "Initializing...");
    u8g2_SendBuffer(&ui->u8g2);
}

void oled_ui_set_disk_manager(oled_ui_t *ui, disk_manager_t *dm) {
    ui->disk_mgr = dm;
}

void oled_ui_set_fdc_device(oled_ui_t *ui, fdc_device_t *fdc) {
    ui->fdc = fdc;
}

void oled_ui_set_test_mode(oled_ui_t *ui, int mode) {
    ui->test_mode = mode;
    if (mode) {
        gpio_pull_down(WD_DDEN_PIN);  /* Simulate enabled (active low) */
        gpio_pull_up(WD_DS0_PIN);     /* Simulate drive 0 selected */
        gpio_pull_down(WD_DS1_PIN);   /* Simulate drive 1 not selected */
    } else {
        gpio_disable_pulls(WD_DDEN_PIN);
        gpio_disable_pulls(WD_DS0_PIN);
        gpio_disable_pulls(WD_DS1_PIN);
    }
}

void oled_ui_check_input(oled_ui_t *ui) {
    uint64_t now = time_us_64() / 1000;  /* ms */

    /* Screensaver wake */
    if (ui->ui_mode == UI_MODE_SCREENSAVER) {
        if (gpio_get(BTN_UP_PIN) == 0 || gpio_get(BTN_DOWN_PIN) == 0 || gpio_get(BTN_SELECT_PIN) == 0) {
            ui->last_activity_time = now;
            ui->ui_mode = UI_MODE_NORMAL;
            oled_ui_update_display(ui);
        }
        return;
    }

    /* UP button */
    if (gpio_get(BTN_UP_PIN) == 0) {
        if (now - ui->last_up_press > BUTTON_DEBOUNCE_MS) {
            ui->last_up_press = now;
            ui->last_activity_time = now;
            handle_up_button(ui);
        }
    }

    /* DOWN button */
    if (gpio_get(BTN_DOWN_PIN) == 0) {
        if (now - ui->last_down_press > BUTTON_DEBOUNCE_MS) {
            ui->last_down_press = now;
            ui->last_activity_time = now;
            handle_down_button(ui);
        }
    }

    /* SELECT button (edge detection) */
    bool select_state = gpio_get(BTN_SELECT_PIN);
    if (select_state == 0 && !ui->select_pressed) {
        ui->select_pressed = true;
        ui->last_select_press = now;
    }
    if (select_state == 1 && ui->select_pressed) {
        uint64_t press_duration = now - ui->last_select_press;
        if (press_duration >= BUTTON_DEBOUNCE_MS) {
            ui->last_activity_time = now;
            handle_select_button(ui);
        }
        ui->select_pressed = false;
    }
}

static void handle_up_button(oled_ui_t *ui) {
    if (!ui->disk_mgr) return;

    switch (ui->ui_mode) {
        case UI_MODE_NORMAL:
            break;
        case UI_MODE_SELECTING_DRIVE_A:
            ui->temp_scroll_index--;
            if (ui->temp_scroll_index < 0)
                ui->temp_scroll_index = disk_mgr_get_total(ui->disk_mgr) - 1;
            oled_ui_update_display(ui);
            break;
        case UI_MODE_SELECTING_DRIVE_B:
            ui->temp_scroll_index--;
            if (ui->temp_scroll_index < -1)
                ui->temp_scroll_index = disk_mgr_get_total(ui->disk_mgr) - 1;
            oled_ui_update_display(ui);
            break;
        case UI_MODE_CONFIRM:
            ui->confirm_yes = !ui->confirm_yes;
            oled_ui_update_display(ui);
            break;
        default:
            break;
    }
}

static void handle_down_button(oled_ui_t *ui) {
    if (!ui->disk_mgr) return;

    switch (ui->ui_mode) {
        case UI_MODE_NORMAL:
            break;
        case UI_MODE_SELECTING_DRIVE_A:
            ui->temp_scroll_index++;
            if (ui->temp_scroll_index >= disk_mgr_get_total(ui->disk_mgr))
                ui->temp_scroll_index = 0;
            oled_ui_update_display(ui);
            break;
        case UI_MODE_SELECTING_DRIVE_B:
            ui->temp_scroll_index++;
            if (ui->temp_scroll_index >= disk_mgr_get_total(ui->disk_mgr))
                ui->temp_scroll_index = -1;
            oled_ui_update_display(ui);
            break;
        case UI_MODE_CONFIRM:
            ui->confirm_yes = !ui->confirm_yes;
            oled_ui_update_display(ui);
            break;
        default:
            break;
    }
}

static void handle_select_button(oled_ui_t *ui) {
    if (!ui->disk_mgr) return;

    switch (ui->ui_mode) {
        case UI_MODE_NORMAL:
            ui->ui_mode = UI_MODE_SELECTING_DRIVE_A;
            ui->temp_scroll_index = (disk_mgr_get_loaded_index(ui->disk_mgr, 0) >= 0) ?
                                    disk_mgr_get_loaded_index(ui->disk_mgr, 0) : 0;
            oled_ui_update_display(ui);
            break;

        case UI_MODE_SELECTING_DRIVE_A:
            ui->temp_drive0_index = ui->temp_scroll_index;
            ui->ui_mode = UI_MODE_SELECTING_DRIVE_B;
            ui->temp_scroll_index = (disk_mgr_get_loaded_index(ui->disk_mgr, 1) >= 0) ?
                                    disk_mgr_get_loaded_index(ui->disk_mgr, 1) : -1;
            oled_ui_update_display(ui);
            break;

        case UI_MODE_SELECTING_DRIVE_B:
            ui->temp_drive1_index = ui->temp_scroll_index;
            ui->ui_mode = UI_MODE_CONFIRM;
            ui->confirm_yes = true;
            oled_ui_update_display(ui);
            break;

        case UI_MODE_CONFIRM:
            if (ui->confirm_yes) {
                load_selected_images(ui);
            } else {
                ui->ui_mode = UI_MODE_SELECTING_DRIVE_A;
                ui->temp_scroll_index = ui->temp_drive0_index;
                oled_ui_update_display(ui);
            }
            break;

        default:
            break;
    }
}

static void load_selected_images(oled_ui_t *ui) {
    if (!ui->disk_mgr) return;

    show_message(ui, "Loading images...");

    disk_mgr_load_image(ui->disk_mgr, 0, ui->temp_drive0_index);

    if (ui->temp_drive1_index >= 0) {
        disk_mgr_load_image(ui->disk_mgr, 1, ui->temp_drive1_index);
    } else {
        disk_mgr_eject(ui->disk_mgr, 1);
    }

    show_message(ui, "Saving config...");
    disk_mgr_save_config(ui->disk_mgr);

    sleep_ms(100);

    show_message(ui, "Done!");
    sleep_ms(500);

    ui->ui_mode = UI_MODE_NORMAL;
    oled_ui_update_display(ui);
}

static void show_message(oled_ui_t *ui, const char *msg) {
    u8g2_ClearBuffer(&ui->u8g2);
    u8g2_SetFont(&ui->u8g2, OLED_FONT);
    u8g2_DrawStr(&ui->u8g2, 0, 32, msg);
    u8g2_SendBuffer(&ui->u8g2);
}

void oled_ui_update_display(oled_ui_t *ui) {
    switch (ui->ui_mode) {
        case UI_MODE_NORMAL:
            display_normal_mode(ui);
            break;
        case UI_MODE_SELECTING_DRIVE_A:
            display_selecting_drive_a(ui);
            break;
        case UI_MODE_SELECTING_DRIVE_B:
            display_selecting_drive_b(ui);
            break;
        case UI_MODE_CONFIRM:
            display_confirm(ui);
            break;
        default:
            break;
    }
}

void oled_ui_periodic_update(oled_ui_t *ui) {
    uint64_t now = time_us_64() / 1000;

    if (ui->ui_mode == UI_MODE_NORMAL && now - ui->last_activity_time > SCREENSAVER_TIMEOUT_MS) {
        ui->ui_mode = UI_MODE_SCREENSAVER;
        u8g2_ClearBuffer(&ui->u8g2);
        u8g2_SendBuffer(&ui->u8g2);
        return;
    }

    if (ui->ui_mode == UI_MODE_NORMAL && now - ui->last_display_update > DISPLAY_UPDATE_INTERVAL) {
        display_normal_mode(ui);
        ui->last_display_update = now;
    }
}

static void display_normal_mode(oled_ui_t *ui) {
    if (!ui->disk_mgr || !ui->fdc) return;

    char buf[32];
    u8g2_ClearBuffer(&ui->u8g2);
    u8g2_SetFont(&ui->u8g2, OLED_FONT);

    /* Drive A */
    disk_image_t *diskA = disk_mgr_get_disk(ui->disk_mgr, 0);
    if (diskA && diskA->filename[0] != '\0') {
        char fname[21];
        strncpy(fname, diskA->filename, 18);
        fname[18] = '\0';
        if (strlen(diskA->filename) > 18) {
            strcpy(fname + 15, "...");
        }
        snprintf(buf, sizeof(buf), "A:%s", fname);
        u8g2_DrawStr(&ui->u8g2, 0, 10, buf);

        if (fdc_get_active_drive(ui->fdc) == 0) {
            snprintf(buf, sizeof(buf), " T:%d/%d", fdc_get_current_track(ui->fdc), diskA->tracks - 1);
        } else {
            strcpy(buf, " T:--");
        }
        u8g2_DrawStr(&ui->u8g2, 0, 20, buf);
    } else {
        u8g2_DrawStr(&ui->u8g2, 0, 10, "A:(empty)");
    }

    /* Drive B */
    disk_image_t *diskB = disk_mgr_get_disk(ui->disk_mgr, 1);
    if (diskB && diskB->filename[0] != '\0') {
        char fname[21];
        strncpy(fname, diskB->filename, 18);
        fname[18] = '\0';
        if (strlen(diskB->filename) > 18) {
            strcpy(fname + 15, "...");
        }
        snprintf(buf, sizeof(buf), "B:%s", fname);
        u8g2_DrawStr(&ui->u8g2, 0, 34, buf);

        if (fdc_get_active_drive(ui->fdc) == 1) {
            snprintf(buf, sizeof(buf), " T:%d/%d", fdc_get_current_track(ui->fdc), diskB->tracks - 1);
        } else {
            strcpy(buf, " T:--");
        }
        u8g2_DrawStr(&ui->u8g2, 0, 44, buf);
    } else {
        u8g2_DrawStr(&ui->u8g2, 0, 34, "B:(empty)");
    }

    /* Status line */
    if (ui->test_mode) {
        u8g2_DrawStr(&ui->u8g2, 0, 63, "TEST MODE");
        u8g2_DrawStr(&ui->u8g2, 60, 63, "Select=Menu");
    } else {
        u8g2_DrawStr(&ui->u8g2, 0, 63, "Press to select");
    }

    u8g2_SendBuffer(&ui->u8g2);
}

static void display_selecting_drive_a(oled_ui_t *ui) {
    if (!ui->disk_mgr) return;

    char buf[32];
    u8g2_ClearBuffer(&ui->u8g2);
    u8g2_SetFont(&ui->u8g2, OLED_FONT);

    u8g2_DrawStr(&ui->u8g2, 0, 8, "Select Drive A:");
    u8g2_DrawHLine(&ui->u8g2, 0, 10, 128);

    int total = disk_mgr_get_total(ui->disk_mgr);
    int start_idx = (ui->temp_scroll_index - 2 > 0) ? ui->temp_scroll_index - 2 : 0;
    int end_idx = (start_idx + 4 < total - 1) ? start_idx + 4 : total - 1;
    if (end_idx - start_idx < 4 && start_idx > 0) {
        start_idx = (end_idx - 4 > 0) ? end_idx - 4 : 0;
    }

    int y = 22;
    for (int i = start_idx; i <= end_idx && i < total; i++) {
        char fname[24];
        const char *img_name = disk_mgr_get_image_name(ui->disk_mgr, i);
        if (!img_name) continue;
        strncpy(fname, img_name, 20);
        fname[20] = '\0';
        if (strlen(img_name) > 20) strcpy(fname + 17, "...");

        if (i == ui->temp_scroll_index) {
            u8g2_SetDrawColor(&ui->u8g2, 1);
            u8g2_DrawBox(&ui->u8g2, 0, y - 8, 128, 10);
            u8g2_SetDrawColor(&ui->u8g2, 0);
            snprintf(buf, sizeof(buf), ">%s", fname);
            u8g2_DrawStr(&ui->u8g2, 0, y, buf);
            u8g2_SetDrawColor(&ui->u8g2, 1);
        } else {
            snprintf(buf, sizeof(buf), " %s", fname);
            u8g2_DrawStr(&ui->u8g2, 0, y, buf);
        }
        y += 10;
    }

    u8g2_DrawStr(&ui->u8g2, 0, 63, "Up/Down=Scroll Sel=OK");
    u8g2_SendBuffer(&ui->u8g2);
}

static void display_selecting_drive_b(oled_ui_t *ui) {
    if (!ui->disk_mgr) return;

    char buf[32];
    u8g2_ClearBuffer(&ui->u8g2);
    u8g2_SetFont(&ui->u8g2, OLED_FONT);

    u8g2_DrawStr(&ui->u8g2, 0, 8, "Select Drive B:");
    u8g2_DrawHLine(&ui->u8g2, 0, 10, 128);

    int total = disk_mgr_get_total(ui->disk_mgr);
    int start_idx = (ui->temp_scroll_index - 2 > -1) ? ui->temp_scroll_index - 2 : -1;
    int end_idx = (start_idx + 4 < total - 1) ? start_idx + 4 : total - 1;
    if (end_idx - start_idx < 4 && start_idx > -1) {
        start_idx = (end_idx - 4 > -1) ? end_idx - 4 : -1;
    }

    int y = 22;

    /* NONE option at index -1 */
    if (start_idx == -1) {
        if (ui->temp_scroll_index == -1) {
            u8g2_SetDrawColor(&ui->u8g2, 1);
            u8g2_DrawBox(&ui->u8g2, 0, y - 8, 128, 10);
            u8g2_SetDrawColor(&ui->u8g2, 0);
            u8g2_DrawStr(&ui->u8g2, 0, y, ">NONE");
            u8g2_SetDrawColor(&ui->u8g2, 1);
        } else {
            u8g2_DrawStr(&ui->u8g2, 0, y, " NONE");
        }
        y += 10;
        start_idx = 0;
    }

    for (int i = start_idx; i <= end_idx && i < total; i++) {
        char fname[24];
        const char *img_name = disk_mgr_get_image_name(ui->disk_mgr, i);
        if (!img_name) continue;
        strncpy(fname, img_name, 20);
        fname[20] = '\0';
        if (strlen(img_name) > 20) strcpy(fname + 17, "...");

        if (i == ui->temp_scroll_index) {
            u8g2_SetDrawColor(&ui->u8g2, 1);
            u8g2_DrawBox(&ui->u8g2, 0, y - 8, 128, 10);
            u8g2_SetDrawColor(&ui->u8g2, 0);
            snprintf(buf, sizeof(buf), ">%s", fname);
            u8g2_DrawStr(&ui->u8g2, 0, y, buf);
            u8g2_SetDrawColor(&ui->u8g2, 1);
        } else {
            snprintf(buf, sizeof(buf), " %s", fname);
            u8g2_DrawStr(&ui->u8g2, 0, y, buf);
        }
        y += 10;
    }

    u8g2_DrawStr(&ui->u8g2, 0, 63, "Up/Down=Scroll Sel=OK");
    u8g2_SendBuffer(&ui->u8g2);
}

static void display_confirm(oled_ui_t *ui) {
    if (!ui->disk_mgr) return;

    char buf[32];
    u8g2_ClearBuffer(&ui->u8g2);
    u8g2_SetFont(&ui->u8g2, OLED_FONT);

    u8g2_DrawStr(&ui->u8g2, 0, 8, "Load these images?");
    u8g2_DrawHLine(&ui->u8g2, 0, 10, 128);

    /* Drive A */
    char fname[21];
    const char *img_name = disk_mgr_get_image_name(ui->disk_mgr, ui->temp_drive0_index);
    if (img_name) {
        strncpy(fname, img_name, 18);
        fname[18] = '\0';
        if (strlen(img_name) > 18) strcpy(fname + 15, "...");
        snprintf(buf, sizeof(buf), "A:%s", fname);
    } else {
        strcpy(buf, "A:(empty)");
    }
    u8g2_DrawStr(&ui->u8g2, 0, 22, buf);

    /* Drive B */
    if (ui->temp_drive1_index >= 0) {
        img_name = disk_mgr_get_image_name(ui->disk_mgr, ui->temp_drive1_index);
        if (img_name) {
            strncpy(fname, img_name, 18);
            fname[18] = '\0';
            if (strlen(img_name) > 18) strcpy(fname + 15, "...");
            snprintf(buf, sizeof(buf), "B:%s", fname);
        } else {
            strcpy(buf, "B:(empty)");
        }
    } else {
        strcpy(buf, "B:(empty)");
    }
    u8g2_DrawStr(&ui->u8g2, 0, 32, buf);

    /* YES/NO */
    if (ui->confirm_yes) {
        u8g2_SetDrawColor(&ui->u8g2, 1);
        u8g2_DrawBox(&ui->u8g2, 27, 42, 35, 9);
        u8g2_SetDrawColor(&ui->u8g2, 0);
        u8g2_DrawStr(&ui->u8g2, 30, 50, "[YES]");
        u8g2_SetDrawColor(&ui->u8g2, 1);
        u8g2_DrawStr(&ui->u8g2, 75, 50, "[NO]");
    } else {
        u8g2_DrawStr(&ui->u8g2, 30, 50, "[YES]");
        u8g2_SetDrawColor(&ui->u8g2, 1);
        u8g2_DrawBox(&ui->u8g2, 72, 42, 29, 9);
        u8g2_SetDrawColor(&ui->u8g2, 0);
        u8g2_DrawStr(&ui->u8g2, 75, 50, "[NO]");
        u8g2_SetDrawColor(&ui->u8g2, 1);
    }

    u8g2_DrawStr(&ui->u8g2, 0, 63, "Up/Down=Toggle Sel=OK");
    u8g2_SendBuffer(&ui->u8g2);
}
