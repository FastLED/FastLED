#ifndef _PIO_GEN_H
#define _PIO_GEN_H

#include "pio_asm.h"
#include "hardware/pio.h"

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

static inline int add_clockless_pio_program(PIO pio, int T1, int T2, int T3) {
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
    };
    
    if (!pio_can_add_program(pio, &clockless_pio_program))
        return -1;
    
    return (int)pio_add_program(pio, &clockless_pio_program);
}

static inline pio_sm_config clockless_pio_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + CLOCKLESS_PIO_WRAP_TARGET, offset + CLOCKLESS_PIO_WRAP);
    sm_config_set_sideset(&c, CLOCKLESS_PIO_SIDESET_COUNT, false, false);
    return c;
}

#endif // _PIO_GEN_H
