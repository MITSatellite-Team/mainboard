#pragma once
#include <Arduino.h>
#include "hardware/pio.h"

void ccd_init();
void ccd_start_scan();
bool ccd_wait_for_end();
void ccd_stop();