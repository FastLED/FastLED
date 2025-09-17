#pragma once

// Assembly Shims for High-Priority Interrupts on ESP32-C3/C6 (RISC-V)
//
// This file provides interrupt service routine (ISR) shims for high-priority
// interrupts on ESP32-C3/C6 RISC-V cores with external interrupt controllers.
//
// IMPORTANT: Unlike Xtensa, RISC-V cores can use C handlers at any priority
// level with proper IRAM placement and minimal restrictions.
//
// TODO: This implementation is UNTESTED and may contain errors.
//       PLIC register addresses and assembly directives need research and
//       validation for specific RISC-V ESP32 variants before use.

#include <stdint.h>
#include "soc/soc.h"
#include "esp_intr_alloc.h"
#include "riscv/interrupt.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// RISC-V ARCHITECTURE CONTEXT (ESP32-C3/C6)
//=============================================================================

/*
 * RISC-V Interrupt Architecture References:
 *
 * [1] RISC-V Privileged Architecture Specification v1.12:
 *     https://www.scs.stanford.edu/~zyedidia/docs/riscv/riscv-privileged.pdf
 *
 *     Section: "Machine Interrupt Registers"
 *     Quote: "In vectored mode, all synchronous exceptions into machine mode
 *            cause the pc to be set to the address in the BASE field of mtvec,
 *            whereas interrupts cause the pc to be set to the address in the
 *            BASE field plus four times the interrupt cause number."
 *
 *     Trap Entry Sequence:
 *     mepc ← pc
 *     mcause ← interrupt_cause | (1 << (XLEN-1))  // MSB=1 for interrupts
 *     mstatus.MPIE ← mstatus.MIE
 *     mstatus.MIE ← 0
 *     pc ← mtvec.BASE + 4 * cause  // vectored mode
 *
 *     Trap Return (MRET):
 *     "The MRET instruction is used to return from a trap in M-mode.
 *      MRET first determines what the new privilege mode will be according
 *      to the values of MPP and MPV in mstatus, then sets mstatus.MIE to
 *      the value of mstatus.MPIE, and finally sets pc=mepc."
 *
 * [2] ESP32-C3 Technical Reference Manual:
 *     https://www.espressif.com/sites/default/files/documentation/esp32-c3_technical_reference_manual_en.pdf
 *
 *     Section: "Interrupt Matrix"
 *     ESP32-C3 has 31 interrupt sources with programmable priorities 1-15
 *
 * [3] ESP-IDF RISC-V Interrupt Allocation:
 *     https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/system/intr_alloc.html
 *
 *     Quote: "The interrupt controller allows software to set interrupt priorities"
 *     RISC-V cores use PLIC (Platform-Level Interrupt Controller)
 */

//=============================================================================
// ESP32-C3/C6 INTERRUPT ARCHITECTURE
//=============================================================================

/*
 * ESP32-C3 (RISC-V RV32IMC) Interrupt System:
 *
 * Core: Single-core RISC-V with 32-bit instruction set
 * Interrupt Controller: PLIC (Platform-Level Interrupt Controller)
 * Priority Levels: 1-15 (software programmable)
 * External Interrupts: 31 sources on C3, 28 on C6
 *
 * Key Differences from Xtensa:
 * 1. No fixed priority levels - all priorities are software-configurable
 * 2. C handlers can be used at any priority with IRAM_ATTR
 * 3. No assembly requirement for high-priority interrupts
 * 4. Standard RISC-V trap/return mechanism (mret instruction)
 * 5. External interrupt controller handles arbitration
 *
 * Interrupt Handling Flow:
 * 1. Peripheral asserts interrupt → PLIC
 * 2. PLIC prioritizes and forwards to CPU as machine external interrupt
 * 3. CPU traps to machine mode, saves minimal state
 * 4. Vector table dispatches to specific handler
 * 5. Handler claims interrupt from PLIC, services device
 * 6. Handler completes interrupt in PLIC
 * 7. mret returns to interrupted code
 *
 * Reference: ESP32-C3 TRM, Chapter "Interrupt Matrix"
 */

#ifdef CONFIG_IDF_TARGET_ESP32C3
  #define FASTLED_RISCV_MAX_EXT_INTERRUPTS 31
  #define FASTLED_RISCV_MAX_PRIORITY       7   // Corrected: ESP32-C3 max is 7, not 15
  #define FASTLED_RISCV_CHIP_NAME         "ESP32-C3"
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
  #define FASTLED_RISCV_MAX_EXT_INTERRUPTS 28
  #define FASTLED_RISCV_MAX_PRIORITY       7   // Corrected: ESP32-C6 max is 7, not 15
  #define FASTLED_RISCV_CHIP_NAME         "ESP32-C6"
#else
  #error "Unsupported RISC-V chip - only ESP32-C3 and ESP32-C6 supported"
#endif

//=============================================================================
// RISC-V INTERRUPT PRIORITY RECOMMENDATIONS
//=============================================================================

/*
 * Priority Levels for FastLED Applications on ESP32-C3/C6:
 *
 * Priority 3: MAXIMUM officially supported by FastLED RMT driver
 *             Official rmt_tx_channel_config_t supports levels 1-3 only
 *             RECOMMENDED for standard FastLED timing improvements
 *
 * Priority 1-2: Low/Medium priority (standard operations)
 *               Basic timers, GPIO, UART, non-critical peripherals
 *
 * Priority 4-7: EXPERIMENTAL/CUSTOM implementation only
 *               Requires bypassing official RMT driver
 *               NOT supported by standard FastLED RMT integration
 *               Level 7 may be NMI on some implementations
 *
 * Unlike Xtensa, there's no hard cutoff where assembly is required.
 * All levels can use C handlers with proper IRAM placement.
 *
 * IMPORTANT: Official FastLED RMT driver uses rmt_tx_channel_config_t which
 * only accepts intr_priority values 1-3. Higher levels require custom
 * implementation that bypasses the driver.
 */

#define FASTLED_RISCV_PRIORITY_OFFICIAL_MAX  3   // Maximum supported by RMT driver
#define FASTLED_RISCV_PRIORITY_RECOMMENDED   3   // Recommended for FastLED
#define FASTLED_RISCV_PRIORITY_MEDIUM        2   // Medium priority
#define FASTLED_RISCV_PRIORITY_LOW           1   // Low priority

//=============================================================================
// RISC-V REGISTER CONTEXT
//=============================================================================

/*
 * RISC-V Register Context for Interrupts:
 *
 * Hardware-saved (automatic by trap mechanism):
 * - mepc: Machine Exception Program Counter (return address)
 * - mcause: Machine Cause Register (interrupt source + type)
 * - mstatus: Machine Status Register (interrupt enable state)
 *
 * Software-saved (by compiler/ABI in C handlers):
 * - x1 (ra): Return address register
 * - x5-x7, x10-x17 (t0-t6, a0-a7): Temporary and argument registers
 * - x28-x31 (t3-t6): Additional temporaries
 * - Caller-saved registers per RISC-V ABI
 *
 * The C compiler automatically generates prologue/epilogue for ISR functions
 * marked with IRAM_ATTR, saving/restoring necessary registers.
 *
 * Stack alignment: 16-byte boundary for ABI compliance
 *
 * Reference: RISC-V Calling Convention
 * https://riscv.org/wp-content/uploads/2015/01/riscv-calling.pdf
 */

// RISC-V ABI register definitions
#define FASTLED_RISCV_REG_RA   1   // x1  - return address
#define FASTLED_RISCV_REG_SP   2   // x2  - stack pointer
#define FASTLED_RISCV_REG_A0  10   // x10 - argument/return value 0
#define FASTLED_RISCV_REG_A1  11   // x11 - argument/return value 1

// Stack frame alignment for RISC-V ABI
#define FASTLED_RISCV_STACK_ALIGN 16

//=============================================================================
// INTERRUPT HANDLER DECLARATIONS
//=============================================================================

/*
 * C Interrupt Handlers for FastLED
 *
 * Unlike Xtensa, RISC-V allows C handlers at any priority level.
 * These functions must be:
 * 1. Marked with IRAM_ATTR (placed in IRAM, not flash)
 * 2. Keep execution time minimal for high priorities
 * 3. Use PLIC claim/complete protocol for external interrupts
 * 4. No printf/malloc in high-priority handlers (good practice)
 *
 * The compiler generates proper entry/exit sequences automatically.
 */

// Official FastLED handler (priority 1-3) - RECOMMENDED
void IRAM_ATTR fastled_riscv_official_handler(void *arg);

// Experimental high priority handler (priority 4-7) - CUSTOM only
void IRAM_ATTR fastled_riscv_experimental_handler(void *arg);

//=============================================================================
// PLIC (PLATFORM-LEVEL INTERRUPT CONTROLLER) INTERFACE
//=============================================================================

/*
 * PLIC Register Interface for ESP32-C3/C6
 *
 * The PLIC manages external interrupt sources and priorities.
 * Each interrupt source can be assigned a priority 1-15.
 * Priority 0 disables the interrupt.
 *
 * Key PLIC Operations:
 * 1. Set Priority: Configure interrupt source priority
 * 2. Enable: Enable interrupt source for current hart (CPU)
 * 3. Claim: Read which interrupt is pending (atomically claims it)
 * 4. Complete: Signal interrupt processing complete
 *
 * TODO: VERIFY - These PLIC addresses need validation from official TRM
 *
 * PLIC Base Addresses (ESP32-C3):
 * - Priority registers: 0x600C0000 + 4*source
 * - Enable registers:   0x600C2000 + hart*0x80
 * - Claim/Complete:     0x600C200004 + hart*0x1000
 *
 * Reference: ESP32-C3 TRM, "Interrupt Matrix" chapter
 */

// PLIC base addresses for ESP32-C3 (adjust for C6 if different)
#ifdef CONFIG_IDF_TARGET_ESP32C3
  #define FASTLED_PLIC_PRIORITY_BASE    0x600C0000
  #define FASTLED_PLIC_ENABLE_BASE      0x600C2000
  #define FASTLED_PLIC_CLAIM_BASE       0x600C200004
  #define FASTLED_PLIC_COMPLETE_BASE    0x600C200004
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
  // ESP32-C6 addresses (verify from TRM)
  #define FASTLED_PLIC_PRIORITY_BASE    0x600C0000
  #define FASTLED_PLIC_ENABLE_BASE      0x600C2000
  #define FASTLED_PLIC_CLAIM_BASE       0x600C200004
  #define FASTLED_PLIC_COMPLETE_BASE    0x600C200004
#endif

// PLIC helper functions
static inline void fastled_plic_set_priority(int source, int priority) {
    volatile uint32_t *priority_reg = (volatile uint32_t *)(FASTLED_PLIC_PRIORITY_BASE + 4 * source);
    *priority_reg = priority;
}

static inline void fastled_plic_enable_interrupt(int source) {
    int word_idx = source / 32;
    int bit_idx = source % 32;
    volatile uint32_t *enable_reg = (volatile uint32_t *)(FASTLED_PLIC_ENABLE_BASE + 4 * word_idx);
    *enable_reg |= (1 << bit_idx);
}

static inline uint32_t fastled_plic_claim(void) {
    volatile uint32_t *claim_reg = (volatile uint32_t *)FASTLED_PLIC_CLAIM_BASE;
    return *claim_reg;
}

static inline void fastled_plic_complete(uint32_t interrupt_id) {
    volatile uint32_t *complete_reg = (volatile uint32_t *)FASTLED_PLIC_COMPLETE_BASE;
    *complete_reg = interrupt_id;
}

//=============================================================================
// INTERRUPT INSTALLATION HELPERS
//=============================================================================

/*
 * Install High-Priority Interrupt (RISC-V)
 *
 * Configures a peripheral interrupt to use specified priority level
 * with C handler. Much simpler than Xtensa - no assembly required.
 *
 * Parameters:
 *   source: Interrupt source (e.g., ETS_RMT_INTR_SOURCE)
 *   priority: Priority level 1-3 for official, 4-7 for experimental
 *   handler: C function to handle interrupt (must be IRAM_ATTR)
 *   arg: User argument passed to handler
 *   handle: Pointer to store interrupt handle
 *
 * Returns: ESP_OK on success, error code on failure
 *
 * Example usage for RMT at recommended priority:
 *   esp_intr_handle_t rmt_handle;
 *   esp_err_t err = fastled_riscv_install_interrupt(
 *       ETS_RMT_INTR_SOURCE,
 *       FASTLED_RISCV_PRIORITY_RECOMMENDED,
 *       my_rmt_handler,
 *       NULL,
 *       &rmt_handle);
 */
esp_err_t fastled_riscv_install_interrupt(
    int source,
    int priority,
    void (*handler)(void *),
    void *arg,
    esp_intr_handle_t *handle
);

/*
 * Install Official Priority Interrupt (RECOMMENDED)
 *
 * Convenience function for maximum official priority (3) interrupts.
 * Use for standard FastLED timing via official RMT driver.
 */
esp_err_t fastled_riscv_install_official_interrupt(
    int source,
    void (*handler)(void *),
    void *arg,
    esp_intr_handle_t *handle
);

/*
 * Install Experimental Priority Interrupt
 *
 * Convenience function for experimental high priority (4-7) interrupts.
 * BYPASSES official RMT driver - for custom implementations only.
 */
esp_err_t fastled_riscv_install_experimental_interrupt(
    int source,
    int priority,  // 4-7
    void (*handler)(void *),
    void *arg,
    esp_intr_handle_t *handle
);

//=============================================================================
// RMT-SPECIFIC INTEGRATION (RISC-V)
//=============================================================================

/*
 * RMT Integration for FastLED on RISC-V
 *
 * Official and experimental RMT integration approaches.
 *
 * OFFICIAL APPROACH (RECOMMENDED):
 * - ESP32-C3/C6 RMT driver uses configurable priority interrupts (1-3)
 * - Priority 3 is MAXIMUM supported by official FastLED RMT driver
 * - Use fastled_riscv_rmt_init_official() for standard integration
 * - No assembly shims required - standard C handlers work
 *
 * EXPERIMENTAL APPROACH:
 * - Wi-Fi jitter still affects timing but to a lesser degree than Xtensa
 * - Priority 4-7 provides additional immunity from interference
 * - Requires bypassing official RMT driver with custom implementation
 * - Use fastled_riscv_rmt_init_experimental() for high-priority experiments
 *
 * Reference: ESP-IDF RMT Driver for ESP32-C3
 * https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/peripherals/rmt.html
 */

// RMT interrupt source for ESP32-C3/C6
#ifdef CONFIG_IDF_TARGET_ESP32C3
  #define FASTLED_RISCV_RMT_INTR_SOURCE ETS_RMT_INTR_SOURCE
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
  #define FASTLED_RISCV_RMT_INTR_SOURCE ETS_RMT_INTR_SOURCE
#endif

// Initialize RMT with OFFICIAL FastLED driver (priority 1-3, RECOMMENDED)
esp_err_t fastled_riscv_rmt_init_official(
    int channel,
    int gpio_num,
    uint32_t resolution_hz,
    size_t mem_block_symbols,
    int priority_level  // 1-3, recommend FASTLED_RISCV_PRIORITY_RECOMMENDED
);

// Initialize RMT with EXPERIMENTAL high-priority (4-7, bypasses driver)
esp_err_t fastled_riscv_rmt_init_experimental(
    int channel,
    int gpio_num,
    uint32_t resolution_hz,
    size_t mem_block_symbols,
    int priority_level  // 4-7, custom implementation required
);

// RMT official handler prototype (priority 1-3)
void IRAM_ATTR fastled_riscv_rmt_official_handler(void *arg);

// RMT experimental handler prototype (priority 4-7)
void IRAM_ATTR fastled_riscv_rmt_experimental_handler(void *arg);

//=============================================================================
// INTERRUPT TRAMPOLINE MACRO (OPTIONAL FOR RISC-V)
//=============================================================================

/*
 * FASTLED_ESP_RISCV_INTERRUPT_TRAMPOLINE
 *
 * Generates a C interrupt trampoline function for consistency with Xtensa.
 * On RISC-V, this is optional since C handlers work directly at any priority.
 *
 * Usage:
 *   FASTLED_ESP_RISCV_INTERRUPT_TRAMPOLINE(my_isr, my_c_handler)
 *
 * Generates:
 *   void my_isr(void* arg) - C function that calls my_c_handler
 *
 * Parameters:
 *   new_function_name: Name of the generated interrupt handler function
 *   function_pointer: C function to call from the trampoline
 *
 * The generated function has signature: void(void*)
 * and is automatically placed in IRAM section.
 */

#define FASTLED_ESP_RISCV_INTERRUPT_TRAMPOLINE(new_function_name, function_pointer) \
    __attribute__((section(".iram1"))) \
    __attribute__((used)) \
    void new_function_name(void* arg) { \
        /* RISC-V can call C functions directly from interrupt context */ \
        /* Simple C trampoline - no assembly required unlike Xtensa */ \
        function_pointer(arg); \
    }

/*
 * FASTLED_ESP_RISCV_ASM_INTERRUPT_TRAMPOLINE (Advanced)
 *
 * CRITICAL: This is specifically for EXPERIMENTAL interrupt levels 4-7 only.
 * ESP-IDF does NOT provide PLIC cleanup for custom high-priority interrupts
 * that bypass the official RMT driver (levels 4-7).
 *
 * WHY MANUAL PLIC HANDLING IS REQUIRED:
 * - Levels 1-3: ESP-IDF handles PLIC claim/complete automatically
 * - Levels 4-7: CUSTOM implementation - ESP-IDF provides NO cleanup
 * - Without PLIC complete: Interrupt storm occurs (continuous retriggering)
 * - Manual PLIC protocol prevents system lockup in high-priority handlers
 *
 * ARCHITECTURAL CONTEXT:
 * - Official FastLED RMT driver: Uses levels 1-3 with ESP-IDF management
 * - Experimental bypassed RMT: Uses levels 4-7 requiring manual PLIC
 * - This assembly handles the PLIC protocol ESP-IDF doesn't provide
 *
 * Usage:
 *   FASTLED_ESP_RISCV_ASM_INTERRUPT_TRAMPOLINE(my_isr, my_c_handler)
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

#define FASTLED_ESP_RISCV_ASM_INTERRUPT_TRAMPOLINE(new_function_name, function_pointer) \
    __attribute__((section(".iram1"))) \
    __attribute__((used)) \
    __attribute__((naked)) \
    void new_function_name(void* arg) { \
        __asm__ volatile ( \
            ".align 4\n" \
            /* Create stack frame (16-byte aligned per RISC-V ABI) */ \
            "addi sp, sp, -32\n" \
            "sw   ra, 28(sp)\n"  /* Save return address */ \
            "sw   a0, 24(sp)\n"  /* Save a0 (arg) */ \
            "sw   a1, 20(sp)\n"  /* Save a1 */ \
            "sw   a2, 16(sp)\n"  /* Save a2 */ \
            "sw   a3, 12(sp)\n"  /* Save a3 */ \
            "sw   a4,  8(sp)\n"  /* Save a4 */ \
            "sw   a5,  4(sp)\n"  /* Save a5 */ \
            "sw   t0,  0(sp)\n"  /* Save t0 */ \
            \
            /* CRITICAL: Claim interrupt from PLIC for levels 4-7 */ \
            /* ESP-IDF does NOT handle this for experimental interrupts */ \
            "li   t0, %1\n"      /* Load PLIC_CLAIM_BASE */ \
            "lw   a1, 0(t0)\n"   /* a1 = interrupt_id */ \
            "beqz a1, finish%=\n" /* spurious interrupt check */ \
            \
            /* Call C function with original arg in a0 */ \
            "li   t0, %0\n"      /* Load function pointer */ \
            "jalr ra, t0, 0\n"   /* Call C function */ \
            \
            /* CRITICAL: Complete interrupt in PLIC for levels 4-7 */ \
            /* Without this: interrupt storm (continuous retriggering) */ \
            /* ESP-IDF cleanup ONLY for levels 1-3, NOT 4-7 */ \
            "li   t0, %2\n"      /* Load PLIC_COMPLETE_BASE */ \
            "sw   a1, 0(t0)\n"   /* Complete interrupt_id */ \
            \
            "finish%=:\n" \
            /* Restore registers from stack */ \
            "lw   ra, 28(sp)\n"  /* Restore ra */ \
            "lw   a0, 24(sp)\n"  /* Restore a0 */ \
            "lw   a1, 20(sp)\n"  /* Restore a1 */ \
            "lw   a2, 16(sp)\n"  /* Restore a2 */ \
            "lw   a3, 12(sp)\n"  /* Restore a3 */ \
            "lw   a4,  8(sp)\n"  /* Restore a4 */ \
            "lw   a5,  4(sp)\n"  /* Restore a5 */ \
            "lw   t0,  0(sp)\n"  /* Restore t0 */ \
            "addi sp, sp, 32\n"  /* Restore stack pointer */ \
            "ret\n"              /* Return */ \
            : \
            : "i" (function_pointer), \
              "i" (FASTLED_PLIC_CLAIM_BASE), \
              "i" (FASTLED_PLIC_COMPLETE_BASE) \
            : "memory" \
        ); \
    }

/*
 * Example Usage (RISC-V):
 *
 * // Define your C handler function
 * void IRAM_ATTR my_rmt_handler(void* arg) {
 *     // Handle RMT interrupt - can use normal C code
 * }
 *
 * // Generate the trampoline (optional on RISC-V)
 * FASTLED_ESP_RISCV_INTERRUPT_TRAMPOLINE(my_rmt_isr, my_rmt_handler)
 *
 * // Install the interrupt
 * esp_intr_handle_t handle;
 * esp_intr_alloc(ETS_RMT_INTR_SOURCE,
 *                ESP_INTR_FLAG_IRAM,
 *                my_rmt_isr, NULL, &handle);
 */

//=============================================================================
// ASSEMBLY TRAMPOLINE AUDIT - CRITICAL TECHNICAL ISSUES
//=============================================================================

/*
 * DETAILED TECHNICAL AUDIT OF FASTLED_ESP_RISCV_ASM_INTERRUPT_TRAMPOLINE
 *
 * The assembly trampoline implementation above contains multiple critical
 * technical errors that make it unsuitable for production use:
 *
 * === CRITICAL FAILURES (WILL NOT COMPILE/RUN) ===
 *
 * 1. INVALID 64-BIT ADDRESSES ON 32-BIT ARCHITECTURE:
 *    - FASTLED_PLIC_CLAIM_BASE = 0x600C200004 (40-bit address)
 *    - ESP32-C3/C6 are 32-bit RISC-V cores (RV32IMC)
 *    - Hardware cannot address beyond 0xFFFFFFFF
 *    - Will cause memory access violations
 *
 * 2. LOAD IMMEDIATE INSTRUCTION OVERFLOW:
 *    - "li t0, %1" cannot load 40-bit addresses
 *    - RISC-V li pseudoinstruction limited to 32-bit values
 *    - Requires multi-instruction sequence (lui + addi)
 *    - Assembler will reject this code
 *
 * 3. INCORRECT JALR INSTRUCTION SYNTAX:
 *    - "jalr ra, t0, 0" has redundant offset
 *    - Should be "jalr ra, t0" or "jalr t0"
 *    - Default offset is 0, making ", 0" unnecessary
 *
 * 4. INLINE ASSEMBLY CONSTRAINT VIOLATIONS:
 *    - "i" constraint requires immediate values that fit in instruction
 *    - 40-bit addresses violate this constraint
 *    - Compiler will generate errors during assembly
 *    - Should use "r" constraint with register loading
 *
 * 5. PLIC REGISTER ADDRESS CORRUPTION:
 *    - CLAIM and COMPLETE use identical addresses (0x600C200004)
 *    - Standard PLIC uses different addresses or access methods
 *    - Addresses are unverified (TODO comments admit this)
 *
 * === ARCHITECTURAL VIOLATIONS ===
 *
 * 6. RISC-V CALLING CONVENTION VIOLATION:
 *    - Overwrites a1 (second argument) with interrupt_id
 *    - C function expects arguments in a0, a1 per RISC-V ABI
 *    - Function receives corrupted argument list
 *
 * 7. REGISTER CORRUPTION WITHOUT PROPER RESTORATION:
 *    - Loads interrupt_id into a1, then restores original a1
 *    - interrupt_id is lost if C function needs it
 *    - Creates debugging and logic issues
 *
 * 8. MISSING CLOBBER DECLARATIONS:
 *    - Assembly modifies t0, a1 without declaring clobbers
 *    - Compiler may optimize incorrectly
 *    - Should include "t0", "a1" in clobber list
 *
 * === DESIGN ISSUES ===
 *
 * 9. SPURIOUS INTERRUPT HANDLING:
 *    - Skips PLIC complete for spurious interrupts (a1 == 0)
 *    - Standard PLIC may require completion regardless
 *    - Could cause interrupt storms
 *
 * 10. EXCESSIVE REGISTER PRESERVATION:
 *     - Saves 8 registers when fewer would suffice
 *     - Over-engineering for simple trampoline function
 *     - Performance impact without benefit
 *
 * 11. SECTION ATTRIBUTE REDUNDANCY:
 *     - .iram1 section on naked function is unusual pattern
 *     - Not wrong but unconventional
 *
 * === RECOMMENDATION ===
 *
 * DO NOT USE FASTLED_ESP_RISCV_ASM_INTERRUPT_TRAMPOLINE
 *
 * The assembly implementation is fundamentally broken and will not compile
 * or function correctly. Use the simple C trampoline instead:
 *
 *   FASTLED_ESP_RISCV_INTERRUPT_TRAMPOLINE(my_isr, my_handler)
 *
 * RISC-V allows C handlers at any priority level, making assembly trampolines
 * optional optimizations rather than requirements (unlike Xtensa).
 *
 * If assembly optimization is truly needed:
 * 1. Fix all address and instruction syntax issues
 * 2. Verify PLIC register addresses from ESP32-C3/C6 TRM
 * 3. Implement proper calling convention
 * 4. Add comprehensive testing on target hardware
 *
 * The current implementation demonstrates why assembly should be avoided
 * unless absolutely necessary and thoroughly validated.
 */

//=============================================================================
// ASSEMBLY IMPLEMENTATION REFERENCE (OPTIONAL)
//=============================================================================

/*
 * Assembly Shim Implementation (Optional for RISC-V)
 *
 * TODO: RESEARCH NEEDED - These RISC-V assembly directives need verification
 *
 * While not required like on Xtensa, assembly handlers can provide
 * minimal latency for extremely time-critical operations.
 *
 * Example RISC-V assembly interrupt handler:
 *
 *     .section .iram1,"ax"
 *     .global riscv_critical_isr
 *     .type riscv_critical_isr,@function
 *     .align 4
 *
 * riscv_critical_isr:
 *     # Prologue: save registers (16-byte aligned stack)
 *     addi sp, sp, -16
 *     sw   ra, 12(sp)
 *     sw   t0,  8(sp)
 *     sw   t1,  4(sp)
 *
 *     # Claim interrupt from PLIC
 *     li   t0, FASTLED_PLIC_CLAIM_BASE
 *     lw   t1, 0(t0)        # t1 = interrupt_id
 *     beqz t1, finish       # spurious interrupt
 *
 *     # Service interrupt (device-specific)
 *     # ... minimal register operations only ...
 *
 *     # Complete interrupt in PLIC
 *     li   t0, FASTLED_PLIC_COMPLETE_BASE
 *     sw   t1, 0(t0)        # complete interrupt_id
 *
 * finish:
 *     # Epilogue: restore registers
 *     lw   ra, 12(sp)
 *     lw   t0,  8(sp)
 *     lw   t1,  4(sp)
 *     addi sp, sp, 16
 *
 *     # Return from machine-mode trap
 *     mret
 *
 * Key Differences from Xtensa:
 * 1. Use mret instead of rfi N
 * 2. Standard RISC-V calling convention
 * 3. PLIC claim/complete protocol required
 * 4. No special interrupt level considerations
 *
 * Reference: RISC-V Assembly Programmer's Manual
 * https://github.com/riscv-non-isa/riscv-asm-manual/blob/master/riscv-asm.md
 */

extern void riscv_critical_isr(void);  // Optional assembly handler

//=============================================================================
// SAFETY AND BEST PRACTICES
//=============================================================================

/*
 * RISC-V Interrupt Safety Guidelines:
 *
 * 1. IRAM Placement (Critical):
 *    - Mark all high-priority handlers with IRAM_ATTR
 *    - Ensures operation when flash cache disabled
 *    - Required for priority levels that might preempt flash ops
 *
 * 2. Execution Time:
 *    - Keep high-priority handlers minimal
 *    - Priority 3 (official max) should be ≤ 5-10 microseconds
 *    - Priority 4-7 (experimental) should be ≤ 1-2 microseconds
 *    - Lower priorities can be longer but consider impact
 *
 * 3. PLIC Protocol:
 *    - Always claim interrupt at start of handler
 *    - Always complete interrupt before returning
 *    - Failure to complete causes interrupt storm
 *
 * 4. Stack Usage:
 *    - RISC-V ABI requires 16-byte stack alignment
 *    - Keep stack usage minimal in high-priority handlers
 *    - No deep call chains in critical handlers
 *
 * 5. Data Access:
 *    - High-priority handlers should only access SRAM data
 *    - Avoid flash-mapped constants/strings
 *    - Use volatile for memory-mapped registers
 *
 * 6. Debugging:
 *    - Cannot use printf in high-priority handlers
 *    - Use LED indicators or memory dumps for debugging
 *    - Consider lower priority for development versions
 *
 * Unlike Xtensa, RISC-V doesn't have strict assembly requirements,
 * but following these guidelines ensures reliable operation.
 */

//=============================================================================
// INTERRUPT SOURCE DEFINITIONS
//=============================================================================

/*
 * Common Interrupt Sources for ESP32-C3/C6
 *
 * These are the peripheral interrupt sources that can be configured
 * for high-priority operation with FastLED.
 *
 * Reference: ESP32-C3/C6 Technical Reference Manual, Interrupt Matrix
 */

// RMT peripheral interrupts
#ifdef CONFIG_IDF_TARGET_ESP32C3
  #define FASTLED_INTR_RMT_CH0  ETS_RMT_INTR_SOURCE
  // C3 has shared RMT interrupt for all channels
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
  #define FASTLED_INTR_RMT_CH0  ETS_RMT_INTR_SOURCE
  // C6 RMT interrupt mapping (verify from TRM)
#endif

// GPIO interrupts (for bit-banging)
#define FASTLED_INTR_GPIO     ETS_GPIO_INTR_SOURCE

// Timer interrupts (for frame timing)
#define FASTLED_INTR_TIMER0   ETS_TG0_T0_LEVEL_INTR_SOURCE
#define FASTLED_INTR_TIMER1   ETS_TG0_T1_LEVEL_INTR_SOURCE

// DMA interrupts (for large transfers)
#define FASTLED_INTR_DMA_CH0  ETS_DMA_CH0_INTR_SOURCE
#define FASTLED_INTR_DMA_CH1  ETS_DMA_CH1_INTR_SOURCE

//=============================================================================
// BUILD CONFIGURATION
//=============================================================================

/*
 * Required build configuration for RISC-V high-priority interrupts:
 *
 * Kconfig options:
 *   CONFIG_RMT_ISR_IRAM_SAFE=y           # RMT ISR in IRAM
 *   CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=n # Disable memory protection for performance
 *
 * Compiler flags:
 *   -march=rv32imc       # RISC-V ISA with compressed instructions
 *   -mabi=ilp32          # 32-bit integer ABI
 *
 * Linker requirements:
 *   - .iram1 section properly mapped to IRAM
 *   - Sufficient IRAM for all interrupt handlers
 *
 * Much simpler than Xtensa - no special assembly handling required.
 */

#ifdef __cplusplus
}
#endif