#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TMP117.h>
#include "pins.h"

Adafruit_TMP117 tmp117;
MbedI2C myWire(PIN_SDA, PIN_SCL);

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    myWire.begin();

    if (!tmp117.begin(0x48, &myWire)) {
        Serial.println("FAIL: TMP117 not found");
        while (1);
    }
    Serial.println("PASS: TMP117 found");
}

// --- helpers ---

void printTemp() {
    sensors_event_t temp;
    tmp117.getEvent(&temp);
    Serial.print("Temp: ");
    Serial.print(temp.temperature);
    Serial.println(" C");
}

// --- tests ---

void test_read_temp() {
    Serial.println("\n-- test_read_temp --");
    sensors_event_t temp;
    tmp117.getEvent(&temp);
    if (temp.temperature > -40 && temp.temperature < 85) {
        Serial.println("PASS: temp in valid range");
        printTemp();
    } else {
        Serial.println("FAIL: temp out of range");
    }
}

void test_repeated_reads() {
    Serial.println("\n-- test_repeated_reads --");
    for (int i = 0; i < 5; i++) {
        printTemp();
        delay(500);
    }
    Serial.println("PASS: repeated reads done");
}

void loop() {
    test_read_temp();
    test_repeated_reads();

    Serial.println("\n-- done, looping in 5s --");
    delay(5000);
}