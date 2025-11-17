// ok no namespace fl
#pragma once

#include "fl/compiler_control.h"

/*
 * ESP32 NMI (Level 7) Assembly-to-C Shim Macros
 *
 * PURPOSE:
 * Enable Level 7 Non-Maskable Interrupts (NMI) on ESP32 Xtensa architectures
 * to safely call C functions, specifically for RMT buffer refill operations
 * where WiFi interference cannot be tolerated.
 *
 * CRITICAL REQUIREMENTS FOR C HANDLER FUNCTIONS:
 * 1. MUST be marked FL_IRAM (code must be in IRAM, not flash)
 * 2. MUST NOT call FreeRTOS APIs (xSemaphore*, xQueue*, xTask*, etc.)
 * 3. MUST access only DRAM variables (use DRAM_ATTR for global state)
 * 4. SHOULD complete in <500ns (WS2812 timing safety margin)
 * 5. MUST use extern "C" linkage for C++ code
 *
 * ARCHITECTURE SUPPORT:
 * - ✅ ESP32 (Xtensa LX6)
 * - ✅ ESP32-S2 (Xtensa LX7)
 * - ✅ ESP32-S3 (Xtensa LX7)
 * - ❌ ESP32-C3/C6/H2 (RISC-V) - Not applicable, use RISC-V NMI handling
 *
 * IMPLEMENTATION STATUS:
 * - Assembly validated against Xtensa ISA Reference Manual (Cadence)
 * - Register usage validated against Call0 ABI specification
 * - Stack frame follows 16-byte alignment requirement
 * - Tested on: [Pending hardware validation]
 * - Measured overhead: [Pending timing measurement]
 *
 * REFERENCES:
 * [1] Xtensa ISA Reference Manual
 *     https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf
 *     Section: "High-Priority Interrupt Option", Call0 ABI
 *
 * [2] ESP32 Technical Reference Manual
 *     https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf
 *     Chapter: Interrupt Matrix, RMT Peripheral
 *
 * [3] ESP-IDF High Priority Interrupts Guide
 *     https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/hlinterrupts.html
 *
 * [4] FastLED ESP32 Interrupt Architecture
 *     See: src/platforms/esp/32/XTENSA_INTERRUPTS.md
 *     See: src/platforms/esp/32/interrupts/LOOP.md
 */

#ifdef ESP32

FL_EXTERN_C_BEGIN

//=============================================================================
// ARCHITECTURE DETECTION AND VALIDATION
//=============================================================================

#if defined(CONFIG_IDF_TARGET_ESP32)
    #define FASTLED_XTENSA_LX6 1
    #define FASTLED_NMI_ARCH_NAME "Xtensa LX6"
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    #define FASTLED_XTENSA_LX7 1
    #define FASTLED_NMI_ARCH_NAME "Xtensa LX7"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    #define FASTLED_XTENSA_LX7 1
    #define FASTLED_NMI_ARCH_NAME "Xtensa LX7"
#else
    #error "ASM_2_C_SHIM.h: Unknown ESP32 Xtensa variant. Only ESP32, ESP32-S2, ESP32-S3 supported."
#endif

// Verify this is not RISC-V (C3/C6/H2)
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    #error "ASM_2_C_SHIM.h: RISC-V platforms (ESP32-C3/C6/H2) not supported. Use RISC-V-specific interrupt handling."
#endif

//=============================================================================
// REGISTER SAVE/RESTORE CONFIGURATION
//=============================================================================

/*
 * Xtensa Call0 ABI Register Convention:
 *
 * a0  - Return address (saved by caller, must preserve)
 * a1  - Stack pointer (SP, callee must maintain 16-byte alignment)
 * a2  - First argument / return value
 * a3  - Second argument / return value low
 * a4  - Third argument / return value high
 * a5  - Fourth argument
 * a6  - Fifth argument
 * a7  - Sixth argument
 * a8  - Static chain (nested functions)
 * a9  - Temporary (caller-saved)
 * a10 - Temporary (caller-saved)
 * a11 - Temporary (caller-saved)
 * a12 - Callee-saved (MUST preserve for caller)
 * a13 - Callee-saved (MUST preserve for caller)
 * a14 - Callee-saved (MUST preserve for caller)
 * a15 - Callee-saved / optional frame pointer (MUST preserve for caller)
 *
 * Stack Frame: 16-byte aligned, grows downward (lower addresses)
 *
 * Reference: Xtensa ISA Manual, Call0 ABI Calling Convention
 */

// Stack frame size: 16 registers × 4 bytes/register = 64 bytes
// Maintains 16-byte alignment: 64 % 16 = 0 ✓
#define FASTLED_NMI_STACK_FRAME_SIZE 64

// Register save offsets (in bytes from stack pointer after frame allocation)
#define FASTLED_NMI_A0_OFFSET   0   // Return address
#define FASTLED_NMI_A1_OFFSET   4   // Stack pointer (not actually saved, just for completeness)
#define FASTLED_NMI_A2_OFFSET   8   // First argument register
#define FASTLED_NMI_A3_OFFSET   12
#define FASTLED_NMI_A4_OFFSET   16
#define FASTLED_NMI_A5_OFFSET   20
#define FASTLED_NMI_A6_OFFSET   24
#define FASTLED_NMI_A7_OFFSET   28
#define FASTLED_NMI_A8_OFFSET   32
#define FASTLED_NMI_A9_OFFSET   36
#define FASTLED_NMI_A10_OFFSET  40
#define FASTLED_NMI_A11_OFFSET  44
#define FASTLED_NMI_A12_OFFSET  48  // Callee-saved start
#define FASTLED_NMI_A13_OFFSET  52
#define FASTLED_NMI_A14_OFFSET  56
#define FASTLED_NMI_A15_OFFSET  60  // Callee-saved end

//=============================================================================
// MACRO: FASTLED_NMI_ASM_SHIM_STATIC
//=============================================================================

/*
 * Generate Level 7 NMI Handler with Static C Function Call
 *
 * Creates an assembly interrupt handler that saves all registers, calls a
 * known C function using Call0 ABI, restores registers, and returns via RFI 7.
 *
 * PARAMETERS:
 *   handler_name: Symbol name for the NMI handler (e.g., "xt_nmi")
 *                 This is the symbol ESP-IDF will call when NMI fires.
 *   c_function:   Name of C function to call (must be FL_IRAM)
 *                 Function signature: extern "C" void c_function(void)
 *
 * GENERATED HANDLER:
 *   - Placed in .iram1.text section (IRAM, not flash)
 *   - Entry point: handler_name (global symbol)
 *   - Saves: All 16 Xtensa registers (a0-a15)
 *   - Calls: c_function using direct call0 (Call0 ABI)
 *   - Restores: All 16 registers in reverse order
 *   - Returns: Via rfi 7 (Return From Interrupt level 7)
 *
 * PERFORMANCE:
 *   - Prologue: ~30ns (16 × s32i instructions at 240 MHz)
 *   - Call: ~5ns (direct call0, no function pointer overhead)
 *   - Epilogue: ~30ns (16 × l32i instructions)
 *   - Total overhead: ~65ns (excludes C function execution time)
 *
 * USAGE EXAMPLE:
 *   // Declare C handler function (in your .cpp file)
 *   extern "C" void FL_IRAM rmt_nmi_refill_buffer(void) {
 *       // Your RMT buffer refill logic here
 *       // NO FreeRTOS calls! NO flash access!
 *   }
 *
 *   // Generate NMI shim (in same .cpp file or header)
 *   FASTLED_NMI_ASM_SHIM_STATIC(xt_nmi, rmt_nmi_refill_buffer)
 *
 *   // Install Level 7 NMI (in setup code)
 *   esp_intr_handle_t nmi_handle;
 *   esp_err_t err = esp_intr_alloc(
 *       ETS_RMT_INTR_SOURCE,                      // Interrupt source
 *       ESP_INTR_FLAG_LEVEL7 | ESP_INTR_FLAG_IRAM, // Flags
 *       nullptr,                                  // Handler (nullptr for Level 7)
 *       nullptr,                                  // Arg (nullptr for Level 7)
 *       &nmi_handle                               // Output handle
 *   );
 *
 * CRITICAL NOTES:
 * 1. ESP-IDF requires handler=nullptr and arg=nullptr for Level 7
 * 2. ESP-IDF looks for the "xt_nmi" symbol at link time
 * 3. C function MUST be extern "C" for correct symbol name
 * 4. C function MUST be FL_IRAM to avoid flash cache misses
 * 5. Do not call this macro multiple times with same handler_name
 */
#define FASTLED_NMI_ASM_SHIM_STATIC(handler_name, c_function) \
    __asm__ ( \
        /* Place handler in IRAM text section (not flash) */ \
        ".section .iram1.text\n" \
        \
        /* Declare global symbol for linker */ \
        ".global " #handler_name "\n" \
        \
        /* Mark as function type for debugging */ \
        ".type " #handler_name ", @function\n" \
        \
        /* Align to 4-byte boundary for instruction fetch */ \
        ".align 4\n" \
        \
        /* Handler entry point */ \
        #handler_name ":\n" \
        \
        /* ===== PROLOGUE: Save all registers (Call0 ABI) ===== */ \
        \
        /* Allocate stack frame (64 bytes, 16-byte aligned) */ \
        "    addi    a1, a1, -64\n" \
        \
        /* Save all general-purpose registers */ \
        /* a0 = return address (critical for call0) */ \
        "    s32i    a0, a1,  0\n" \
        /* a1 = stack pointer (not saved, we're modifying it) */ \
        /* a2-a7 = argument/temp registers (caller-saved per ABI) */ \
        "    s32i    a2, a1,  8\n" \
        "    s32i    a3, a1, 12\n" \
        "    s32i    a4, a1, 16\n" \
        "    s32i    a5, a1, 20\n" \
        "    s32i    a6, a1, 24\n" \
        "    s32i    a7, a1, 28\n" \
        /* a8-a11 = temporary registers (caller-saved) */ \
        "    s32i    a8, a1, 32\n" \
        "    s32i    a9, a1, 36\n" \
        "    s32i    a10, a1, 40\n" \
        "    s32i    a11, a1, 44\n" \
        /* a12-a15 = callee-saved registers (MUST preserve per ABI) */ \
        "    s32i    a12, a1, 48\n" \
        "    s32i    a13, a1, 52\n" \
        "    s32i    a14, a1, 56\n" \
        "    s32i    a15, a1, 60\n" \
        \
        /* ===== CALL C FUNCTION (Call0 ABI) ===== */ \
        \
        /* Direct call to known C function symbol */ \
        /* call0 sets a0 = PC+3, then PC = c_function */ \
        "    call0   " #c_function "\n" \
        \
        /* ===== EPILOGUE: Restore all registers ===== */ \
        \
        /* Restore all general-purpose registers */ \
        "    l32i    a0, a1,  0\n"  /* Restore return address first */ \
        "    l32i    a2, a1,  8\n" \
        "    l32i    a3, a1, 12\n" \
        "    l32i    a4, a1, 16\n" \
        "    l32i    a5, a1, 20\n" \
        "    l32i    a6, a1, 24\n" \
        "    l32i    a7, a1, 28\n" \
        "    l32i    a8, a1, 32\n" \
        "    l32i    a9, a1, 36\n" \
        "    l32i    a10, a1, 40\n" \
        "    l32i    a11, a1, 44\n" \
        /* Restore callee-saved registers */ \
        "    l32i    a12, a1, 48\n" \
        "    l32i    a13, a1, 52\n" \
        "    l32i    a14, a1, 56\n" \
        "    l32i    a15, a1, 60\n" \
        \
        /* Deallocate stack frame (restore SP) */ \
        "    addi    a1, a1, 64\n" \
        \
        /* ===== RETURN FROM NMI ===== */ \
        \
        /* Return from interrupt level 7 */ \
        /* rfi 7: PS ← EPS7, PC ← EPC7 (hardware restores state) */ \
        "    rfi     7\n" \
        \
        /* Mark end of function for debugging info */ \
        ".size " #handler_name ", .-" #handler_name "\n" \
    )

//=============================================================================
// MACRO: FASTLED_NMI_ASM_SHIM_DYNAMIC
//=============================================================================

/*
 * Generate Level 7 NMI Handler with Dynamic Function Pointer Call
 *
 * Creates an assembly handler that calls a C function via a global function
 * pointer, allowing the handler to be changed at runtime. Adds ~8ns overhead
 * compared to static variant due to pointer load.
 *
 * PARAMETERS:
 *   handler_name: Symbol name for the NMI handler (e.g., "xt_nmi_dynamic")
 *   ptr_variable: Global function pointer variable (must be DRAM_ATTR)
 *                 Type: void (*ptr_variable)(void)
 *
 * FUNCTION POINTER REQUIREMENTS:
 *   - Must be declared with DRAM_ATTR (in DRAM, not flash)
 *   - Type: typedef void (*nmi_handler_t)(void)
 *   - Must point to FL_IRAM function
 *   - Example: nmi_handler_t DRAM_ATTR g_nmi_handler = &default_handler;
 *
 * PERFORMANCE:
 *   - Prologue: ~30ns (same as static)
 *   - Pointer load: ~8ns (movi + l32i)
 *   - Call: ~5ns (callx0 via register)
 *   - Epilogue: ~30ns (same as static)
 *   - Total overhead: ~73ns (8ns more than static variant)
 *
 * USAGE EXAMPLE:
 *   // Declare function pointer type
 *   typedef void (*nmi_handler_t)(void);
 *
 *   // Define handler functions
 *   extern "C" void FL_IRAM nmi_handler_default(void) { /* ... */ }
 *   extern "C" void FL_IRAM nmi_handler_alternate(void) { /* ... */ }
 *
 *   // Declare function pointer in DRAM
 *   nmi_handler_t DRAM_ATTR g_nmi_handler = &nmi_handler_default;
 *
 *   // Generate dynamic NMI shim
 *   FASTLED_NMI_ASM_SHIM_DYNAMIC(xt_nmi, g_nmi_handler)
 *
 *   // Change handler at runtime (before NMI fires)
 *   g_nmi_handler = &nmi_handler_alternate;
 *
 * USE CASES:
 *   - Multiple RMT channels with different refill logic
 *   - Runtime switching between test and production handlers
 *   - Conditional behavior based on system state
 *
 * CRITICAL NOTES:
 * 1. Function pointer variable MUST be in DRAM (use DRAM_ATTR)
 * 2. Pointed-to function MUST be in IRAM (use FL_IRAM)
 * 3. Updating pointer while NMI enabled creates race condition (disable first)
 * 4. Null pointer check not included (would add overhead, assume valid)
 */
#define FASTLED_NMI_ASM_SHIM_DYNAMIC(handler_name, ptr_variable) \
    __asm__ ( \
        /* Place handler in IRAM text section */ \
        ".section .iram1.text\n" \
        ".global " #handler_name "\n" \
        ".type " #handler_name ", @function\n" \
        ".align 4\n" \
        #handler_name ":\n" \
        \
        /* ===== PROLOGUE: Save all registers ===== */ \
        "    addi    a1, a1, -64\n" \
        "    s32i    a0, a1,  0\n" \
        "    s32i    a2, a1,  8\n" \
        "    s32i    a3, a1, 12\n" \
        "    s32i    a4, a1, 16\n" \
        "    s32i    a5, a1, 20\n" \
        "    s32i    a6, a1, 24\n" \
        "    s32i    a7, a1, 28\n" \
        "    s32i    a8, a1, 32\n" \
        "    s32i    a9, a1, 36\n" \
        "    s32i    a10, a1, 40\n" \
        "    s32i    a11, a1, 44\n" \
        "    s32i    a12, a1, 48\n" \
        "    s32i    a13, a1, 52\n" \
        "    s32i    a14, a1, 56\n" \
        "    s32i    a15, a1, 60\n" \
        \
        /* ===== LOAD FUNCTION POINTER AND CALL ===== */ \
        \
        /* Load address of function pointer variable */ \
        "    movi    a3, " #ptr_variable "\n" \
        \
        /* Load function pointer from memory */ \
        /* a3 now contains &ptr_variable, load *ptr_variable */ \
        "    l32i    a3, a3, 0\n" \
        \
        /* Indirect call via register (Call0 ABI) */ \
        /* callx0 sets a0 = PC+3, then PC = a3 */ \
        "    callx0  a3\n" \
        \
        /* ===== EPILOGUE: Restore all registers ===== */ \
        "    l32i    a0, a1,  0\n" \
        "    l32i    a2, a1,  8\n" \
        "    l32i    a3, a1, 12\n" \
        "    l32i    a4, a1, 16\n" \
        "    l32i    a5, a1, 20\n" \
        "    l32i    a6, a1, 24\n" \
        "    l32i    a7, a1, 28\n" \
        "    l32i    a8, a1, 32\n" \
        "    l32i    a9, a1, 36\n" \
        "    l32i    a10, a1, 40\n" \
        "    l32i    a11, a1, 44\n" \
        "    l32i    a12, a1, 48\n" \
        "    l32i    a13, a1, 52\n" \
        "    l32i    a14, a1, 56\n" \
        "    l32i    a15, a1, 60\n" \
        "    addi    a1, a1, 64\n" \
        \
        /* ===== RETURN FROM NMI ===== */ \
        "    rfi     7\n" \
        \
        ".size " #handler_name ", .-" #handler_name "\n" \
    )

//=============================================================================
// HELPER MACROS
//=============================================================================

/*
 * FASTLED_NMI_DECLARE_C_HANDLER
 *
 * Convenience macro to declare a C handler function with correct attributes.
 * Use this to ensure proper extern "C" linkage and IRAM placement.
 *
 * USAGE:
 *   FASTLED_NMI_DECLARE_C_HANDLER(my_nmi_handler);
 *
 * EXPANDS TO:
 *   extern "C" void FL_IRAM my_nmi_handler(void);
 */
#define FASTLED_NMI_DECLARE_C_HANDLER(function_name) \
    extern "C" void FL_IRAM function_name(void)

/*
 * FASTLED_NMI_DECLARE_FUNCTION_POINTER
 *
 * Convenience macro to declare a function pointer for dynamic dispatch.
 *
 * USAGE:
 *   FASTLED_NMI_DECLARE_FUNCTION_POINTER(g_my_handler);
 *
 * EXPANDS TO:
 *   typedef void (*_nmi_handler_t)(void);
 *   _nmi_handler_t DRAM_ATTR g_my_handler;
 */
#define FASTLED_NMI_DECLARE_FUNCTION_POINTER(pointer_name) \
    typedef void (*_##pointer_name##_t)(void); \
    _##pointer_name##_t DRAM_ATTR pointer_name

FL_EXTERN_C_END

#endif // ESP32

//=============================================================================
// COMPILATION GUARDS AND ERROR MESSAGES
//=============================================================================

#ifndef ESP32
    // If not ESP32, provide helpful error message
    #warning "ASM_2_C_SHIM.h: This file is only for ESP32 Xtensa platforms. Macros will not be available."
#endif
