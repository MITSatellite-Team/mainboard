#include "ads8866.h"
#include "hardware/pio.h"

#define PIN_MISO  8
#define PIN_CS    9
#define PIN_SCLK  10

void ads_init() {
    pio_sm_set_enabled(pio0, 2, false);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_set_pins(&c, PIN_CS, 1);
    pio_gpio_init(pio0, PIN_CS);
    pio_sm_set_consecutive_pindirs(pio0, 2, PIN_CS, 1, true);
    pio_sm_init(pio0, 2, 0, &c);
    pio_sm_set_enabled(pio0, 2, true);

    gpio_init(PIN_SCLK);
    gpio_set_dir(PIN_SCLK, GPIO_OUT);
    gpio_put(PIN_SCLK, false);

    gpio_init(PIN_MISO);
    gpio_set_dir(PIN_MISO, GPIO_IN);
}

uint16_t ads_read() {
    sio_hw->gpio_clr = (1u << PIN_SCLK);
    delayMicroseconds(2);           // let CCD OS settle after φ2 rising edge
    pio_sm_exec(pio0, 2, 0xe001);  // CONVST HIGH — sample taken now
    delayMicroseconds(9);  // must hold CONVST high ≥ tconv-max (8.8µs per datasheet)
    pio_sm_exec(pio0, 2, 0xe000);  // CONVST LOW
    __asm__ __volatile__("nop\nnop\nnop\nnop");

    uint16_t value = 0;
    for (int i = 15; i >= 0; i--) {
        sio_hw->gpio_set = (1u << PIN_SCLK);
        __asm__ __volatile__("nop\nnop");
        if (sio_hw->gpio_in & (1u << PIN_MISO)) value |= (1 << i);
        sio_hw->gpio_clr = (1u << PIN_SCLK);
        __asm__ __volatile__("nop\nnop");
    }
    return value;
}

float ads_to_volts(uint16_t raw) {
    return (raw / 65535.0f) * 5.0f;
}