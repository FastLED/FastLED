#pragma once

// Assembly Shims for High-Priority Interrupts on ESP32 (Xtensa LX6)
//
// This file provides assembly interrupt service routine (ISR) shims for
// high-priority interrupts (level 4 and 5) on ESP32 Xtensa LX6 cores.
//
// IMPORTANT: High-priority interrupts (≥4) on Xtensa MUST be written in
// assembly language and cannot use most C/RTOS facilities.
//
// TODO: This assembly implementation is UNTESTED and most certainly WRONG.
//       The assembly directives and implementation need to be researched and
//       validated for Xtensa LX6 architecture before use.

#include <stdint.h>
#include "soc/soc.h"
#include "esp_intr_alloc.h"
#include "hal/interrupt_coreasm.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// XTENSA LX6 ARCHITECTURE CONTEXT (ESP32)
//=============================================================================

/*
 * Xtensa LX6 Interrupt Architecture References:
 *
 * TODO: RESEARCH NEEDED - Verify these references are correct for LX6
 *
 * [1] Cadence Xtensa ISA Summary (High-Priority Interrupt Option):
 *     https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf
 *
 *     Section: "High-Priority Interrupt Option"
 *     Quote: "Each high-priority interrupt has dedicated EPC[i] and EPS[i]
 *            special registers and an EXCSAVE[i] special register."
 *
 *     Entry Sequence (takeinterrupt pseudo-code):
 *     EPC[level] ← PC
 *     EPS[level] ← PS
 *     PC ← InterruptVector[level]
 *     PS.INTLEVEL ← level
 *     PS.EXCM ← 1
 *
 *     Exit Sequence (RFI instruction):
 *     "RFI returns from a high-priority interrupt by restoring the processor
 *      state from the EPS[level] and EPC[level] special registers."
 *
 * [2] ESP-IDF High Priority Interrupts Guide:
 *     https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/hlinterrupts.html
 *
 *     Quote: "High priority interrupts must be written in assembly language"
 *     Quote: "Level 4 and 5 interrupts cannot call most C/RTOS facilities"
 *
 * [3] Xtensa Exception/Interrupt Masking:
 *     PS.EXCM=1 masks interrupts at level ≤ EXCMLEVEL (typically 3)
 *     CINTLEVEL is set to at least EXCMLEVEL when PS.EXCM=1
 *     NMI (level 6+) can still interrupt level 5 handlers
 */

//=============================================================================
// ESP32 INTERRUPT PRIORITY LEVELS
//=============================================================================

/*
 * ESP32 (Xtensa LX6) Interrupt Levels for FastLED:
 *
 * TODO: VERIFY - These level definitions may differ between LX6 and LX7
 *
 * Level 1-3: Standard priority - OFFICIALLY SUPPORTED by FastLED/RMT driver
 *            Level 3 is the MAXIMUM officially supported for RMT timing
 *            Can use C handlers with esp_intr_alloc()
 *            rmt_tx_channel_config_t.intr_priority supports 1-3 only
 *
 * Level 4-5: High priority - EXPERIMENTAL/CUSTOM implementation only
 *            Requires bypassing official RMT driver
 *            Must use assembly handlers and custom interrupt allocation
 *            Cannot call C/RTOS functions safely
 *            NOT supported by standard FastLED RMT integration
 *
 * Level 6+:  NMI - Non-maskable interrupts
 *            Reserved for watchdog, debug, etc.
 *
 * IMPORTANT: Official FastLED RMT driver uses rmt_tx_channel_config_t which
 * only accepts intr_priority values 1-3. Higher levels require custom
 * implementation that bypasses the driver.
 *
 * Reference: ESP-IDF RMT Driver docs showing priority limits
 * https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/rmt.html
 */

#define FASTLED_ESP32_MAX_OFFICIAL_LEVEL 3    // Maximum supported by RMT driver
#define FASTLED_ESP32_MAX_CUSTOM_LEVEL   5    // Maximum with custom implementation
#define FASTLED_ESP32_RECOMMENDED_LEVEL  3    // Recommended for FastLED

//=============================================================================
// REGISTER SAVE/RESTORE CONTEXT
//=============================================================================

/*
 * Xtensa Register Context for High-Priority ISRs:
 *
 * TODO: VERIFY - Register layout may differ between LX6 and LX7
 *
 * Hardware-saved (automatic):
 * - EPC[level]: Exception Program Counter (return address)
 * - EPS[level]: Exception Processor Status (PS register state)
 *
 * Software-saved (manual in assembly):
 * - A0-A15: General purpose address registers
 * - Special registers if used (SAR, LCOUNT, etc.)
 *
 * Stack alignment: 16-byte boundary required for proper operation
 *
 * Reference: Xtensa ISA Manual, Register File Organization
 */

// Minimum stack frame size for high-priority ISR (16-byte aligned)
#define FASTLED_ISR_STACK_FRAME_SIZE 64

// Register save area offsets (in bytes from stack pointer)
#define FASTLED_ISR_A0_OFFSET   0
#define FASTLED_ISR_A1_OFFSET   4
#define FASTLED_ISR_A2_OFFSET   8
#define FASTLED_ISR_A3_OFFSET   12
#define FASTLED_ISR_A4_OFFSET   16
#define FASTLED_ISR_A5_OFFSET   20
#define FASTLED_ISR_A6_OFFSET   24
#define FASTLED_ISR_A7_OFFSET   28
// ... continue for additional registers as needed

//=============================================================================
// ASSEMBLY ISR SHIM DECLARATIONS
//=============================================================================

/*
 * Level 4 Interrupt Shim (EXPERIMENTAL)
 *
 * TODO: ASSEMBLY IMPLEMENTATION NEEDED - This is currently placeholder only
 *
 * Entry point for level-4 interrupts. BYPASSES official RMT driver.
 * Must be installed at interrupt vector xt_highint4 using esp_intr_alloc()
 * with ESP_INTR_FLAG_LEVEL4, NOT through rmt_tx_channel_config_t.
 *
 * WARNING: This is NOT supported by official FastLED RMT integration.
 * Use only for experimental/custom implementations.
 *
 * Assembly implementation requirements:
 * 1. Save all used registers to stack (16-byte aligned)
 * 2. Call C handler function (marked IRAM_ATTR)
 * 3. Restore registers from stack
 * 4. Execute "rfi 4" to return from interrupt
 *
 * Example installation (CUSTOM, not standard FastLED):
 *   esp_intr_alloc(ETS_RMT_INTR_SOURCE,
 *                  ESP_INTR_FLAG_LEVEL4 | ESP_INTR_FLAG_IRAM,
 *                  xt_highint4, NULL, &handle);
 */
extern void xt_highint4(void);

/*
 * Level 5 Interrupt Shim (EXPERIMENTAL)
 *
 * TODO: ASSEMBLY IMPLEMENTATION NEEDED - This is currently placeholder only
 *
 * Entry point for level-5 interrupts. BYPASSES official RMT driver.
 * Highest maskable priority level that can preempt Wi-Fi interrupts.
 *
 * WARNING: This is NOT supported by official FastLED RMT integration.
 * Critical for RMT LED timing under Wi-Fi load - provides maximum
 * interrupt latency immunity, but requires custom implementation.
 */
extern void xt_highint5(void);

//=============================================================================
// C HANDLER FUNCTION PROTOTYPES
//=============================================================================

/*
 * C Handler Functions (IRAM_ATTR required) - EXPERIMENTAL ONLY
 *
 * These functions are called from the assembly shims for CUSTOM implementations.
 * They are NOT part of the official FastLED RMT driver which uses level 1-3.
 *
 * Requirements for custom handlers:
 * 1. Marked with IRAM_ATTR (placed in IRAM, not flash)
 * 2. Keep execution time minimal
 * 3. No printf, malloc, or FreeRTOS calls
 * 4. Only register and memory operations
 * 5. Must implement custom RMT refill logic (no driver support)
 *
 * Reference: ESP-IDF IRAM Safe Interrupt Service Routines
 * https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig-reference.html
 */

// Level 4 C handler - EXPERIMENTAL, called from xt_highint4 assembly shim
void IRAM_ATTR fastled_esp32_level4_handler(void);

// Level 5 C handler - EXPERIMENTAL, called from xt_highint5 assembly shim
void IRAM_ATTR fastled_esp32_level5_handler(void);

//=============================================================================
// INTERRUPT INSTALLATION HELPERS
//=============================================================================

/*
 * Install Level 3 High-Priority Interrupt (RECOMMENDED)
 *
 * Configures a peripheral interrupt to use level-3 priority - the MAXIMUM
 * supported by official FastLED RMT driver via rmt_tx_channel_config_t.
 *
 * This is the RECOMMENDED approach for FastLED timing improvements.
 *
 * Parameters:
 *   source: Interrupt source (e.g., ETS_RMT_INTR_SOURCE)
 *   arg: User argument passed to handler (can be NULL)
 *   handle: Pointer to store interrupt handle
 *
 * Returns: ESP_OK on success, error code on failure
 *
 * Example usage for RMT (STANDARD FastLED approach):
 *   esp_intr_handle_t rmt_intr_handle;
 *   esp_err_t err = fastled_esp32_install_level3_interrupt(
 *       ETS_RMT_INTR_SOURCE, NULL, &rmt_intr_handle);
 */
esp_err_t fastled_esp32_install_level3_interrupt(
    int source,
    void *arg,
    esp_intr_handle_t *handle
);

/*
 * Install Level 4 High-Priority Interrupt (EXPERIMENTAL)
 *
 * BYPASSES official RMT driver. For experimental/custom implementations only.
 * Requires custom RMT handling code and assembly shim.
 */
esp_err_t fastled_esp32_install_level4_interrupt(
    int source,
    void *arg,
    esp_intr_handle_t *handle
);

/*
 * Install Level 5 High-Priority Interrupt (EXPERIMENTAL)
 *
 * BYPASSES official RMT driver. Maximum priority for custom implementations.
 * Use only when level 3 (official) or level 4 (experimental) is insufficient.
 */
esp_err_t fastled_esp32_install_level5_interrupt(
    int source,
    void *arg,
    esp_intr_handle_t *handle
);

//=============================================================================
// RMT-SPECIFIC INTEGRATION
//=============================================================================

/*
 * RMT Integration for FastLED Timing
 *
 * Official FastLED RMT driver integration and experimental extensions.
 *
 * OFFICIAL APPROACH (RECOMMENDED):
 * - RMT driver uses level 1-3 interrupts via rmt_tx_channel_config_t
 * - Level 3 is MAXIMUM supported by official FastLED RMT driver
 * - Use fastled_esp32_rmt_init_official() for standard integration
 *
 * EXPERIMENTAL APPROACH:
 * - Wi-Fi operations can cause 40-50µs jitter on level 3 interrupts
 * - WS2812 timing tolerance is ~±150ns, potentially requiring level 4+
 * - Requires bypassing official RMT driver with custom implementation
 * - Use fastled_esp32_rmt_init_custom() for experimental high-priority
 *
 * Reference: FastLED RMT5 Driver Flicker Analysis
 * https://github.com/FastLED/FastLED/issues/1761
 */

// RMT interrupt source mappings for ESP32
#define FASTLED_ESP32_RMT_INTR_SOURCE ETS_RMT_INTR_SOURCE

// Initialize RMT with OFFICIAL FastLED driver (level 1-3, RECOMMENDED)
esp_err_t fastled_esp32_rmt_init_official(
    int channel,
    int gpio_num,
    uint32_t resolution_hz,
    size_t mem_block_symbols,
    int interrupt_level  // 1-3 only, 3 recommended
);

// Initialize RMT with CUSTOM high-priority interrupt (EXPERIMENTAL)
esp_err_t fastled_esp32_rmt_init_custom(
    int channel,
    int gpio_num,
    uint32_t resolution_hz,
    size_t mem_block_symbols,
    int interrupt_level  // 4 or 5, bypasses official driver
);

//=============================================================================
// ASSEMBLY TRAMPOLINE MACRO
//=============================================================================

/*
 * FASTLED_ESP_XTENSA_ASM_INTERRUPT_TRAMPOLINE
 *
 * CRITICAL: This is specifically for EXPERIMENTAL interrupt levels 4-5 only.
 * Xtensa REQUIRES assembly for high-priority interrupts (≥4) - C handlers
 * are NOT supported at these levels on ESP32 (LX6).
 *
 * WHY ASSEMBLY IS MANDATORY ON XTENSA:
 * - Levels 1-3: ESP-IDF supports C handlers with automatic cleanup
 * - Levels 4-5: MUST use assembly - no C/RTOS facilities available
 * - Hardware limitation: Xtensa high-priority interrupts bypass C runtime
 * - Manual register save/restore required for proper operation
 *
 * ARCHITECTURAL CONTEXT:
 * - Official FastLED RMT driver: Uses levels 1-3 with C handlers
 * - Experimental bypassed RMT: Uses levels 4-5 requiring assembly
 * - This trampoline bridges assembly requirement with C function calls
 *
 * TODO: VERIFY - This macro may need LX6-specific adjustments
 *
 * Usage:
 *   FASTLED_ESP_XTENSA_ASM_INTERRUPT_TRAMPOLINE(my_isr, my_c_handler)
 *
 * Generates:
 *   void my_isr(void* arg) - assembly trampoline that calls my_c_handler
 *
 * Parameters:
 *   new_function_name: Name of the generated interrupt handler function
 *   function_pointer: C function to call from the trampoline
 *
 * The generated function has signature: void(void*)
 * and is automatically placed in IRAM section.
 */

#define FASTLED_ESP_XTENSA_ASM_INTERRUPT_TRAMPOLINE(new_function_name, function_pointer) \
    __attribute__((section(".iram1"))) \
    __attribute__((used)) \
    __attribute__((naked)) \
    void new_function_name(void* arg) { \
        __asm__ volatile ( \
            ".align 4\n" \
            /* Create stack frame (16-byte aligned) */ \
            "addi a1, a1, -64\n" \
            "s32i a0, a1,  0\n"  /* Save a0 (return address) */ \
            "s32i a2, a1,  4\n"  /* Save a2 (first arg) */ \
            "s32i a3, a1,  8\n"  /* Save a3 */ \
            "s32i a4, a1, 12\n"  /* Save a4 */ \
            "s32i a5, a1, 16\n"  /* Save a5 */ \
            "s32i a6, a1, 20\n"  /* Save a6 */ \
            "s32i a7, a1, 24\n"  /* Save a7 */ \
            "s32i a8, a1, 28\n"  /* Save a8 */ \
            "s32i a9, a1, 32\n"  /* Save a9 */ \
            "s32i a10, a1, 36\n" /* Save a10 */ \
            "s32i a11, a1, 40\n" /* Save a11 */ \
            "s32i a12, a1, 44\n" /* Save a12 */ \
            "s32i a13, a1, 48\n" /* Save a13 */ \
            "s32i a14, a1, 52\n" /* Save a14 */ \
            "s32i a15, a1, 56\n" /* Save a15 */ \
            \
            /* Set up call to C function */ \
            /* arg is passed in a2, which is correct for Xtensa ABI */ \
            "movi a3, %0\n"      /* Load function pointer to a3 */ \
            "callx0 a3\n"        /* Call C function via register */ \
            \
            /* Restore registers from stack */ \
            "l32i a0, a1,  0\n"  /* Restore a0 */ \
            "l32i a2, a1,  4\n"  /* Restore a2 */ \
            "l32i a3, a1,  8\n"  /* Restore a3 */ \
            "l32i a4, a1, 12\n"  /* Restore a4 */ \
            "l32i a5, a1, 16\n"  /* Restore a5 */ \
            "l32i a6, a1, 20\n"  /* Restore a6 */ \
            "l32i a7, a1, 24\n"  /* Restore a7 */ \
            "l32i a8, a1, 28\n"  /* Restore a8 */ \
            "l32i a9, a1, 32\n"  /* Restore a9 */ \
            "l32i a10, a1, 36\n" /* Restore a10 */ \
            "l32i a11, a1, 40\n" /* Restore a11 */ \
            "l32i a12, a1, 44\n" /* Restore a12 */ \
            "l32i a13, a1, 48\n" /* Restore a13 */ \
            "l32i a14, a1, 52\n" /* Restore a14 */ \
            "l32i a15, a1, 56\n" /* Restore a15 */ \
            "addi a1, a1, 64\n"  /* Restore stack pointer */ \
            "ret\n"              /* Return (not RFI - this is C callable) */ \
            : \
            : "i" (function_pointer) \
            : "memory" \
        ); \
    }

/*
 * Example Usage:
 *
 * // Define your C handler function
 * void IRAM_ATTR my_rmt_handler(void* arg) {
 *     // Handle RMT interrupt - minimal code only
 * }
 *
 * // Generate the assembly trampoline
 * FASTLED_ESP_XTENSA_ASM_INTERRUPT_TRAMPOLINE(my_rmt_isr, my_rmt_handler)
 *
 * // Install the interrupt
 * esp_intr_handle_t handle;
 * esp_intr_alloc(ETS_RMT_INTR_SOURCE,
 *                ESP_INTR_FLAG_LEVEL4 | ESP_INTR_FLAG_IRAM,
 *                my_rmt_isr, NULL, &handle);
 */

//=============================================================================
// ASSEMBLY IMPLEMENTATION REFERENCE
//=============================================================================

/*
 * Assembly Shim Implementation Template (Reference)
 *
 * TODO: RESEARCH NEEDED - These assembly directives need verification for LX6
 *
 * The actual assembly implementation should be in a separate .S file:
 * src/platforms/esp/32/interrupts/xtensa_lx6_asm.S
 *
 * Example structure for xt_highint5:
 *
 *     .section .iram1,"ax"
 *     .global xt_highint5
 *     .type xt_highint5,@function
 *     .align 4
 *
 * xt_highint5:
 *     # Create stack frame (16-byte aligned)
 *     addi  a1, a1, -FASTLED_ISR_STACK_FRAME_SIZE
 *
 *     # Save registers used by handler
 *     s32i  a0, a1, FASTLED_ISR_A0_OFFSET
 *     s32i  a2, a1, FASTLED_ISR_A2_OFFSET
 *     s32i  a3, a1, FASTLED_ISR_A3_OFFSET
 *     # ... save other registers as needed
 *
 *     # Call C handler (must be IRAM_ATTR)
 *     call0 fastled_esp32_level5_handler
 *
 *     # Restore registers
 *     l32i  a0, a1, FASTLED_ISR_A0_OFFSET
 *     l32i  a2, a1, FASTLED_ISR_A2_OFFSET
 *     l32i  a3, a1, FASTLED_ISR_A3_OFFSET
 *     # ... restore other registers
 *
 *     # Restore stack frame
 *     addi  a1, a1, FASTLED_ISR_STACK_FRAME_SIZE
 *
 *     # Return from level-5 interrupt
 *     rfi   5
 *
 * Key Points:
 * 1. .iram1 section ensures code is in IRAM
 * 2. 16-byte stack alignment for Xtensa ABI
 * 3. Save/restore only registers actually used
 * 4. call0 for C function (no register windows)
 * 5. rfi 5 restores EPS[5]→PS and EPC[5]→PC
 *
 * Reference: Xtensa ISA Summary, High-Priority Interrupt Option
 * https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf
 */

//=============================================================================
// SAFETY AND LIMITATIONS
//=============================================================================

/*
 * Critical Safety Requirements:
 *
 * 1. IRAM Placement:
 *    - All handler code must be in IRAM (not flash)
 *    - Use IRAM_ATTR for C functions
 *    - Use .iram1 section for assembly
 *    - Ensures operation when flash cache disabled
 *
 * 2. No RTOS Calls:
 *    - Cannot use xTaskNotifyFromISR, xQueueSendFromISR, etc.
 *    - Cannot call printf, malloc, free
 *    - Cannot access variables in flash
 *    - Use only register and SRAM operations
 *
 * 3. Interrupt Masking:
 *    - Level 5 masks all interrupts ≤ level 3 (PS.EXCM=1)
 *    - Wi-Fi (level 4) is masked by level 5
 *    - Keep handler execution time minimal
 *    - NMI (level 6+) can still preempt
 *
 * 4. Stack Management:
 *    - 16-byte alignment required
 *    - Use current task stack (no stack switching)
 *    - Minimal stack usage to avoid overflow
 *
 * Reference: ESP-IDF High Priority Interrupts Guide
 * https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/hlinterrupts.html
 */

//=============================================================================
// BUILD CONFIGURATION
//=============================================================================

/*
 * Required build configuration for high-priority interrupts:
 *
 * Kconfig options:
 *   CONFIG_RMT_ISR_IRAM_SAFE=y      # RMT ISR in IRAM
 *   CONFIG_ESP_SYSTEM_CHECK_INT_LEVEL_4=n  # Allow level 4 interrupts
 *
 * Linker requirements:
 *   - Assembly .S files must be included in build
 *   - .iram1 section properly mapped to IRAM
 *
 * Compiler flags:
 *   -mlongcalls  # Required for IRAM function calls
 */

#ifdef __cplusplus
}
#endif