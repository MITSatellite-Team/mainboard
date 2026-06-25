#pragma once
#include <Arduino.h>

#define ADS_NUM_PIXELS 1024

void ads_init();
void ads_trigger();              // call on each PHI2 rising edge
uint16_t* ads_get_buffer();      // call after EOS
void ads_reset();                // call before each scan
bool ads_is_complete();          // true when buffer is full
uint16_t ads_get_index();