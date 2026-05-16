#include <Arduino.h>
#include <SdFat.h>
#include <SPI.h>
#include "hardware/pio.h"
#include "ccd_driver.pio.h"
#include "ads8866.h"

#define PIN_PHIST   4
#define PIN_PHI1    2
#define PIN_PHI2    3
#define PIN_EOS     5
#define PIN_SD_CS   17
#define PIN_SD_MISO 16
#define PIN_SD_SCLK 18
#define PIN_SD_MOSI 19
#define NUM_PIXELS  1024
#define STACK_COUNT 1
#define SIGNAL_GAIN 20
#define CLKDIV      2.0f
#define TIMEOUT_MS  500
#define SCAN_INTERVAL_MS 1000UL

#define SD_SPI_SPEED SD_SCK_MHZ(10)  // lower to 4 if you get CRC errors

PIO pio = pio0;
uint sm_phist = 0;
uint sm_phi   = 1;
uint offset_phist;
uint offset_phi;

volatile bool eos_fired = false;
uint16_t pixel_data[NUM_PIXELS];
uint32_t stack_accum[NUM_PIXELS];
uint16_t stack_result[NUM_PIXELS];

SdFat  sd;
SdFile file;
bool     sd_ready     = false;
uint32_t scan_index   = 0;
unsigned long last_scan_ms = 0;

// ===== CCD =====
void eos_isr() { eos_fired = true; }

void phi_init() {
    pio_sm_config c = phi_square_program_get_default_config(offset_phi);
    sm_config_set_set_pins(&c, PIN_PHI1, 2);
    sm_config_set_clkdiv(&c, CLKDIV);
    pio_sm_init(pio, sm_phi, offset_phi, &c);
    pio_sm_set_enabled(pio, sm_phi, true);
    pio_sm_exec(pio, sm_phi, 0xe02a);
}

void stop_phi() {
    pio_sm_set_enabled(pio, sm_phi, false);
    pio_sm_restart(pio, sm_phi);
    sio_hw->gpio_clr = (1u << PIN_PHI1);
    sio_hw->gpio_set = (1u << PIN_PHI2);
    phi_init();
}

int run_single_scan() {
    eos_fired = false;
    memset(pixel_data, 0, sizeof(pixel_data));

    pio_sm_set_enabled(pio, sm_phist, false);
    pio_sm_set_enabled(pio, sm_phi, false);
    pio_sm_restart(pio, sm_phist);
    pio_sm_restart(pio, sm_phi);
    pio_sm_clear_fifos(pio, sm_phist);
    pio_sm_clear_fifos(pio, sm_phi);
    pio_interrupt_clear(pio, 0);
    pio_sm_exec(pio, sm_phist, pio_encode_jmp(offset_phist));
    pio_sm_exec(pio, sm_phi, pio_encode_jmp(offset_phi));
    pio_sm_put(pio, sm_phist, 1);
    pio_enable_sm_mask_in_sync(pio, (1u << sm_phist) | (1u << sm_phi));

    int pixel = 0;
    unsigned long start = millis();
    while (!eos_fired) {
        if (millis() - start > TIMEOUT_MS) { stop_phi(); return -1; }
        while (!(sio_hw->gpio_in & (1u << PIN_PHI1)) && !eos_fired) {
            if (millis() - start > TIMEOUT_MS) { stop_phi(); return -1; }
        }
        while ((sio_hw->gpio_in & (1u << PIN_PHI1)) && !eos_fired) {
            if (millis() - start > TIMEOUT_MS) { stop_phi(); return -1; }
        }
        if (!eos_fired && pixel < NUM_PIXELS) {
            pixel_data[pixel++] = ads_read();
        }
    }
    stop_phi();
    return pixel;
}

// ===== SD =====
// Called only after all CCD/scan work is fully complete.
// Appends one CSV row: scan_index, p0, p1, ..., p1023
void sd_save_scan(uint16_t* data, int num_pixels) {
    if (!file.open("scans.csv", O_WRONLY | O_CREAT | O_AT_END)) {
        Serial.println("SD: failed to open scans.csv");
        return;
    }
    file.print(scan_index);
    for (int i = 0; i < num_pixels; i++) {
        file.print(',');
        file.print(data[i]);
    }
    file.println();
    file.close();
    Serial.print("SD: scan "); Serial.print(scan_index); Serial.println(" saved");
    scan_index++;
}

// ===== Setup =====
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Booting...");

    // SD — initialise before anything else so SPI pins are configured
    SPI.setRX(PIN_SD_MISO);
    SPI.setCS(PIN_SD_CS);
    SPI.setSCK(PIN_SD_SCLK);
    SPI.setTX(PIN_SD_MOSI);
    if (sd.begin(PIN_SD_CS, SD_SPI_SPEED)) {
        sd_ready = true;
        Serial.println("SD: ready");
    } else {
        Serial.println("SD: init failed — continuing without SD");
        sd.errorPrint(&Serial);
    }

    pinMode(PIN_EOS, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_EOS), eos_isr, FALLING);
    Serial.println("EOS interrupt attached");

    offset_phist = pio_add_program(pio, &phist_pulse_program);
    pio_sm_config c_phist = phist_pulse_program_get_default_config(offset_phist);
    sm_config_set_set_pins(&c_phist, PIN_PHIST, 1);
    sm_config_set_clkdiv(&c_phist, 1.0f);
    pio_gpio_init(pio, PIN_PHIST);
    pio_sm_set_consecutive_pindirs(pio, sm_phist, PIN_PHIST, 1, true);
    pio_sm_init(pio, sm_phist, offset_phist, &c_phist);
    pio_sm_set_enabled(pio, sm_phist, true);
    Serial.println("phist SM ready");

    offset_phi = pio_add_program(pio, &phi_square_program);
    pio_gpio_init(pio, PIN_PHI1);
    pio_gpio_init(pio, PIN_PHI2);
    pio_sm_set_consecutive_pindirs(pio, sm_phi, PIN_PHI1, 2, true);
    phi_init();
    Serial.println("phi SM ready");

    ads_init();
    Serial.println("ADC ready");

    // Fire first scan immediately on boot
    last_scan_ms = millis() - SCAN_INTERVAL_MS;
    Serial.println("Ready");
}

// ===== Loop =====
void loop() {
    if (millis() - last_scan_ms < SCAN_INTERVAL_MS) return;
    last_scan_ms = millis();

    // ── CCD acquisition (untouched) ──────────────────────────────────────
    memset(stack_accum, 0, sizeof(stack_accum));
    int good_scans = 0;

    Serial.print("stacking "); Serial.print(STACK_COUNT); Serial.println(" scans...");

    for (int s = 0; s < STACK_COUNT; s++) {
        int pixels = run_single_scan();
        if (pixels < 0) {
            Serial.print("scan "); Serial.print(s); Serial.println(": timeout");
        } else if (pixels >= 1000) {
            for (int i = 0; i < NUM_PIXELS; i++) stack_accum[i] += pixel_data[i];
            good_scans++;
            Serial.print("scan "); Serial.print(s);
            Serial.print(": EOS "); Serial.print(pixels); Serial.println(" px ok");
        } else {
            Serial.print("scan "); Serial.print(s);
            Serial.print(": EOS "); Serial.print(pixels); Serial.println(" px (short)");
        }
    }

    if (good_scans == 0) {
        Serial.println("no good scans");
        return;
    }

    int64_t bias_sum = 0;
    for (int i = 10; i < NUM_PIXELS - 10; i++)
        bias_sum += (int32_t)(stack_accum[i] / good_scans);
    int32_t scan_bias = (int32_t)(bias_sum / (NUM_PIXELS - 20));

    for (int i = 0; i < NUM_PIXELS; i++) {
        int32_t v = scan_bias - (int32_t)(stack_accum[i] / good_scans);
        v *= SIGNAL_GAIN;
        stack_result[i] = (uint16_t)constrain(v, 0, 65535);
    }

    // Print to serial
    Serial.print("stacked:"); Serial.print(good_scans); Serial.print(" ");
    for (int i = 0; i < NUM_PIXELS; i++) {
        Serial.print(stack_result[i]);
        if (i < NUM_PIXELS - 1) Serial.print(",");
    }
    Serial.println();

    // ── SD save — after all CCD work is done ────────────────────────────
    if (sd_ready) {
        sd_save_scan(stack_result, NUM_PIXELS);
    }
}