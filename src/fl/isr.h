/*
  FastLED — Cross-Platform ISR Handler API
  -----------------------------------------
  Unified interrupt service routine attachment API that works across
  all FastLED-supported platforms: ESP32, Teensy, AVR, STM32, stub, etc.

  License: MIT (FastLED)
*/

#pragma once

// allow-include-after-namespace

#include "fl/compiler_control.h"
#include "fl/force_inline.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"

namespace fl {
namespace isr {

// =============================================================================
// ISR Handler Function Type
// =============================================================================

/**
 * ISR handler function signature.
 *
 * @param user_data: User-provided context pointer (can be nullptr)
 *
 * Requirements:
 * - Must be placed in fast memory using the FL_IRAM attribute (see fl/compiler_control.h)
 *    - ESP32/ESP8266: Places code in IRAM (internal SRAM)
 *    - STM32: Places code in .text_ram section
 *    - Other platforms: No-op (functions execute from normal memory)
 * - Should execute quickly (typically <10us)
 * - Cannot use blocking operations
 * - Cannot use malloc/free
 * - Platform-specific restrictions may apply (see platform docs)
 *
 * Example:
 *   void FL_IRAM my_timer_isr(void* user_data) {
 *       // Fast ISR handler code here
 *   }
 */
typedef void (*isr_handler_t)(void* user_data);

// =============================================================================
// Priority Level Constants (Forward Declaration)
// =============================================================================

// Abstract priority levels (platform-independent, higher value = higher priority)
// Platforms map these to their native priority schemes internally
// These are abstract priority levels that map to platform-specific values
// Declared here so they can be used in isr_config_t constructor
constexpr uint8_t ISR_PRIORITY_LOW        = 1;  // Lowest priority
constexpr uint8_t ISR_PRIORITY_DEFAULT    = 1;  // Default priority (alias for LOW)
constexpr uint8_t ISR_PRIORITY_MEDIUM     = 2;  // Medium priority
constexpr uint8_t ISR_PRIORITY_HIGH       = 3;  // High priority (recommended max)
constexpr uint8_t ISR_PRIORITY_CRITICAL   = 4;  // Critical (experimental, platform-dependent)
constexpr uint8_t ISR_PRIORITY_MAX        = 7;  // Maximum (may require assembly, platform-dependent)

// =============================================================================
// ISR Configuration Structure
// =============================================================================

/**
 * Configuration for ISR attachment.
 *
 * This structure provides platform-independent ISR configuration while
 * allowing platform-specific optimizations through the flags field.
 */
struct isr_config_t {
    isr_handler_t handler;        // Handler function pointer
    void* user_data;              // User context (passed to handler)
    uint32_t frequency_hz;        // Timer frequency in Hz (0 = GPIO/external interrupt)
    uint8_t priority;             // Priority level (platform-dependent range)
    uint32_t flags;               // Platform-specific flags (see below)

    // Constructor with defaults
    isr_config_t()
        : handler(nullptr)
        , user_data(nullptr)
        , frequency_hz(0)
        , priority(ISR_PRIORITY_DEFAULT)
        , flags(0)
    {}
};

// =============================================================================
// ISR Configuration Flags (Platform-Specific)
// =============================================================================

// Common flags (applicable to most platforms)
constexpr uint32_t ISR_FLAG_IRAM_SAFE     = (1u << 0);  // Place in IRAM (ESP32)
constexpr uint32_t ISR_FLAG_EDGE_RISING   = (1u << 1);  // Edge-triggered, rising edge
constexpr uint32_t ISR_FLAG_EDGE_FALLING  = (1u << 2);  // Edge-triggered, falling edge
constexpr uint32_t ISR_FLAG_LEVEL_HIGH    = (1u << 3);  // Level-triggered, high level
constexpr uint32_t ISR_FLAG_LEVEL_LOW     = (1u << 4);  // Level-triggered, low level
constexpr uint32_t ISR_FLAG_ONE_SHOT      = (1u << 5);  // One-shot timer (vs auto-reload)
constexpr uint32_t ISR_FLAG_MANUAL_TICK   = (1u << 6);  // Manual tick mode (simulation/testing)

// ESP32-specific flags
constexpr uint32_t ISR_FLAG_ESP_SHARED    = (1ul << 16); // Shared interrupt (ESP32)
constexpr uint32_t ISR_FLAG_ESP_LOWMED    = (1ul << 17); // Low/medium priority shared (ESP32)

// STM32-specific flags
constexpr uint32_t ISR_FLAG_STM32_PREEMPT = (1ul << 18); // Use preemption priority (STM32)

// =============================================================================
// ISR Handle Type
// =============================================================================

/**
 * Opaque handle to an attached ISR.
 * Platform implementations store their native handle here.
 */
struct isr_handle_t {
    void* platform_handle;        // Platform-specific handle
    isr_handler_t handler;        // Handler function (for validation)
    void* user_data;              // User data (for validation)
    uint8_t platform_id;          // Platform identifier (for runtime checks)

    isr_handle_t()
        : platform_handle(nullptr)
        , handler(nullptr)
        , user_data(nullptr)
        , platform_id(0)
    {}

    // Check if handle is valid
    bool is_valid() const { return platform_handle != nullptr; }
};

// =============================================================================
// Priority Level Documentation
// =============================================================================

// Priority constants are declared earlier in the file (before isr_config_t)
// Platform-specific priority ranges:
// - ESP32 (Xtensa): 1-3 (C handlers), 4-5 (assembly required)
// - ESP32-C3 (RISC-V): 1-7 (C handlers, but 4-7 may have limitations)
// - Teensy: NVIC priority 0-255 (lower number = higher priority)
// - AVR: No hardware priority (interrupts can't nest by default)
// - STM32: NVIC priority 0-15 (configurable preemption/sub-priority)
// - Stub: All priorities treated equally

// =============================================================================
// Cross-Platform ISR API
// =============================================================================

/**
 * Attach a timer-based ISR handler.
 *
 * @param config: ISR configuration structure
 * @param handle: Output handle for detachment (can be nullptr if handle not needed)
 * @return: 0 on success, negative error code on failure
 *
 * Example (ESP32):
 *   isr_config_t cfg;
 *   cfg.handler = my_isr_handler;
 *   cfg.user_data = &my_context;
 *   cfg.frequency_hz = 1000000; // 1 MHz timer
 *   cfg.priority = ISR_PRIORITY_HIGH;
 *   cfg.flags = ISR_FLAG_IRAM_SAFE;
 *
 *   isr_handle_t handle;
 *   int result = isr::attachTimerHandler(cfg, &handle);
 *
 * Platform-specific notes:
 * - ESP32: Uses gptimer, supports frequencies 1 Hz - 80 MHz
 * - Teensy: Uses IntervalTimer, supports up to 150kHz typical
 * - AVR: Uses Timer1, frequency depends on prescaler settings
 * - STM32: Uses hardware timer, frequency depends on system clock
 * - Stub: Uses software simulation, unlimited frequency
 */
int attachTimerHandler(const isr_config_t& config, isr_handle_t* handle = nullptr);

/**
 * Attach an external interrupt handler (GPIO-based).
 *
 * @param pin: GPIO pin number (platform-specific numbering)
 * @param config: ISR configuration structure (frequency_hz ignored)
 * @param handle: Output handle for detachment (can be nullptr if handle not needed)
 * @return: 0 on success, negative error code on failure
 *
 * Example:
 *   isr_config_t cfg;
 *   cfg.handler = my_gpio_handler;
 *   cfg.flags = ISR_FLAG_EDGE_RISING;
 *   cfg.priority = ISR_PRIORITY_MEDIUM;
 *
 *   int result = isr::attachExternalHandler(2, cfg);
 *
 * Platform-specific notes:
 * - ESP32: Uses esp_intr_alloc with GPIO interrupt
 * - Teensy: Uses attachInterrupt()
 * - AVR: Uses attachInterrupt() or direct register manipulation
 * - STM32: Uses HAL_NVIC_SetPriority and EXTI configuration
 * - Stub: Simulates GPIO events
 */
int attachExternalHandler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle = nullptr);

/**
 * Detach an ISR handler.
 *
 * @param handle: Handle returned by attachTimerHandler or attachExternalHandler
 * @return: 0 on success, negative error code on failure
 *
 * After detachment, the handle is invalidated and should not be reused.
 */
int detachHandler(isr_handle_t& handle);

/**
 * Enable an ISR (after temporary disable).
 *
 * @param handle: Handle to the ISR
 * @return: 0 on success, negative error code on failure
 */
int enableHandler(isr_handle_t& handle);

/**
 * Disable an ISR temporarily (without detaching).
 *
 * @param handle: Handle to the ISR
 * @return: 0 on success, negative error code on failure
 */
int disableHandler(isr_handle_t& handle);

/**
 * Query if an ISR is currently enabled.
 *
 * @param handle: Handle to the ISR
 * @return: true if enabled, false if disabled or invalid
 */
bool isHandlerEnabled(const isr_handle_t& handle);

/**
 * Get platform-specific error description.
 *
 * @param error_code: Error code returned by attachTimerHandler/attachExternalHandler
 * @return: Human-readable error string
 */
const char* getErrorString(int error_code);

// =============================================================================
// Platform Information
// =============================================================================

/**
 * Get the platform name.
 * @return: String like "ESP32", "Teensy", "AVR", "STM32", "Stub"
 */
const char* getPlatformName();

/**
 * Get the maximum timer frequency supported by this platform.
 * @return: Maximum frequency in Hz, or 0 if unlimited
 */
uint32_t getMaxTimerFrequency();

/**
 * Get the minimum timer frequency supported by this platform.
 * @return: Minimum frequency in Hz
 */
uint32_t getMinTimerFrequency();

/**
 * Get the maximum priority level supported by this platform.
 * @return: Maximum priority value
 */
uint8_t getMaxPriority();

/**
 * Check if assembly is required for a given priority level.
 * @param priority: Priority level to check
 * @return: true if assembly handler required, false if C handler allowed
 */
bool requiresAssemblyHandler(uint8_t priority);

// =============================================================================
// ISR-Optimized Memory Copy Utilities
// =============================================================================

/// @brief Check if a pointer is aligned to a specific byte boundary
/// @param ptr Pointer to check
/// @param alignment Alignment requirement in bytes (must be power of 2)
/// @return true if aligned, false otherwise
FASTLED_FORCE_INLINE bool is_aligned(const void* ptr, size_t alignment) {
    return (fl::ptr_to_int(ptr) & (alignment - 1)) == 0;
}

/// @brief ISR-optimized 32-bit block copy for 4-byte aligned memory
/// @param dst Destination pointer (must be 4-byte aligned)
/// @param src Source pointer (must be 4-byte aligned)
/// @param count Number of 32-bit words to copy (NOT bytes)
/// @note Only call with 4-byte aligned pointers and valid count
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memcpy32(uint32_t* FL_RESTRICT_PARAM dst,
              const uint32_t* FL_RESTRICT_PARAM src,
              size_t count) {
    // Optimized 32-bit word copy - compiler will often vectorize this
    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

/// @brief ISR-optimized 16-bit block copy for 2-byte aligned memory
/// @param dst Destination pointer (must be 2-byte aligned)
/// @param src Source pointer (must be 2-byte aligned)
/// @param count Number of 16-bit words to copy (NOT bytes)
/// @note Only call with 2-byte aligned pointers and valid count
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memcpy16(uint16_t* FL_RESTRICT_PARAM dst,
              const uint16_t* FL_RESTRICT_PARAM src,
              size_t count) {
    // Optimized 16-bit word copy
    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

/// @brief ISR-optimized byte copy
/// @param dst Destination pointer
/// @param src Source pointer
/// @param count Number of bytes to copy
/// @note No alignment requirements
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memcpybyte(uint8_t* FL_RESTRICT_PARAM dst,
                const uint8_t* FL_RESTRICT_PARAM src,
                size_t count) {
    // Byte-by-byte copy
    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

/// @brief ISR-optimized memcpy with alignment detection and switch dispatch
/// @param dst Destination pointer
/// @param src Source pointer
/// @param num_bytes Number of bytes to copy
/// @note Automatically uses 32-bit, 16-bit, or byte copy based on alignment
/// @note Uses switch for dispatch (compiler may optimize to jump table)
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memcpy(void* FL_RESTRICT_PARAM dst,
            const void* FL_RESTRICT_PARAM src,
            size_t num_bytes) {
    // Branchless index calculation (but switch still has branches):
    // - Both 4-byte aligned AND size multiple of 4: index 2 (memcpy32)
    // - Both 2-byte aligned AND size multiple of 2: index 1 (memcpy16)
    // - Otherwise: index 0 (memcpybyte)

    uintptr_t dst_addr = fl::ptr_to_int(dst);
    uintptr_t src_addr = fl::ptr_to_int(src);

    // Branchless: Convert boolean to integer (0 or 1) using bitwise ops
    // Check if all are 2-byte aligned (LSB is 0)
    size_t align2 = (((dst_addr | src_addr | num_bytes) & 1) == 0);
    // Check if all are 4-byte aligned (lower 2 bits are 0)
    size_t align4 = (((dst_addr | src_addr | num_bytes) & 3) == 0);

    // Branchless index calculation using arithmetic
    int index = align2 + align4;

    // Switch dispatch (compiler optimizes to jump table on most platforms)
    // Note: This introduces a branch, but it's a predicted indirect jump
    switch (index) {
        case 2:
            memcpy32(static_cast<uint32_t*>(dst),
                     static_cast<const uint32_t*>(src),
                     num_bytes >> 2);
            break;
        case 1:
            memcpy16(static_cast<uint16_t*>(dst),
                     static_cast<const uint16_t*>(src),
                     num_bytes >> 1);
            break;
        case 0:
        default:
            memcpybyte(static_cast<uint8_t*>(dst),
                       static_cast<const uint8_t*>(src),
                       num_bytes);
            break;
    }
}

// =============================================================================
// ISR-Safe Memory Zero Utilities
// =============================================================================

/// @brief ISR-safe memset replacement (byte-by-byte zero)
/// @param dest Destination pointer
/// @param count Number of bytes to zero
/// @note fl::memset is not allowed in ISR context on some platforms
/// @note This function uses a simple loop to zero memory
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memset_zero_byte(uint8_t* dest, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dest[i] = 0x0;
    }
}

/// @brief ISR-safe word-aligned memset (4-byte writes)
/// @param dest Destination pointer (must be 4-byte aligned)
/// @param count Number of bytes to zero (will process in 4-byte chunks)
/// @note Handles remainder bytes using byte writes
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memset_zero_word(uint8_t* dest, size_t count) {
    uint32_t* dest32 = fl::bit_cast<uint32_t*>(dest);
    size_t count32 = count / 4;
    size_t remainder = count % 4;

    for (size_t i = 0; i < count32; i++) {
        dest32[i] = 0;
    }

    if (remainder > 0) {
        uint8_t* remainder_ptr = dest + (count32 * 4);
        memset_zero_byte(remainder_ptr, remainder);
    }
}

/// @brief ISR-safe memset with alignment optimization
/// @param dest Destination pointer
/// @param count Number of bytes to zero
/// @note Automatically selects word or byte writes based on alignment
FL_OPTIMIZE_FUNCTION FL_IRAM FASTLED_FORCE_INLINE
void memset_zero(uint8_t* dest, size_t count) {
    uintptr_t address = fl::ptr_to_int(dest);

    // If aligned AND large enough, use fast word writes
    if ((address % 4 == 0) && (count >= 4)) {
        memset_zero_word(dest, count);
    } else {
        // Unaligned or small: use byte writes
        memset_zero_byte(dest, count);
    }
}


// =============================================================================
// CriticalSection RAII Helper
// =============================================================================

/// @brief RAII helper for critical sections (interrupt disable/enable)
/// Automatically disables interrupts on construction and enables on destruction
/// Use this for protecting shared data accessed from both ISR and main context
///
/// Example:
///   {
///       CriticalSection cs;  // Interrupts disabled here
///       shared_data = new_value;
///   }  // Interrupts automatically re-enabled here
class CriticalSection {
public:
    CriticalSection();
    ~CriticalSection();
    // Non-copyable
    CriticalSection(const CriticalSection&) = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;
};

} // namespace isr

} // namespace fl

// =============================================================================
// Platform-Specific ISR Implementation (includes noInterrupts/interrupts)
// =============================================================================

// Include platform-specific ISR implementation after API declaration
// This provides both ISR handler functions AND interrupt control functions
