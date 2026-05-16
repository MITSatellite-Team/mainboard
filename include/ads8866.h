#pragma once
#include <Arduino.h>

void ads_init();
uint16_t ads_read();
float ads_to_volts(uint16_t raw);