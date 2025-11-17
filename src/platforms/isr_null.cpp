/*
  FastLED — Null ISR Implementation (Weak Symbols)
  ------------------------------------------------
  Default null implementation of the cross-platform ISR API using weak symbols.
  This provides a fallback when no platform-specific implementation is available.

  Platform-specific implementations (ESP32, Teensy, AVR, etc.) provide strong
  symbol overrides that replace these weak defaults at link time.

  License: MIT (FastLED)
*/

#include "fl/isr.h"
#include "fl/compiler_control.h"
// Get ESP-IDF version macros if on ESP32
#if defined(ESP32)
    #include "platforms/esp/32/core/led_sysdefs_esp32.h"
#endif

namespace fl {
namespace isr {

// =============================================================================
// Null ISR Implementation (Free Functions)
// =============================================================================

/**
 * Null implementation that provides safe no-op defaults.
 * Used when no platform-specific ISR implementation is available.
 */

int null_attach_timer_handler(const isr_config_t& config, isr_handle_t* out_handle) {
    (void)config;
    if (out_handle) {
        *out_handle = isr_handle_t();  // Invalid handle
    }
    return -100;  // Not implemented error
}

int null_attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) {
    (void)pin;
    (void)config;
    if (out_handle) {
        *out_handle = isr_handle_t();  // Invalid handle
    }
    return -100;  // Not implemented error
}

int null_detach_handler(isr_handle_t& handle) {
    handle = isr_handle_t();  // Invalidate handle
    return -100;  // Not implemented error
}

int null_enable_handler(const isr_handle_t& handle) {
    (void)handle;
    return -100;  // Not implemented error
}

int null_disable_handler(const isr_handle_t& handle) {
    (void)handle;
    return -100;  // Not implemented error
}

bool null_is_handler_enabled(const isr_handle_t& handle) {
    (void)handle;
    return false;
}

const char* null_get_error_string(int error_code) {
    switch (error_code) {
        case 0: return "Success";
        case -100: return "Not implemented (no platform ISR support)";
        default: return "Unknown error";
    }
}

const char* null_get_platform_name() {
    return "Null";
}

uint32_t null_get_max_timer_frequency() {
    return 0;  // No timer support
}

uint32_t null_get_min_timer_frequency() {
    return 0;  // No timer support
}

uint8_t null_get_max_priority() {
    return 0;  // No priority support
}

bool null_requires_assembly_handler(uint8_t priority) {
    (void)priority;
    return false;
}

} // namespace isr
} // namespace fl
