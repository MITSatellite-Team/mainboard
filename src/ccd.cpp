#include "ccd.h"
#include "ccd_driver.pio.h"

#define PIN_PHIST 4
#define PIN_PHI1  2
#define PIN_PHI2  3
#define PIN_EOS   5

static PIO pio = pio0;
static uint sm_phist = 0;
static uint sm_phi   = 1;
static uint offset_phist;
static uint offset_phi;
static volatile bool eos_fired = false;

static void eos_isr() {
    eos_fired = true;
}

static void phi_init() {
    pio_sm_config c = phi_square_program_get_default_config(offset_phi);
    sm_config_set_set_pins(&c, PIN_PHI1, 2);
    sm_config_set_clkdiv(&c, 6.0f);  // ~1ms total scan
    pio_sm_init(pio, sm_phi, offset_phi, &c);
    pio_sm_set_enabled(pio, sm_phi, true);
    pio_sm_exec(pio, sm_phi, 0xe002);  // set pins, 0b10 → φ2 HIGH idle
}

static void stop_phi() {
    pio_sm_set_enabled(pio, sm_phi, false);
    pio_sm_restart(pio, sm_phi);
    sio_hw->gpio_clr = (1u << PIN_PHI1);
    sio_hw->gpio_set = (1u << PIN_PHI2);
    phi_init();
}

void ccd_init() {
    pinMode(PIN_EOS, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_EOS), eos_isr, FALLING);

    offset_phist = pio_add_program(pio, &phist_pulse_program);
    pio_sm_config c_phist = phist_pulse_program_get_default_config(offset_phist);
    sm_config_set_set_pins(&c_phist, PIN_PHIST, 1);
    sm_config_set_clkdiv(&c_phist, 1.0f);
    pio_gpio_init(pio, PIN_PHIST);
    pio_sm_set_consecutive_pindirs(pio, sm_phist, PIN_PHIST, 1, true);
    pio_sm_init(pio, sm_phist, offset_phist, &c_phist);
    pio_sm_set_enabled(pio, sm_phist, true);

    offset_phi = pio_add_program(pio, &phi_square_program);
    pio_gpio_init(pio, PIN_PHI1);
    pio_gpio_init(pio, PIN_PHI2);
    pio_sm_set_consecutive_pindirs(pio, sm_phi, PIN_PHI1, 2, true);
    phi_init();
}

void ccd_start_scan() {
    eos_fired = false;
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
}

bool ccd_wait_for_end() {
    unsigned long start = millis();
    while (!eos_fired) {
        if (millis() - start > 50) {  // 1ms scan + 49ms buffer
            stop_phi();
            return false;
        }
    }
    return true;
}

void ccd_stop() {
    stop_phi();
}