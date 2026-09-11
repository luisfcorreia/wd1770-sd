/*
 * wd1770-sd RP2350B - GPIO Bus Access Implementation
 */

#include "hardware_config.h"
#include "bus_io.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"

#include <stdio.h>

#include "data_bus.pio.h"

static PIO bus_pio = pio0;
static uint sm_read = 0xff;
static uint offset_read = 0;

void bus_io_init(void) {
    /* Data bus: GP1-GP8 as inputs (tri-state) */
    for (int i = 0; i < WD_DATA_BITS; i++) {
        gpio_init(WD_DATA_BASE + i);
        gpio_set_dir(WD_DATA_BASE + i, GPIO_IN);
    }

    /* Control signals: inputs */
    gpio_init(WD_A0_PIN);   gpio_set_dir(WD_A0_PIN, GPIO_IN);
    gpio_init(WD_A1_PIN);   gpio_set_dir(WD_A1_PIN, GPIO_IN);
    gpio_init(WD_CS_PIN);   gpio_set_dir(WD_CS_PIN, GPIO_IN);
    gpio_init(WD_RW_PIN);   gpio_set_dir(WD_RW_PIN, GPIO_IN);

    /* Output signals */
    gpio_init(WD_INTRQ_PIN); gpio_set_dir(WD_INTRQ_PIN, GPIO_OUT);
    gpio_init(WD_DRQ_PIN);   gpio_set_dir(WD_DRQ_PIN, GPIO_OUT);
    gpio_put(WD_INTRQ_PIN, 0);
    gpio_put(WD_DRQ_PIN, 0);

    /* DDEN, DS0, DS1: inputs (optionally with pull-ups for test mode) */
    gpio_init(WD_DDEN_PIN);  gpio_set_dir(WD_DDEN_PIN, GPIO_IN);
    gpio_init(WD_DS0_PIN);   gpio_set_dir(WD_DS0_PIN, GPIO_IN);
    gpio_init(WD_DS1_PIN);   gpio_set_dir(WD_DS1_PIN, GPIO_IN);

    /* Buttons: active low with internal pull-up */
    gpio_init(BTN_UP_PIN);     gpio_set_dir(BTN_UP_PIN, GPIO_IN); gpio_pull_up(BTN_UP_PIN);
    gpio_init(BTN_DOWN_PIN);   gpio_set_dir(BTN_DOWN_PIN, GPIO_IN); gpio_pull_up(BTN_DOWN_PIN);
    gpio_init(BTN_SELECT_PIN); gpio_set_dir(BTN_SELECT_PIN, GPIO_IN); gpio_pull_up(BTN_SELECT_PIN);

    /* LED */
    gpio_init(LED_PIN); gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);
}

/* Data bus: read 8 bits via GPIO registers (fast) */
uint8_t bus_read_data(void) {
    /* Set all 8 data pins to input */
    for (int i = 0; i < WD_DATA_BITS; i++) {
        gpio_set_dir(WD_DATA_BASE + i, GPIO_IN);
    }
    busy_wait_us(1);
    return (uint8_t)(sio_hw->gpio_in & WD_DATA_MASK);
}

void bus_write_data(uint8_t data) {
    /* Set all 8 data pins to output */
    for (int i = 0; i < WD_DATA_BITS; i++) {
        gpio_set_dir(WD_DATA_BASE + i, GPIO_OUT);
    }
    /* Write all 8 bits at once */
    sio_hw->gpio_out = (sio_hw->gpio_out & ~WD_DATA_MASK) | (data & WD_DATA_MASK);
}

void bus_release(void) {
    for (int i = 0; i < WD_DATA_BITS; i++) {
        gpio_set_dir(WD_DATA_BASE + i, GPIO_IN);
    }
}

void bus_set_direction_out(void) {
    for (int i = 0; i < WD_DATA_BITS; i++) {
        gpio_set_dir(WD_DATA_BASE + i, GPIO_OUT);
    }
}

void bus_set_direction_in(void) {
    for (int i = 0; i < WD_DATA_BITS; i++) {
        gpio_set_dir(WD_DATA_BASE + i, GPIO_IN);
    }
}

/* Control signal reads */
bool bus_cs_active(void) {
    return gpio_get(WD_CS_PIN) == 0;  /* Active low */
}

bool bus_rw_read(void) {
    return gpio_get(WD_RW_PIN) == 1;  /* HIGH = read, LOW = write */
}

uint8_t bus_read_address(void) {
    return (gpio_get(WD_A1_PIN) << 1) | gpio_get(WD_A0_PIN);
}

bool bus_dden_active(void) {
    return gpio_get(WD_DDEN_PIN) == 0;  /* Active low */
}

uint8_t bus_read_drive_select(void) {
    if (gpio_get(WD_DS0_PIN)) return 0;
    if (gpio_get(WD_DS1_PIN)) return 1;
    return 0xff;  /* No drive selected */
}

/* Output signals */
void bus_set_intrq(bool state) {
    gpio_put(WD_INTRQ_PIN, state ? 1 : 0);
}

void bus_set_drq(bool state) {
    gpio_put(WD_DRQ_PIN, state ? 1 : 0);
}

/* PIO-based bus operations */
void pio_bus_init(void) {
    /* Claim PIO and state machines */
    bus_pio = pio0;
    sm_read = pio_claim_unused_sm(bus_pio, false);
    if (sm_read == PICO_ERROR_GENERIC) {
        sm_read = 0xff;
        DBGLN("PIO: No free SM for read");
        return;
    }

    /* Load read program */
    offset_read = pio_add_program(bus_pio, &data_bus_read_program);

    /* Configure read SM:
     * - IN base: GP1 (data bus)
     * - JMP pin: GP11 (CS) - wait for CS low
     * - Wait pin: GP11 (CS) - check read direction
     */
    pio_sm_config c = data_bus_read_program_get_default_config(offset_read);
    sm_config_set_in_pins(&c, WD_DATA_BASE);    /* IN base: GP1 */
    sm_config_set_jmp_pin(&c, WD_CS_PIN);       /* JMP pin: GP11 */
    sm_config_set_in_shift(&c, false, false, 8); /* Right shift, no autopush, 8 bits */
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    /* Init SM */
    pio_sm_init(bus_pio, sm_read, 0, &c);

    /* Set GPIO base for GP1-GP8 access */
    pio_sm_set_consecutive_pindirs(bus_pio, sm_read, WD_DATA_BASE, WD_DATA_BITS, false);

    DBGLN("PIO: Bus read SM initialized");
}

void pio_bus_start_read(uint8_t *dest) {
    if (sm_read == 0xff) return;

    /* Clear FIFO and restart SM */
    pio_sm_clear_fifos(bus_pio, sm_read);
    pio_sm_restart(bus_pio, sm_read);
    pio_sm_exec(bus_pio, sm_read, pio_encode_jmp(offset_read));
    pio_sm_set_enabled(bus_pio, sm_read, true);

    /* Wait for data (with timeout) */
    uint32_t timeout = 100000;
    while (pio_sm_is_rx_fifo_empty(bus_pio, sm_read) && timeout--) {
        tight_loop_contents();
    }

    if (timeout > 0) {
        *dest = (uint8_t)pio_sm_get(bus_pio, sm_read);
    } else {
        *dest = 0xff;
    }

    pio_sm_set_enabled(bus_pio, sm_read, false);
}

void pio_bus_start_write(uint8_t data) {
    /* Write via GPIO (PIO write is more complex, GPIO is sufficient) */
    bus_write_data(data);
}
