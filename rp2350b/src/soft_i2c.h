/*
 * wd1770-sd RP2350B - Software I2C for OLED Display
 */

#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include <stdint.h>
#include <stdbool.h>

/* Initialize software I2C on given pins */
void soft_i2c_init(uint scl_pin, uint sda_pin);

/* Send one byte, returns true on ACK */
bool soft_i2c_write_byte(uint8_t data);

/* Start condition */
void soft_i2c_start(void);

/* Stop condition */
void soft_i2c_stop(void);

/* Write byte with ACK check */
bool soft_i2c_write(uint8_t data);

/* Write buffer */
bool soft_i2c_write_buf(const uint8_t *data, uint len);

#endif /* SOFT_I2C_H */
