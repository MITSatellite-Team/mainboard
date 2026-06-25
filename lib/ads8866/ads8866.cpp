#include "ads8866.h"
#include "pins.h"
#include "hardware/structs/sio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

static uint16_t _buf[ADS_NUM_PIXELS];
static volatile uint16_t _idx       = 0;
static volatile bool     _complete  = false;

// --- low level read ---

static inline uint16_t _read_adc() {
    // settle ~200ns
    __asm__ __volatile__("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop");

    // CONVST high — start conversion
    sio_hw->gpio_set = (1u << PIN_ADC_CONVST);

    // wait tconv = 8.8us — burn with NOPs at 125MHz
    // 8800ns / 8ns = 1100 cycles
    for (volatile int i = 0; i < 275; i++) {
        __asm__ __volatile__("nop\nnop\nnop\nnop");
    }

    // CONVST low — data ready
    sio_hw->gpio_clr = (1u << PIN_ADC_CONVST);

    // td-CNV-DO delay (~13ns = 2 cycles)
    __asm__ __volatile__("nop\nnop");

    // clock out 16 bits MSB first
    uint16_t value = 0;
    for (int i = 15; i >= 0; i--) {
        sio_hw->gpio_set = (1u << PIN_ADC_SCLK);
        __asm__ __volatile__("nop\nnop");
        if (sio_hw->gpio_in & (1u << PIN_ADC_MISO)) value |= (1 << i);
        sio_hw->gpio_clr = (1u << PIN_ADC_SCLK);
        __asm__ __volatile__("nop\nnop");
    }
    return value;
}

// --- public API ---

void ads_init() {
    pinMode(PIN_ADC_SCLK,   OUTPUT);
    pinMode(PIN_ADC_CONVST, OUTPUT);
    pinMode(PIN_ADC_MISO,   INPUT);
    digitalWrite(PIN_ADC_SCLK,   LOW);
    digitalWrite(PIN_ADC_CONVST, LOW);
}

void ads_reset() {
    _idx      = 0;
    _complete = false;
    memset(_buf, 0, sizeof(_buf));
}

void ads_trigger() {
    if (_complete) return;
    if (_idx >= ADS_NUM_PIXELS) {
        _complete = true;
        return;
    }
    _buf[_idx++] = _read_adc();
}

uint16_t* ads_get_buffer() {
    return _buf;
}

bool ads_is_complete() {
    return _complete;
}
uint16_t ads_get_index() { return _idx; }