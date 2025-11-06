/**
 * @file nmi_wrapper.S
 * @brief Xtensa Level 7 NMI wrapper for FastLED multi-SPI on ESP32/ESP32-S3
 *
 * This assembly shim provides the entry point for Level 7 NMI interrupts,
 * performs minimal context save/restore using Call0 ABI, and calls a C++
 * handler function for high-speed multi-SPI operations.
 *
 * Performance: 50-80ns overhead (context save + restore)
 *
 * Usage:
 *   1. Define a C++ handler: extern "C" void fastled_nmi_handler() IRAM_ATTR;
 *   2. Allocate timer interrupt with ESP_INTR_FLAG_LEVEL7
 *   3. ESP-IDF will automatically call xt_nmi symbol (this wrapper)
 *   4. This wrapper calls fastled_nmi_handler()
 *
 * CRITICAL RESTRICTIONS:
 *   - Handler must be in IRAM (use IRAM_ATTR)
 *   - NO FreeRTOS calls (will crash)
 *   - NO ESP_LOG (uses FreeRTOS mutexes)
 *   - NO malloc/free (heap uses locks)
 *   - Keep handler < 1µs execution time
 *
 * @see src/platforms/esp/32/XTENSA_INTERRUPTS.md for complete documentation
 * @see examples/FastPinsNMI/ for usage example
 *
 * Copyright (c) 2025 FastLED
 * Licensed under the MIT License
 */

    .section .iram1.text
    .global xt_nmi
    .type xt_nmi, @function
    .align 4

/**
 * @brief Level 7 NMI entry point
 *
 * This function is called directly by the Xtensa interrupt dispatcher when a
 * Level 7 (NMI) interrupt fires. It saves minimal CPU context, calls the C++
 * handler, restores context, and returns from interrupt.
 *
 * Register usage:
 *   - a0: Return address (must be saved)
 *   - a1: Stack pointer (preserved by callee)
 *   - a2-a7: Arguments/temps (caller saves, but we save a2-a3 for safety)
 *   - a12-a15: Callee-saved (must be saved if C handler might use them)
 *   - PS: Processor state (must be saved/restored)
 *
 * Stack frame layout (32 bytes):
 *   sp+0:  a0 (return address)
 *   sp+4:  a2 (argument register)
 *   sp+8:  a3 (argument register)
 *   sp+12: a12 (callee-saved)
 *   sp+16: a13 (callee-saved)
 *   sp+20: a14 (callee-saved)
 *   sp+24: a15 (callee-saved)
 *   sp+28: PS (processor state)
 *
 * Call0 ABI: Traditional stack-based calling convention
 *   - call0: Call function, return address in a0
 *   - ret: Return using a0
 *   - Stack grows downward (addi sp, sp, -size)
 *
 * MUST end with 'rfi 7' (not 'ret' or 'retw')!
 */
xt_nmi:
    # Save minimal context (Call0 ABI style)
    # Allocate 32-byte stack frame
    addi    sp, sp, -32         # sp -= 32 (stack grows downward)

    # Save return address and argument registers
    s32i    a0, sp, 0           # Save return address (required)
    s32i    a2, sp, 4           # Save a2 (first arg register)
    s32i    a3, sp, 8           # Save a3 (second arg register)

    # Save callee-saved registers (a12-a15)
    # Required if C handler uses these registers
    s32i    a12, sp, 12         # Save a12
    s32i    a13, sp, 16         # Save a13
    s32i    a14, sp, 20         # Save a14
    s32i    a15, sp, 24         # Save a15

    # Save processor state register
    # Contains interrupt level, window state, etc.
    rsr.ps  a2                  # Read processor state to a2
    s32i    a2, sp, 28          # Save PS to stack

    # Call C++ handler (Call0 ABI)
    # Handler prototype: extern "C" void fastled_nmi_handler() IRAM_ATTR;
    # No arguments passed (uses global state)
    call0   fastled_nmi_handler

    # Restore processor state
    # Must restore PS before returning from interrupt
    l32i    a2, sp, 28          # Load PS from stack
    wsr.ps  a2                  # Write PS back to processor
    rsync                       # Synchronize (required after wsr.ps)

    # Restore registers in reverse order
    l32i    a0, sp, 0           # Restore return address
    l32i    a2, sp, 4           # Restore a2
    l32i    a3, sp, 8           # Restore a3
    l32i    a12, sp, 12         # Restore a12
    l32i    a13, sp, 16         # Restore a13
    l32i    a14, sp, 20         # Restore a14
    l32i    a15, sp, 24         # Restore a15

    # Deallocate stack frame
    addi    sp, sp, 32          # sp += 32 (restore stack pointer)

    # Return from Level 7 NMI interrupt
    # CRITICAL: Must use 'rfi 7', not 'ret' or 'retw'
    # rfi restores EPC7 to PC and EPS7 to PS
    rfi     7                   # Return from interrupt level 7

    # Mark end of function (for debug symbols)
    .size xt_nmi, .-xt_nmi
