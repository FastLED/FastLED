// IWYU pragma: private

// ok no namespace fl
#ifndef _PIO_GEN_H
#define _PIO_GEN_H

#include "platforms/arm/rp/rpcommon/pio_asm.h"
// IWYU pragma: begin_keep
#include "hardware/pio.h"
#include "fl/stl/noexcept.h"

// IWYU pragma: end_keep
/*
 * This file contains code to manage the PIO program for clockless LEDs.
 * 
 * A PIO program is "assembled" from compiler macros so that T1, T2, T3 can be
 * set from other code.
 * Otherwise, this is quite similar to what would be output by pioasm, with the
 * additional step of adding the program to a state machine integrated.
 */

#define CLOCKLESS_PIO_SIDESET_COUNT 0

#define CLOCKLESS_PIO_WRAP_TARGET 0
#define CLOCKLESS_PIO_WRAP 3

// we have 4 bits to store delay in instruction encoding with one sideset bit, but we can accept up to 16 because 1 is always subtracted first
#define CLOCKLESS_PIO_MAX_TIME_PERIOD (1 << (5 - CLOCKLESS_PIO_SIDESET_COUNT))

static inline int add_clockless_pio_program(PIO pio, int T1, int T2, int T3) FL_NOEXCEPT {
    pio_instr clockless_pio_instr[] = {
        // wrap_target
        // out x, 1; read next bit to x
        (pio_instr)(PIO_INSTR_OUT | PIO_OUT_DST_X | PIO_OUT_CNT(1)),
        // set pins, 1 [T1 - 1]; set output high for T1
        (pio_instr)(PIO_INSTR_SET | PIO_SET_DST_PINS | PIO_SET_DATA(1) | PIO_DELAY(T1 - 1, CLOCKLESS_PIO_SIDESET_COUNT)),
        // mov pins, x [T2 - 1]; set output to X for T2
        (pio_instr)(PIO_INSTR_MOV | PIO_MOV_DST_PINS | PIO_MOV_SRC_X | PIO_DELAY(T2 - 1, CLOCKLESS_PIO_SIDESET_COUNT)),
        // set pins, 0 [T3 - 2] // set output low for T3 (minus two because we'll also read next bit using one instruction during this time)
        (pio_instr)(PIO_INSTR_SET | PIO_SET_DST_PINS | PIO_SET_DATA(0) | PIO_DELAY(T3 - 2, CLOCKLESS_PIO_SIDESET_COUNT)),
        // wrap
    };
    
    struct pio_program clockless_pio_program = {
        .instructions = clockless_pio_instr,
        .length = sizeof(clockless_pio_instr) / sizeof(clockless_pio_instr[0]),
        .origin = -1,
#if defined(PICO_SDK_VERSION_MAJOR) && PICO_SDK_VERSION_MAJOR >= 2
        // pico-sdk 2.x added these two fields to `pio_program`. They are
        // gated by *different* macros in the SDK header — getting the gating
        // wrong here is what regressed RP2040 builds in #2792 / #2727.
        //
        //   * `pio_version` is unconditionally present in 2.x — guard on
        //     PICO_SDK_VERSION_MAJOR only.
        //   * `used_gpio_ranges` is per-chip, gated on PICO_PIO_VERSION > 0
        //     in rp2_common/hardware_pio/include/hardware/pio.h. RP2350
        //     defines `PICO_PIO_VERSION = 1` so the field exists; RP2040
        //     defines `PICO_PIO_VERSION = 0` so the field is genuinely
        //     absent even on the 2.x SDK.
        //
        // Zero-init preserves the previous implicit behavior while silencing
        // -Wmissing-field-initializers and documenting intent: no specific
        // PIO version pinned, no GPIO range pre-claimed. Revisit if the SDK
        // starts enforcing `used_gpio_ranges` at pio_add_program time.
        .pio_version = 0,
    #if defined(PICO_PIO_VERSION) && PICO_PIO_VERSION > 0
        .used_gpio_ranges = 0,
    #endif
#endif
    };
    
    if (!pio_can_add_program(pio, &clockless_pio_program))
        return -1;
    
    return (int)pio_add_program(pio, &clockless_pio_program);
}

static inline pio_sm_config clockless_pio_program_get_default_config(uint offset) FL_NOEXCEPT {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + CLOCKLESS_PIO_WRAP_TARGET, offset + CLOCKLESS_PIO_WRAP);
    sm_config_set_sideset(&c, CLOCKLESS_PIO_SIDESET_COUNT, false, false);
    return c;
}

#endif // _PIO_GEN_H
