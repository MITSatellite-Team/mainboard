#include <Arduino.h>
#include "hardware/structs/sio.h"
#include "pins.h"

#define PIN_MISO  PIN_ADC_MISO
#define PIN_CS    PIN_ADC_CONVST
#define PIN_SCLK  PIN_ADC_SCLK

void ads_init() {
    pinMode(PIN_SCLK, OUTPUT);
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_MISO, INPUT);
    digitalWrite(PIN_SCLK, LOW);
    digitalWrite(PIN_CS, LOW);
}

uint16_t ads_read() {
    sio_hw->gpio_clr = (1u << PIN_SCLK);
    delayMicroseconds(2);
    sio_hw->gpio_set = (1u << PIN_CS);   // CONVST HIGH
    delayMicroseconds(9);
    sio_hw->gpio_clr = (1u << PIN_CS);   // CONVST LOW
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

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("Part 1c: ported old ADC code");
    ads_init();
}

void loop() {
    uint16_t result = ads_read();

    Serial.print("Raw: ");
    Serial.print(result);
    Serial.print("  (0x");
    Serial.print(result, HEX);
    Serial.print(")  bits: ");
    for (int i = 15; i >= 0; i--) {
        Serial.print((result >> i) & 1);
    }
    Serial.println();

    delay(500);
}