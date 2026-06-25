#pragma once
#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

// phist_pulse
#define phist_pulse_wrap_target 0
#define phist_pulse_wrap 17

static const uint16_t phist_pulse_program_instructions[] = {
    0x80a0, //  0: pull   block
    0xe001, //  1: set    pins, 1
    0xe029, //  2: set    x, 9
    0x0043, //  3: jmp    x--, 3
    0xc000, //  4: irq    nowait 0
    0xe05f, //  5: set    y, 31
    0xe03f, //  6: set    x, 31
    0x0047, //  7: jmp    x--, 7
    0x0086, //  8: jmp    y--, 6
    0xe05f, //  9: set    y, 31
    0xe03f, // 10: set    x, 31
    0x004b, // 11: jmp    x--, 11
    0x008a, // 12: jmp    y--, 10
    0xe05f, // 13: set    y, 31
    0xe03f, // 14: set    x, 31
    0x004f, // 15: jmp    x--, 15
    0x008e, // 16: jmp    y--, 14
    0xe000, // 17: set    pins, 0
};

#if !PICO_NO_HARDWARE
static const struct pio_program phist_pulse_program = {
    .instructions = phist_pulse_program_instructions,
    .length = 18,
    .origin = -1,
};
static inline pio_sm_config phist_pulse_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + phist_pulse_wrap_target, offset + phist_pulse_wrap);
    return c;
}
#endif

// phi_square
#define phi_square_wrap_target 0
#define phi_square_wrap 12

static const uint16_t phi_square_program_instructions[] = {
    0x20c0, //  0: wait   1 irq, 0
    0xe001, //  1: set    pins, 1
    0xe05f, //  2: set    y, 31
    0xe03f, //  3: set    x, 31
    0x0044, //  4: jmp    x--, 4
    0x0083, //  5: jmp    y--, 3
    0xe002, //  6: set    pins, 2
    0xc001, //  7: irq    nowait 1
    0xe05f, //  8: set    y, 31
    0xe03f, //  9: set    x, 31
    0x004a, // 10: jmp    x--, 10
    0x0089, // 11: jmp    y--, 9
    0x0001, // 12: jmp    1
};

#if !PICO_NO_HARDWARE
static const struct pio_program phi_square_program = {
    .instructions = phi_square_program_instructions,
    .length = 13,
    .origin = -1,
};
static inline pio_sm_config phi_square_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + phi_square_wrap_target, offset + phi_square_wrap);
    return c;
}
#endif