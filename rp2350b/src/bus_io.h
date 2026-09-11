/*
 * wd1770-sd RP2350B - GPIO + PIO Bus Access Layer
 */

#ifndef BUS_IO_H
#define BUS_IO_H

#include <stdint.h>
#include <stdbool.h>

/* Initialize all GPIO pins */
void bus_io_init(void);

/* Data bus operations (GPIO fallback) */
uint8_t bus_read_data(void);
void bus_write_data(uint8_t data);
void bus_release(void);
void bus_set_direction_out(void);
void bus_set_direction_in(void);

/* Control signal reads */
bool bus_cs_active(void);
bool bus_rw_read(void);
uint8_t bus_read_address(void);
bool bus_dden_active(void);
uint8_t bus_read_drive_select(void);

/* Output control signals */
void bus_set_intrq(bool state);
void bus_set_drq(bool state);

/* PIO-based bus operations */
void pio_bus_init(void);
void pio_bus_start_read(uint8_t *dest);
void pio_bus_start_write(uint8_t data);

#endif /* BUS_IO_H */
