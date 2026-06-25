#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include "pins.h"

RTC_DS3231 rtc;
MbedI2C myWire(PIN_SDA, PIN_SCL);

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    myWire.begin();

    if (!rtc.begin(&myWire)) {
        Serial.println("FAIL: RTC not found");
        while (1);
    }
    Serial.println("PASS: RTC found");
}

// --- helpers ---

void printTime() {
    DateTime now = rtc.now();
    Serial.print(now.year());   Serial.print('/');
    Serial.print(now.month());  Serial.print('/');
    Serial.print(now.day());    Serial.print(' ');
    Serial.print(now.hour());   Serial.print(':');
    Serial.print(now.minute()); Serial.print(':');
    Serial.println(now.second());
}

void setTime(int y, int mo, int d, int h, int mi, int s) {
    rtc.adjust(DateTime(y, mo, d, h, mi, s));
    Serial.print("Time set to: ");
    printTime();
}

// --- tests ---

void test_get_time() {
    Serial.println("\n-- test_get_time --");
    DateTime now = rtc.now();
    if (now.year() >= 2024) {
        Serial.println("PASS: got valid time");
        printTime();
    } else {
        Serial.println("FAIL: time looks wrong");
    }
}

void test_set_time() {
    Serial.println("\n-- test_set_time --");
    setTime(2025, 1, 15, 12, 0, 0);
    delay(100);
    DateTime now = rtc.now();
    if (now.year() == 2025 && now.month() == 1 && now.day() == 15) {
        Serial.println("PASS: time set correctly");
    } else {
        Serial.println("FAIL: time mismatch after set");
    }
}

void test_compile_time() {
    Serial.println("\n-- test_compile_time --");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.print("Set to compile time: ");
    printTime();
    Serial.println("PASS: compile time set");
}

void test_lost_power() {
    Serial.println("\n-- test_lost_power --");
    if (rtc.lostPower()) {
        Serial.println("WARN: RTC lost power, resetting to compile time");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    } else {
        Serial.println("PASS: RTC has not lost power");
    }
}

void loop() {
    test_lost_power();
    test_get_time();
    test_set_time();
    test_compile_time();

    Serial.println("\n-- done, looping in 5s --");
    delay(5000);
}