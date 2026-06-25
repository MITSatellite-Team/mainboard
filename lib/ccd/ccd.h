#pragma once
#include <Arduino.h>

void ccd_init();
void ccd_start_scan();
bool ccd_wait_for_end();
void ccd_stop();
bool ccd_eos_fired();
uint32_t ccd_get_pixel_count();
uint32_t ccd_get_scan_time_us();