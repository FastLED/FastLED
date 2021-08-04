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
 * 
 * 
 * The PIO program is copied from pico-examples, which uses a 3-clause BSD license:
 * 
 * Copyright 2020 (c) 2020 Raspberry Pi (Trading) Ltd.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 *    disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define CLOCKLESS_PIO_SIDESET_COUNT 1

#define CLOCKLESS_PIO_BITLOOP_ADR 0
#define CLOCKLESS_PIO_DOZERO_ADR 3

#define CLOCKLESS_PIO_WRAP_TARGET 0
#define CLOCKLESS_PIO_WRAP 3

// we have 4 bits to store delay in instruction encoding with one sideset bit, but we can accept up to 16 because 1 is always subtracted first
#define CLOCKLESS_PIO_MAX_TIME_PERIOD (1 << (5 - CLOCKLESS_PIO_SIDESET_COUNT))

static inline int add_clockless_pio_program(PIO pio, int T1, int T2, int T3) {
    pio_instr clockless_pio_instr[] = {
        // wrap_target
        // bitloop
        (pio_instr)(PIO_INSTR_OUT | PIO_OUT_DST_X | PIO_OUT_CNT(1) | PIO_SIDESET(0, CLOCKLESS_PIO_SIDESET_COUNT) | PIO_DELAY(T3 - 1, CLOCKLESS_PIO_SIDESET_COUNT)),
        (pio_instr)(PIO_INSTR_JMP | PIO_JMP_CND_NOT_X | PIO_JMP_ADR(CLOCKLESS_PIO_DOZERO_ADR) | PIO_SIDESET(1, CLOCKLESS_PIO_SIDESET_COUNT) | PIO_DELAY(T1 - 1, CLOCKLESS_PIO_SIDESET_COUNT)),
        // do_one
        (pio_instr)(PIO_INSTR_JMP | PIO_JMP_CND_ALWAYS | PIO_JMP_ADR(CLOCKLESS_PIO_BITLOOP_ADR) | PIO_SIDESET(1, CLOCKLESS_PIO_SIDESET_COUNT) | PIO_DELAY(T2 - 1, CLOCKLESS_PIO_SIDESET_COUNT)),
        // do_zero
        (pio_instr)(PIO_NOP | PIO_SIDESET(0, CLOCKLESS_PIO_SIDESET_COUNT) | PIO_DELAY(T2 - 1, CLOCKLESS_PIO_SIDESET_COUNT)),
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
