/*
  FastLED — Cross-Platform ISR Handler API
  -----------------------------------------
  Unified interrupt service routine attachment API that works across
  all FastLED-supported platforms: ESP32, Teensy, AVR, STM32, stub, etc.

  License: MIT (FastLED)
*/

#pragma once

#include "ftl/stdint.h"
#include "fl/compiler_control.h"

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
 * - Must be placed in fast memory (IRAM on ESP32, .text_ram on STM32, etc.)
 * - Should execute quickly (typically <10us)
 * - Cannot use blocking operations
 * - Cannot use malloc/free
 * - Platform-specific restrictions may apply (see platform docs)
 */
typedef void (*isr_handler_t)(void* user_data);

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
        , priority(1)
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
// Priority Level Recommendations
// =============================================================================

// These are abstract priority levels that map to platform-specific values
constexpr uint8_t ISR_PRIORITY_LOW        = 1;  // Lowest priority
constexpr uint8_t ISR_PRIORITY_MEDIUM     = 2;  // Medium priority
constexpr uint8_t ISR_PRIORITY_HIGH       = 3;  // High priority (recommended max)
constexpr uint8_t ISR_PRIORITY_CRITICAL   = 4;  // Critical (experimental, platform-dependent)
constexpr uint8_t ISR_PRIORITY_MAX        = 7;  // Maximum (may require assembly, platform-dependent)

// Platform-specific priority ranges:
// - ESP32 (Xtensa): 1-3 (C handlers), 4-5 (assembly required)
// - ESP32-C3 (RISC-V): 1-7 (C handlers, but 4-7 may have limitations)
// - Teensy: NVIC priority 0-255 (lower number = higher priority)
// - AVR: No hardware priority (interrupts can't nest by default)
// - STM32: NVIC priority 0-15 (configurable preemption/sub-priority)
// - Stub: All priorities treated equally

// =============================================================================
// Platform-Specific Function Declarations
// =============================================================================

// Platform implementations provide these free functions:
// Each platform (null, stub, esp32) provides its own implementation

// Null platform (default/fallback)
int null_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int null_attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle);
int null_detach_handler(isr_handle_t& handle);
int null_enable_handler(const isr_handle_t& handle);
int null_disable_handler(const isr_handle_t& handle);
bool null_is_handler_enabled(const isr_handle_t& handle);
const char* null_get_error_string(int error_code);
const char* null_get_platform_name();
uint32_t null_get_max_timer_frequency();
uint32_t null_get_min_timer_frequency();
uint8_t null_get_max_priority();
bool null_requires_assembly_handler(uint8_t priority);

// Stub platform (testing/simulation)
int stub_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int stub_attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle);
int stub_detach_handler(isr_handle_t& handle);
int stub_enable_handler(const isr_handle_t& handle);
int stub_disable_handler(const isr_handle_t& handle);
bool stub_is_handler_enabled(const isr_handle_t& handle);
const char* stub_get_error_string(int error_code);
const char* stub_get_platform_name();
uint32_t stub_get_max_timer_frequency();
uint32_t stub_get_min_timer_frequency();
uint8_t stub_get_max_priority();
bool stub_requires_assembly_handler(uint8_t priority);

// ESP32 platform
int esp32_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int esp32_attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle);
int esp32_detach_handler(isr_handle_t& handle);
int esp32_enable_handler(const isr_handle_t& handle);
int esp32_disable_handler(const isr_handle_t& handle);
bool esp32_is_handler_enabled(const isr_handle_t& handle);
const char* esp32_get_error_string(int error_code);
const char* esp32_get_platform_name();
uint32_t esp32_get_max_timer_frequency();
uint32_t esp32_get_min_timer_frequency();
uint8_t esp32_get_max_priority();
bool esp32_requires_assembly_handler(uint8_t priority);

// =============================================================================
// Cross-Platform ISR API
// =============================================================================

/**
 * Attach a timer-based ISR handler.
 *
 * @param config: ISR configuration structure
 * @param handle: Output handle for detachment (can be nullptr)
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
inline int attachTimerHandler(const isr_config_t& config, isr_handle_t* handle = nullptr) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_attach_timer_handler(config, handle);
#elif defined(ESP32)
    return esp32_attach_timer_handler(config, handle);
#else
    return null_attach_timer_handler(config, handle);
#endif
}

/**
 * Attach an external interrupt handler (GPIO-based).
 *
 * @param pin: GPIO pin number (platform-specific numbering)
 * @param config: ISR configuration structure (frequency_hz ignored)
 * @param handle: Output handle for detachment (can be nullptr)
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
inline int attachExternalHandler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle = nullptr) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_attach_external_handler(pin, config, handle);
#elif defined(ESP32)
    return esp32_attach_external_handler(pin, config, handle);
#else
    return null_attach_external_handler(pin, config, handle);
#endif
}

/**
 * Detach an ISR handler.
 *
 * @param handle: Handle returned by attachTimerHandler or attachExternalHandler
 * @return: 0 on success, negative error code on failure
 *
 * After detachment, the handle is invalidated and should not be reused.
 */
inline int detachHandler(isr_handle_t& handle) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_detach_handler(handle);
#elif defined(ESP32)
    return esp32_detach_handler(handle);
#else
    return null_detach_handler(handle);
#endif
}

/**
 * Enable an ISR (after temporary disable).
 *
 * @param handle: Handle to the ISR
 * @return: 0 on success, negative error code on failure
 */
inline int enableHandler(const isr_handle_t& handle) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_enable_handler(handle);
#elif defined(ESP32)
    return esp32_enable_handler(handle);
#else
    return null_enable_handler(handle);
#endif
}

/**
 * Disable an ISR temporarily (without detaching).
 *
 * @param handle: Handle to the ISR
 * @return: 0 on success, negative error code on failure
 */
inline int disableHandler(const isr_handle_t& handle) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_disable_handler(handle);
#elif defined(ESP32)
    return esp32_disable_handler(handle);
#else
    return null_disable_handler(handle);
#endif
}

/**
 * Query if an ISR is currently enabled.
 *
 * @param handle: Handle to the ISR
 * @return: true if enabled, false if disabled or invalid
 */
inline bool isHandlerEnabled(const isr_handle_t& handle) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_is_handler_enabled(handle);
#elif defined(ESP32)
    return esp32_is_handler_enabled(handle);
#else
    return null_is_handler_enabled(handle);
#endif
}

/**
 * Get platform-specific error description.
 *
 * @param error_code: Error code returned by attachTimerHandler/attachExternalHandler
 * @return: Human-readable error string
 */
inline const char* getErrorString(int error_code) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_get_error_string(error_code);
#elif defined(ESP32)
    return esp32_get_error_string(error_code);
#else
    return null_get_error_string(error_code);
#endif
}

// =============================================================================
// Platform Information
// =============================================================================

/**
 * Get the platform name.
 * @return: String like "ESP32", "Teensy", "AVR", "STM32", "Stub"
 */
inline const char* getPlatformName() {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_get_platform_name();
#elif defined(ESP32)
    return esp32_get_platform_name();
#else
    return null_get_platform_name();
#endif
}

/**
 * Get the maximum timer frequency supported by this platform.
 * @return: Maximum frequency in Hz, or 0 if unlimited
 */
inline uint32_t getMaxTimerFrequency() {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_get_max_timer_frequency();
#elif defined(ESP32)
    return esp32_get_max_timer_frequency();
#else
    return null_get_max_timer_frequency();
#endif
}

/**
 * Get the minimum timer frequency supported by this platform.
 * @return: Minimum frequency in Hz
 */
inline uint32_t getMinTimerFrequency() {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_get_min_timer_frequency();
#elif defined(ESP32)
    return esp32_get_min_timer_frequency();
#else
    return null_get_min_timer_frequency();
#endif
}

/**
 * Get the maximum priority level supported by this platform.
 * @return: Maximum priority value
 */
inline uint8_t getMaxPriority() {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_get_max_priority();
#elif defined(ESP32)
    return esp32_get_max_priority();
#else
    return null_get_max_priority();
#endif
}

/**
 * Check if assembly is required for a given priority level.
 * @param priority: Priority level to check
 * @return: true if assembly handler required, false if C handler allowed
 */
inline bool requiresAssemblyHandler(uint8_t priority) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_requires_assembly_handler(priority);
#elif defined(ESP32)
    return esp32_requires_assembly_handler(priority);
#else
    return null_requires_assembly_handler(priority);
#endif
}

} // namespace isr
} // namespace fl
