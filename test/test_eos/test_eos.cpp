#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/structs/sio.h"
#include "ccd_driver.pio.h"
#include "pins.h"

PIO pio = pio0;
uint sm_phist = 0;
uint sm_phi   = 1;
uint offset_phist;
uint offset_phi;

volatile uint32_t pixel_count = 0;
volatile bool eos_fired = false;
volatile uint32_t eos_pixel_count = 0;
unsigned long scan_start_ms = 0;

void eos_isr() {
    eos_fired = true;
    eos_pixel_count = pixel_count;
}

void phi2_isr() {
    pixel_count++;
}

void phi_init() {
    pio_sm_config c = phi_square_program_get_default_config(offset_phi);
    sm_config_set_set_pins(&c, PIN_PSI1, 2);
    sm_config_set_clkdiv(&c, 2.0f);
    pio_gpio_init(pio, PIN_PSI1);
    pio_gpio_init(pio, PIN_PSI2);
    pio_sm_set_consecutive_pindirs(pio, sm_phi, PIN_PSI1, 2, true);
    pio_sm_init(pio, sm_phi, offset_phi, &c);
    pio_sm_set_enabled(pio, sm_phi, true);
}

void phist_init() {
    pio_sm_config c = phist_pulse_program_get_default_config(offset_phist);
    sm_config_set_set_pins(&c, PIN_PSIST, 1);
    sm_config_set_clkdiv(&c, 1.0f);
    pio_gpio_init(pio, PIN_PSIST);
    pio_sm_set_consecutive_pindirs(pio, sm_phist, PIN_PSIST, 1, true);
    pio_sm_init(pio, sm_phist, offset_phist, &c);
    pio_sm_set_enabled(pio, sm_phist, true);
}

void start_scan() {
    pixel_count = 0;
    eos_fired = false;
    eos_pixel_count = 0;
    scan_start_ms = millis();
    pio_sm_put_blocking(pio, sm_phist, 1);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("Part 2: EOS test");

    // EOS interrupt
    pinMode(PIN_EOS, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_EOS), eos_isr, FALLING);

    // PIO IRQ1 → pixel counter
    // IRQ 1 from phi_square fires on every PHI2 rising edge
    irq_set_exclusive_handler(PIO0_IRQ_1, phi2_isr);
    irq_set_enabled(PIO0_IRQ_1, true);
    pio_set_irq1_source_enabled(pio, pis_interrupt1, true);

    // load PIO programs
    offset_phist = pio_add_program(pio, &phist_pulse_program);
    offset_phi   = pio_add_program(pio, &phi_square_program);

    phist_init();
    phi_init();

    Serial.println("PIO ready, starting scan...");
    start_scan();
}

void loop() {
    if (eos_fired) {
        unsigned long elapsed = millis() - scan_start_ms;

        Serial.println("--- scan complete ---");
        Serial.print("EOS fired after pixel count: ");
        Serial.println(eos_pixel_count);
        Serial.print("Elapsed time: ");
        Serial.print(elapsed);
        Serial.println("ms");
        Serial.print("Expected ~33.6ms, expected ~1024 pixels: ");

        if (eos_pixel_count >= 1020 && eos_pixel_count <= 1028) {
            Serial.println("PASS: pixel count correct");
        } else {
            Serial.print("FAIL: got ");
            Serial.print(eos_pixel_count);
            Serial.println(" pixels");
        }

        if (elapsed >= 30 && elapsed <= 40) {
            Serial.println("PASS: timing correct");
        } else {
            Serial.print("FAIL: elapsed ");
            Serial.print(elapsed);
            Serial.println("ms");
        }

        Serial.println("Starting next scan in 2s...");
        delay(2000);
        start_scan();
    }
}