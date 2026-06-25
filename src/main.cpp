#include <Arduino.h>
#include "ccd.h"
#include "ads8866.h"
#include "hardware/structs/sio.h"
#include "pins.h"

void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("=== CCD scan test ===");
    ads_init();
    ccd_init();
}

unsigned long EXPOSURE_MS = 0.1;  // change this to control exposure

void loop() {
    Serial.println("starting scan...");
    ads_reset();
    ccd_start_scan();

    // poll PHI2 falling edge
    while (!ccd_eos_fired()) {
        while (!(sio_hw->gpio_in & (1u << PIN_PSI1)) && !ccd_eos_fired()) {}
        if (ccd_eos_fired()) break;
        while ((sio_hw->gpio_in & (1u << PIN_PSI1)) && !ccd_eos_fired()) {}
        if (ccd_eos_fired()) break;
        ads_trigger();
    }

    // print data
    uint16_t* buf = ads_get_buffer();
    Serial.print("data:");
    for (int i = 0; i < ADS_NUM_PIXELS; i++) {
        Serial.print(buf[i]);
        if (i < ADS_NUM_PIXELS - 1) Serial.print(",");
    }
    Serial.println();

    ccd_stop();
    delay(EXPOSURE_MS);  // this IS the exposure time
}