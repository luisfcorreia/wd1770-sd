/*
 * wd1770-sd RP2350B - U8g2 Setup for SH1106 OLED (Software I2C)
 */

#include "u8g2_setup.h"
#include "hardware_config.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

/*
 * U8g2 byte callback for software I2C.
 * Handles I2C start/stop/data transfer via bit-bang on OLED_SCL_PIN and OLED_SDA_PIN.
 */
static uint8_t u8x8_byte_sw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    static uint8_t i2c_addr_byte = 0;

    switch (msg) {
        case U8X8_MSG_BYTE_SEND:
            if (arg_ptr) {
                uint8_t *data = (uint8_t *)arg_ptr;
                for (uint8_t i = 0; i < arg_int; i++) {
                    /* Send byte via bit-bang I2C */
                    uint8_t byte = data[i];
                    for (int bit = 7; bit >= 0; bit--) {
                        gpio_set_dir(OLED_SDA_PIN, (byte & (1 << bit)) ? GPIO_IN : GPIO_OUT);
                        gpio_put(OLED_SCL_PIN, 0);
                        gpio_set_dir(OLED_SCL_PIN, GPIO_OUT);
                        busy_wait_us(1);
                        gpio_set_dir(OLED_SCL_PIN, GPIO_IN);
                        gpio_pull_up(OLED_SCL_PIN);
                        busy_wait_us(1);
                    }
                    /* ACK */
                    gpio_set_dir(OLED_SDA_PIN, GPIO_IN);
                    gpio_put(OLED_SCL_PIN, 0);
                    gpio_set_dir(OLED_SCL_PIN, GPIO_OUT);
                    busy_wait_us(1);
                    gpio_set_dir(OLED_SCL_PIN, GPIO_IN);
                    gpio_pull_up(OLED_SCL_PIN);
                    busy_wait_us(1);
                }
            }
            break;

        case U8X8_MSG_BYTE_INIT:
            break;

        case U8X8_MSG_BYTE_START_TRANSFER: {
            /* I2C start condition */
            gpio_set_dir(OLED_SDA_PIN, GPIO_IN);
            gpio_set_dir(OLED_SCL_PIN, GPIO_IN);
            busy_wait_us(2);
            gpio_set_dir(OLED_SDA_PIN, GPIO_OUT);
            gpio_put(OLED_SDA_PIN, 0);
            busy_wait_us(2);
            gpio_set_dir(OLED_SCL_PIN, GPIO_OUT);
            gpio_put(OLED_SCL_PIN, 0);
            busy_wait_us(2);

            /* Send I2C address */
            i2c_addr_byte = u8x8->i2c_addr;
            uint8_t addr = i2c_addr_byte;
            for (int bit = 7; bit >= 0; bit--) {
                gpio_set_dir(OLED_SDA_PIN, (addr & (1 << bit)) ? GPIO_IN : GPIO_OUT);
                gpio_set_dir(OLED_SCL_PIN, GPIO_IN);
                busy_wait_us(1);
                gpio_set_dir(OLED_SCL_PIN, GPIO_OUT);
                gpio_put(OLED_SCL_PIN, 0);
                busy_wait_us(1);
            }
            /* ACK */
            gpio_set_dir(OLED_SDA_PIN, GPIO_IN);
            gpio_set_dir(OLED_SCL_PIN, GPIO_IN);
            busy_wait_us(1);
            gpio_set_dir(OLED_SCL_PIN, GPIO_OUT);
            gpio_put(OLED_SCL_PIN, 0);
            busy_wait_us(1);
            break;
        }

        case U8X8_MSG_BYTE_END_TRANSFER: {
            /* I2C stop condition */
            gpio_set_dir(OLED_SDA_PIN, GPIO_OUT);
            gpio_put(OLED_SDA_PIN, 0);
            gpio_set_dir(OLED_SCL_PIN, GPIO_IN);
            busy_wait_us(2);
            gpio_set_dir(OLED_SDA_PIN, GPIO_IN);
            gpio_pull_up(OLED_SDA_PIN);
            busy_wait_us(2);
            break;
        }

        case U8X8_MSG_BYTE_SET_DC:
            /* Not used for I2C */
            break;

        default:
            return 0;
    }
    return 1;
}

/*
 * U8g2 GPIO and delay callback for Pico SDK.
 */
static uint8_t u8x8_gpio_and_delay_pico(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    (void)u8x8;
    (void)arg_ptr;

    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            gpio_init(OLED_SCL_PIN);
            gpio_init(OLED_SDA_PIN);
            gpio_set_dir(OLED_SCL_PIN, GPIO_IN);
            gpio_set_dir(OLED_SDA_PIN, GPIO_IN);
            gpio_pull_up(OLED_SCL_PIN);
            gpio_pull_up(OLED_SDA_PIN);
            break;

        case U8X8_MSG_GPIO_I2C_CLOCK:
            if (arg_int) {
                gpio_set_dir(OLED_SCL_PIN, GPIO_IN);
            } else {
                gpio_set_dir(OLED_SCL_PIN, GPIO_OUT);
                gpio_put(OLED_SCL_PIN, 0);
            }
            break;

        case U8X8_MSG_GPIO_I2C_DATA:
            if (arg_int) {
                gpio_set_dir(OLED_SDA_PIN, GPIO_IN);
            } else {
                gpio_set_dir(OLED_SDA_PIN, GPIO_OUT);
                gpio_put(OLED_SDA_PIN, 0);
            }
            break;

        case U8X8_MSG_DELAY_MILLI:
            sleep_ms(arg_int);
            break;

        case U8X8_MSG_DELAY_10MICRO:
            busy_wait_us(10 * arg_int);
            break;

        case U8X8_MSG_DELAY_100NANO:
            busy_wait_us(1);
            break;

        default:
            u8x8_SetGPIOResult(u8x8, 1);
            break;
    }
    return 1;
}

void u8g2_setup_init(u8g2_t *u8g2) {
    /* Setup U8g2 for SH1106 128x64 with software I2C */
    u8g2_Setup_sh1106_128x64_noname_f(
        u8g2,
        U8G2_R0,
        u8x8_byte_sw_i2c,
        u8x8_gpio_and_delay_pico
    );

    /* Initialize display */
    u8g2_InitDisplay(u8g2);
    u8g2_SetPowerSave(u8g2, 0);
}
