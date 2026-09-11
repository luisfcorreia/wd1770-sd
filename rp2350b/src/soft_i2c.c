/*
 * wd1770-sd RP2350B - Software I2C Implementation
 */

#include "soft_i2c.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware_config.h"

static uint i2c_scl;
static uint i2c_sda;

static inline void delay_us(uint32_t us) {
    busy_wait_us(us);
}

static inline void scl_high(void) {
    gpio_set_dir(i2c_scl, GPIO_IN);
}

static inline void scl_low(void) {
    gpio_set_dir(i2c_scl, GPIO_OUT);
    gpio_put(i2c_scl, 0);
}

static inline void sda_high(void) {
    gpio_set_dir(i2c_sda, GPIO_IN);
}

static inline void sda_low(void) {
    gpio_set_dir(i2c_sda, GPIO_OUT);
    gpio_put(i2c_sda, 0);
}

static inline bool sda_read(void) {
    return gpio_get(i2c_sda);
}

void soft_i2c_init(uint scl_pin, uint sda_pin) {
    i2c_scl = scl_pin;
    i2c_sda = sda_pin;

    gpio_init(i2c_scl);
    gpio_init(i2c_sda);
    gpio_set_dir(i2c_scl, GPIO_IN);
    gpio_set_dir(i2c_sda, GPIO_IN);
    gpio_pull_up(i2c_scl);
    gpio_pull_up(i2c_sda);
}

void soft_i2c_start(void) {
    sda_high();
    scl_high();
    delay_us(2);
    sda_low();
    delay_us(2);
    scl_low();
    delay_us(2);
}

void soft_i2c_stop(void) {
    sda_low();
    scl_high();
    delay_us(2);
    sda_high();
    delay_us(2);
}

bool soft_i2c_write_byte(uint8_t data) {
    bool ack = true;

    for (int i = 7; i >= 0; i--) {
        if (data & (1 << i)) {
            sda_high();
        } else {
            sda_low();
        }
        scl_high();
        delay_us(2);
        scl_low();
        delay_us(2);
    }

    /* Read ACK */
    sda_high();
    scl_high();
    delay_us(2);
    ack = !sda_read();  /* ACK = SDA low */
    scl_low();
    delay_us(2);

    return ack;
}

bool soft_i2c_write(uint8_t data) {
    return soft_i2c_write_byte(data);
}

bool soft_i2c_write_buf(const uint8_t *data, uint len) {
    for (uint i = 0; i < len; i++) {
        if (!soft_i2c_write_byte(data[i])) {
            soft_i2c_stop();
            return false;
        }
    }
    return true;
}
