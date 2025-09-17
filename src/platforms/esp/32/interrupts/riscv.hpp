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
 * ESP32-C3/C6 (RISC-V RV32IMC) Interrupt System:
 *
 * Core: Single-core RISC-V with 32-bit instruction set
 * Interrupt Controller: CUSTOM Espressif implementation (NOT standard RISC-V PLIC)
 * Priority Levels: 1-7 (independently programmable per source)
 * External Interrupts: 31 sources on C3, 28 on C6
 *
 * CRITICAL: ESP32-C3/C6 do NOT implement standard RISC-V PLIC
 * Instead, they use Espressif's custom interrupt matrix with ESP-IDF integration.
 *
 * Key Differences from Xtensa:
 * 1. No fixed priority levels - all priorities are software-configurable (1-7)
 * 2. C handlers can be used at any priority with IRAM_ATTR
 * 3. No assembly requirement for high-priority interrupts
 * 4. Standard RISC-V trap/return mechanism (mret instruction)
 * 5. Custom interrupt controller handles arbitration (NOT PLIC)
 * 6. ESP-IDF provides complete interrupt management (no manual claim/complete)
 *
 * Interrupt Handling Flow:
 * 1. Peripheral asserts interrupt → Espressif Interrupt Matrix
 * 2. Interrupt Matrix prioritizes and forwards to CPU
 * 3. CPU traps to machine mode, saves minimal state
 * 4. ESP-IDF vector table dispatches to specific handler
 * 5. Handler services device (ESP-IDF manages all protocol)
 * 6. Return from handler
 * 7. mret returns to interrupted code
 *
 * Reference: ESP32-C3 TRM, Chapter 8 "Interrupt Matrix (INTERRUPT)"
 * Reference: ESP-IDF Interrupt Allocation API documentation
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
// ESP32-C3/C6 INTERRUPT CONTROLLER INTERFACE (NOT PLIC)
//=============================================================================

/*
 * ESP32-C3/C6 Custom Interrupt Controller Interface
 *
 * IMPORTANT: ESP32-C3/C6 do NOT use standard RISC-V PLIC.
 * They implement Espressif's custom interrupt matrix managed by ESP-IDF.
 *
 * ESP-IDF Interrupt Management:
 * - Priority assignment: 1-7 per interrupt source (independently programmable)
 * - Shared interrupts: Multiple peripherals can share same interrupt line
 * - Non-shared interrupts: Dedicated to single peripheral
 * - IRAM-safe handlers: Use ESP_INTR_FLAG_IRAM for flash-safe execution
 *
 * NO MANUAL PLIC OPERATIONS REQUIRED:
 * ESP-IDF automatically handles all interrupt controller protocol:
 * 1. Interrupt registration via esp_intr_alloc()
 * 2. Priority configuration
 * 3. Enable/disable management
 * 4. Handler dispatch
 * 5. Interrupt acknowledgment
 *
 * For FastLED, use standard ESP-IDF interrupt allocation APIs:
 * - esp_intr_alloc() for handler installation
 * - ESP_INTR_FLAG_IRAM for high-priority handlers
 * - Priority levels 1-7 (7 is highest)
 *
 * Reference: ESP32-C3/C6 TRM, Chapter 8 "Interrupt Matrix (INTERRUPT)"
 * Reference: ESP-IDF Interrupt Allocation documentation
 */

// ESP32-C3/C6 use ESP-IDF APIs, not direct register access
// The following PLIC-style functions are NOT APPLICABLE and should not be used:

#if 0  // DISABLED - ESP32-C3/C6 do not use PLIC
// INVALID: These addresses are incorrect for ESP32-C3/C6
#ifdef CONFIG_IDF_TARGET_ESP32C3
  #define FASTLED_PLIC_PRIORITY_BASE    0x600C0000      // INVALID
  #define FASTLED_PLIC_ENABLE_BASE      0x600C2000      // INVALID
  #define FASTLED_PLIC_CLAIM_BASE       0x600C200004ULL // INVALID - 40-bit address on 32-bit arch
  #define FASTLED_PLIC_COMPLETE_BASE    0x600C200004ULL // INVALID - 40-bit address on 32-bit arch
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
  #define FASTLED_PLIC_PRIORITY_BASE    0x600C0000      // INVALID
  #define FASTLED_PLIC_ENABLE_BASE      0x600C2000      // INVALID
  #define FASTLED_PLIC_CLAIM_BASE       0x600C200004ULL // INVALID - 40-bit address on 32-bit arch
  #define FASTLED_PLIC_COMPLETE_BASE    0x600C200004ULL // INVALID - 40-bit address on 32-bit arch
#endif

// INVALID FUNCTIONS - Do not use with ESP32-C3/C6
static inline void fastled_plic_set_priority(int source, int priority) {
    // ESP32-C3/C6 use ESP-IDF APIs, not direct PLIC access
    (void)source; (void)priority;
}

static inline void fastled_plic_enable_interrupt(int source) {
    // ESP32-C3/C6 use ESP-IDF APIs, not direct PLIC access
    (void)source;
}

static inline uint32_t fastled_plic_claim(void) {
    // ESP32-C3/C6 use ESP-IDF APIs, not direct PLIC access
    return 0;
}

static inline void fastled_plic_complete(uint32_t interrupt_id) {
    // ESP32-C3/C6 use ESP-IDF APIs, not direct PLIC access
    (void)interrupt_id;
}
#endif  // DISABLED PLIC functions

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
 * FASTLED_ESP_RISCV_ASM_INTERRUPT_TRAMPOLINE (DISABLED - BROKEN IMPLEMENTATION)
 *
 * CRITICAL NOTICE: This macro contains multiple fatal technical errors and is disabled.
 * ESP32-C3/C6 do NOT use standard RISC-V PLIC and ESP-IDF handles ALL interrupt
 * management automatically. Manual assembly trampolines are NOT NEEDED.
 *
 * CRITICAL FAILURES IN ORIGINAL IMPLEMENTATION:
 * 1. INVALID 40-bit addresses (0x600C200004) on 32-bit ESP32-C3/C6 architecture
 * 2. Assembler "li" instruction cannot load 40-bit immediate values
 * 3. ESP32-C3/C6 use custom interrupt controller, NOT standard RISC-V PLIC
 * 4. ESP-IDF provides complete interrupt management for ALL priority levels
 * 5. RISC-V calling convention violations (corrupts function arguments)
 * 6. Incorrect inline assembly constraints for large addresses
 *
 * RECOMMENDED APPROACH FOR ESP32-C3/C6:
 * - Use standard ESP-IDF interrupt allocation (esp_intr_alloc)
 * - Use simple C handlers with IRAM_ATTR (no assembly required)
 * - ESP-IDF handles all interrupt controller protocol automatically
 * - Priority levels 1-7 all work with C handlers
 *
 * The simple C trampoline (FASTLED_ESP_RISCV_INTERRUPT_TRAMPOLINE) is sufficient
 * for all FastLED use cases on ESP32-C3/C6.
 */

#if 0  // DISABLED - Broken assembly implementation
#define FASTLED_ESP_RISCV_ASM_INTERRUPT_TRAMPOLINE(new_function_name, function_pointer) \
    /* THIS MACRO IS DISABLED DUE TO CRITICAL TECHNICAL ERRORS */ \
    static_assert(false, "FASTLED_ESP_RISCV_ASM_INTERRUPT_TRAMPOLINE is disabled due to critical errors. Use FASTLED_ESP_RISCV_INTERRUPT_TRAMPOLINE instead.");
#endif  // DISABLED

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

//=============================================================================
// IMPLEMENTATION SUMMARY AND RESEARCH FINDINGS (2024)
//=============================================================================

/*
 * FASTLED ESP32-C3/C6 RISC-V INTERRUPT IMPLEMENTATION RESEARCH SUMMARY
 *
 * This file has been thoroughly researched and updated based on official
 * Espressif documentation and RISC-V specifications. Key findings:
 *
 * === ARCHITECTURE CLARIFICATIONS ===
 *
 * 1. ESP32-C3/C6 do NOT implement standard RISC-V PLIC
 *    - Use Espressif's custom interrupt matrix instead
 *    - All interrupt management handled by ESP-IDF
 *    - No manual claim/complete protocol required
 *
 * 2. Priority levels: 1-7 (independently programmable)
 *    - Level 7 is highest priority
 *    - Levels 1-3: Officially documented, support C handlers with IRAM_ATTR
 *    - Levels 4-7: Documentation incomplete - requirements unclear
 *
 *    CITATION: ESP-IDF docs state "shared interrupts can use priority levels 2 and 3"
 *    with ESP_INTR_FLAG_LOWMED, but no mention of levels 4-7 support.
 *    Source: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/system/intr_alloc.html
 *
 * 3. ESP-IDF provides complete interrupt management
 *    - Use esp_intr_alloc() for all interrupt installation
 *    - ESP_INTR_FLAG_IRAM for flash-safe high-priority handlers
 *    - Automatic enable/disable and handler dispatch
 *
 * === CRITICAL ERRORS FIXED ===
 *
 * 1. Removed invalid PLIC register addresses (40-bit addresses on 32-bit arch)
 *    CITATION: Original code used 0x600C200004 (40-bit) on 32-bit ESP32-C3/C6
 *    Invalid because ESP32-C3/C6 are RV32IMC (32-bit address space)
 *
 * 2. Disabled broken assembly trampoline macro
 *    CITATION: Original FASTLED_ESP_RISCV_ASM_INTERRUPT_TRAMPOLINE had multiple
 *    fatal errors: invalid "li t0, %1" for 40-bit addresses, incorrect JALR syntax,
 *    and wrong assumption that ESP32-C3/C6 use standard RISC-V PLIC
 *
 * 3. Corrected architectural documentation
 *    CITATION: ESP32-C3/C6 use "Espressif's custom interrupt matrix" not PLIC
 *    Source: Research from ESP-IDF documentation and RISC-V PLIC specification
 *
 * 4. Updated priority level definitions (1-7, not 1-15)
 *    CITATION: ESP32-C3 TRM shows priority levels 1-6, ESP32-C6 similar
 *    Original 1-15 was copied from generic RISC-V PLIC spec
 *
 * 5. Clarified ESP-IDF vs manual interrupt management
 *    CITATION: ESP-IDF handles "all interrupt controller protocol" automatically
 *    Source: ESP-IDF interrupt allocation documentation
 *
 * === RECOMMENDATIONS FOR FASTLED ===
 *
 * 1. OFFICIAL APPROACH (recommended):
 *    - Use priority levels 1-3 with official RMT driver
 *    - Standard ESP-IDF interrupt allocation
 *    - Simple C handlers with IRAM_ATTR
 *    - No assembly trampolines needed
 *    - Fully documented and supported
 *
 * 2. EXPERIMENTAL APPROACH (advanced - requirements unclear):
 *    - Priority levels 4-7 have incomplete documentation
 *    - May require assembly handlers (similar to Xtensa ESP32)
 *    - ESP-IDF allocation API may not support these levels
 *    - Requires further research and testing to validate
 *
 *    CITATION: For original ESP32 (Xtensa): "High level interrupts. Need to be handled
 *    in assembly." and "Do not call C code from a high-priority interrupt"
 *    Source: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/hlinterrupts.html
 *
 *    COUNTER-CITATION: ESP32-C3 forum evidence: "ESP32C3 allows for high-level
 *    interrupt handlers being coded in C, and there is no much time-benefit from
 *    writing the ISR (even NMI level interrupt) in ASM"
 *    Source: https://www.esp32.com/viewtopic.php?t=23480
 *
 * 3. AVOID:
 *    - Manual PLIC register access (not applicable to ESP32-C3/C6)
 *    - Assembly trampolines (unnecessary complexity)
 *    - Hardcoded register addresses (use ESP-IDF APIs)
 *    - Direct interrupt controller manipulation
 *
 * This implementation provides a solid foundation for FastLED interrupt
 * handling on ESP32-C3/C6 RISC-V platforms with proper ESP-IDF integration.
 *
 * === RESEARCH UNCERTAINTY ===
 *
 * PRIORITY LEVELS 4-7 STATUS: UNCLEAR
 *
 * CONFLICTING EVIDENCE:
 *
 * 1. ESP-IDF Official Documentation (ESP32-C3):
 *    "Shared interrupts by default use only priority level 1 interrupts. However,
 *    you can allocate a shared interrupt with ESP_INTR_FLAG_SHARED|ESP_INTR_FLAG_LOWMED
 *    to use priority levels 2 and 3 as well."
 *    Source: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/system/intr_alloc.html
 *    IMPLICATION: No mention of levels 4-7, suggests API limitation
 *
 * 2. ESP-IDF High Priority Interrupts Guide (Xtensa ESP32 only):
 *    "High level interrupts. Need to be handled in assembly."
 *    "Do not call C code from a high-priority interrupt; as these interrupts are run
 *    from a critical section, this can cause the target to crash."
 *    Source: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/hlinterrupts.html
 *    IMPLICATION: Assembly required for levels 4-7 on Xtensa, unclear for RISC-V
 *
 * 3. Espressif Staff Forum Response (ESP32-C3):
 *    "ESP32C3 allows for high-level interrupt handlers being coded in C, and there
 *    is no much time-benefit from writing the ISR (even NMI level interrupt) in ASM
 *    than writing in C for ESP32C3 chips."
 *    Source: https://www.esp32.com/viewtopic.php?t=23480 (Espressif staff response)
 *    IMPLICATION: C handlers may work at all levels on RISC-V ESP32-C3
 *
 * CONCLUSION: Official ESP-IDF APIs only document support for levels 1-3.
 * While anecdotal evidence suggests C handlers work at higher levels on RISC-V,
 * this lacks official confirmation and may not be accessible via esp_intr_alloc().
 * Conservative approach: stick to documented levels 1-3 until clarified.
 *
 * Date: 2024 Research Update
 * Status: Partially verified - priority level restrictions need clarification
 */

#ifdef __cplusplus
}
#endif