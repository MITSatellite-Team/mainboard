#include "ccd.h"
#include "ccd_driver.pio.h"
#include "ads8866.h"
#include "pins.h"
#include "hardware/structs/sio.h"
#include "hardware/irq.h"

static PIO pio = pio0;
static uint sm_phist = 0;
static uint sm_phi   = 1;
static uint offset_phist;
static uint offset_phi;

static volatile bool     eos_fired           = false;
static volatile uint32_t _pixel_count        = 0;
static volatile uint32_t _pixel_count_at_eos = 0;
static uint32_t          _scan_start_us      = 0;
static uint32_t          _scan_end_us        = 0;

static void eos_isr() {
    _pixel_count_at_eos = _pixel_count;
    eos_fired           = true;
    _scan_end_us        = time_us_32();
}

static void phi2_isr() {
    _pixel_count++;
    pio_interrupt_clear(pio, 1);
}

static void phi_init() {
    pio_sm_config c = phi_square_program_get_default_config(offset_phi);
    sm_config_set_set_pins(&c, PIN_PSI1, 2);
    sm_config_set_clkdiv(&c, 8.0f);  // changed from 4.0f — doubles integration time
    pio_sm_init(pio, sm_phi, offset_phi, &c);
    pio_sm_set_enabled(pio, sm_phi, true);
    pio_sm_exec(pio, sm_phi, 0xe002);
}

static void stop_phi() {
    pio_sm_set_enabled(pio, sm_phi, false);
    pio_sm_restart(pio, sm_phi);
    sio_hw->gpio_clr = (1u << PIN_PSI1);
    sio_hw->gpio_set = (1u << PIN_PSI2);
    phi_init();
}

void ccd_init() {
    pinMode(PIN_EOS, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_EOS), eos_isr, FALLING);

    irq_set_exclusive_handler(PIO0_IRQ_1, phi2_isr);
    irq_set_enabled(PIO0_IRQ_1, true);
    pio_set_irq1_source_enabled(pio, pis_interrupt1, true);

    irq_set_priority(PIO0_IRQ_1, 0);
    irq_set_priority(IO_IRQ_BANK0, 1);

    offset_phist = pio_add_program(pio, &phist_pulse_program);
    pio_sm_config c_phist = phist_pulse_program_get_default_config(offset_phist);
    sm_config_set_set_pins(&c_phist, PIN_PSIST, 1);
    sm_config_set_clkdiv(&c_phist, 1.0f);
    pio_gpio_init(pio, PIN_PSIST);
    pio_sm_set_consecutive_pindirs(pio, sm_phist, PIN_PSIST, 1, true);
    pio_sm_init(pio, sm_phist, offset_phist, &c_phist);
    pio_sm_set_enabled(pio, sm_phist, true);

    offset_phi = pio_add_program(pio, &phi_square_program);
    pio_gpio_init(pio, PIN_PSI1);
    pio_gpio_init(pio, PIN_PSI2);
    pio_sm_set_consecutive_pindirs(pio, sm_phi, PIN_PSI1, 2, true);
    phi_init();
}

void ccd_start_scan() {
    eos_fired           = false;
    _pixel_count        = 0;
    _pixel_count_at_eos = 0;

    ads_reset();

    irq_set_enabled(PIO0_IRQ_1, false);

    pio_sm_set_enabled(pio, sm_phist, false);
    pio_sm_set_enabled(pio, sm_phi,   false);
    pio_sm_restart(pio, sm_phist);
    pio_sm_restart(pio, sm_phi);
    pio_sm_clear_fifos(pio, sm_phist);
    pio_sm_clear_fifos(pio, sm_phi);
    pio_interrupt_clear(pio, 0);
    pio_interrupt_clear(pio, 1);

    sio_hw->gpio_clr = (1u << PIN_PSI1);
    sio_hw->gpio_set = (1u << PIN_PSI2);

    pio_sm_exec(pio, sm_phist, pio_encode_jmp(offset_phist));
    pio_sm_exec(pio, sm_phi,   pio_encode_jmp(offset_phi));

    _scan_start_us = time_us_32();

    irq_set_enabled(PIO0_IRQ_1, true);

    pio_sm_put(pio, sm_phist, 1);
    pio_enable_sm_mask_in_sync(pio, (1u << sm_phist) | (1u << sm_phi));
}

bool ccd_wait_for_end() {
    unsigned long start = millis();
    while (!eos_fired) {
        if (millis() - start > 500) {
            stop_phi();
            return false;
        }
    }
    return true;
}

void ccd_stop() {
    irq_set_enabled(PIO0_IRQ_1, false);
    stop_phi();
}

uint32_t ccd_get_pixel_count()  { return _pixel_count_at_eos; }
uint32_t ccd_get_scan_time_us() { return _scan_end_us - _scan_start_us; }
bool ccd_eos_fired() { return eos_fired; }