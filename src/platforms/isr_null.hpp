/*
  FastLED — Null ISR Implementation (Weak Symbols)
  ------------------------------------------------
  Default null implementation of the cross-platform ISR API using weak symbols.
  This provides a fallback when no platform-specific implementation is available.

  Platform-specific implementations (ESP32, Teensy, AVR, etc.) provide strong
  symbol overrides that replace these weak defaults at link time.

  License: MIT (FastLED)
*/

#pragma once

#include "fl/isr.h"
#include "fl/compiler_control.h"

namespace fl {
namespace isr {

// Platform ID for null implementation
constexpr uint8_t NULL_PLATFORM_ID = 255;  // Indicates "no platform"

// Error code for not implemented
constexpr int ERR_NOT_IMPLEMENTED = -100;

// =============================================================================
// Null ISR Implementation (Free Functions)
// =============================================================================

/**
 * Null implementation that provides safe no-op defaults.
 * Used when no platform-specific ISR implementation is available.
 */

inline int null_attach_timer_handler(const isr_config_t& config, isr_handle_t* out_handle) {
    (void)config;
    if (out_handle) {
        *out_handle = isr_handle_t();  // Invalid handle
    }
    return ERR_NOT_IMPLEMENTED;  // Not implemented error
}

inline int null_attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) {
    (void)pin;
    (void)config;
    if (out_handle) {
        *out_handle = isr_handle_t();  // Invalid handle
    }
    return ERR_NOT_IMPLEMENTED;  // Not implemented error
}

inline int null_detach_handler(isr_handle_t& handle) {
    handle = isr_handle_t();  // Invalidate handle
    return ERR_NOT_IMPLEMENTED;  // Not implemented error
}

inline int null_enable_handler(isr_handle_t& handle) {
    (void)handle;
    return ERR_NOT_IMPLEMENTED;  // Not implemented error
}

inline int null_disable_handler(isr_handle_t& handle) {
    (void)handle;
    return ERR_NOT_IMPLEMENTED;  // Not implemented error
}

inline bool null_is_handler_enabled(const isr_handle_t& handle) {
    (void)handle;
    return false;
}

inline const char* null_get_error_string(int error_code) {
    switch (error_code) {
        case 0: return "Success";
        case ERR_NOT_IMPLEMENTED: return "Not implemented (no platform ISR support)";
        default: return "Unknown error";
    }
}

inline const char* null_get_platform_name() {
    return "Null";
}

inline uint32_t null_get_max_timer_frequency() {
    return 0;  // No timer support
}

inline uint32_t null_get_min_timer_frequency() {
    return 0;  // No timer support
}

inline uint8_t null_get_max_priority() {
    return 0;  // No priority support
}

inline bool null_requires_assembly_handler(uint8_t priority) {
    (void)priority;
    return false;
}

} // namespace isr

// fl::isr::platform namespace wrappers (call fl::isr:: functions)
// The dispatch header (platforms/isr.h) controls when this file is included,
// so we unconditionally define the platform namespace here.
namespace isr {
namespace platform {

inline int attach_timer_handler(const isr_config_t& config, isr_handle_t* handle) {
    return null_attach_timer_handler(config, handle);
}

inline int attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle) {
    return null_attach_external_handler(pin, config, handle);
}

inline int detach_handler(isr_handle_t& handle) {
    return null_detach_handler(handle);
}

inline int enable_handler(isr_handle_t& handle) {
    return null_enable_handler(handle);
}

inline int disable_handler(isr_handle_t& handle) {
    return null_disable_handler(handle);
}

inline bool is_handler_enabled(const isr_handle_t& handle) {
    return null_is_handler_enabled(handle);
}

inline const char* get_error_string(int error_code) {
    return null_get_error_string(error_code);
}

inline const char* get_platform_name() {
    return null_get_platform_name();
}

inline uint32_t get_max_timer_frequency() {
    return null_get_max_timer_frequency();
}

inline uint32_t get_min_timer_frequency() {
    return null_get_min_timer_frequency();
}

inline uint8_t get_max_priority() {
    return null_get_max_priority();
}

inline bool requires_assembly_handler(uint8_t priority) {
    return null_requires_assembly_handler(priority);
}

} // namespace platform
} // namespace isr

// =============================================================================
// Global Interrupt Control (noInterrupts/interrupts)
// =============================================================================

// Only define if not already provided by a platform-specific header
// (e.g., isr_avr.hpp defines these for all AVR including ATtiny)
#ifndef FL_ISR_GLOBAL_INTERRUPTS_DEFINED

/// No-op for null/unsupported platform
inline void interruptsDisable() {
    // No-op: platform doesn't have ISR support
}

/// No-op for null/unsupported platform
inline void interruptsEnable() {
    // No-op: platform doesn't have ISR support
}

#endif // FL_ISR_GLOBAL_INTERRUPTS_DEFINED

} // namespace fl
