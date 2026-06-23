/*
  FastLED — SAM ISR Implementation (Null/Stub)
  ---------------------------------------------
  Null implementation of the cross-platform ISR API for Atmel SAM platforms.
  This provides a safe no-op fallback for Arduino Due (SAM3X8E).

  Note: Full ISR support for SAM can be implemented in the future if needed.

  License: MIT (FastLED)
*/

#pragma once

// IWYU pragma: private

#include "fl/stl/isr/handler.h"
#include "fl/stl/compiler_control.h"
#include "platforms/arm/sam/is_sam.h"
#include "fl/stl/noexcept.h"

// Platform guard - only compile for SAM platforms
#if defined(FL_IS_SAM)

namespace fl {
namespace isr {

// Platform ID for SAM
constexpr u8 SAM_PLATFORM_ID = 10;

// Error code for not implemented
constexpr int ERR_NOT_IMPLEMENTED = -100;

// =============================================================================
// SAM ISR Implementation (Null - Free Functions)
// =============================================================================

/**
 * Null implementation that provides safe no-op defaults for SAM.
 * Used as a placeholder until full ISR support is implemented.
 */

int sam_attach_timer_handler(const isr_config_t& config, isr_handle_t* out_handle) FL_NOEXCEPT {
    (void)config;
    if (out_handle) {
        *out_handle = isr_handle_t();  // Invalid handle
    }
    return ERR_NOT_IMPLEMENTED;  // Not implemented error
}

int sam_attach_external_handler(u8 pin, const isr_config_t& config, isr_handle_t* out_handle) FL_NOEXCEPT {
    (void)pin;
    (void)config;
    if (out_handle) {
        *out_handle = isr_handle_t();  // Invalid handle
    }
    return ERR_NOT_IMPLEMENTED;  // Not implemented error
}

int sam_detach_handler(isr_handle_t& handle) FL_NOEXCEPT {
    handle = isr_handle_t();  // Invalidate handle
    return ERR_NOT_IMPLEMENTED;  // Not implemented error
}

int sam_enable_handler(isr_handle_t& handle) FL_NOEXCEPT {
    (void)handle;
    return ERR_NOT_IMPLEMENTED;  // Not implemented error
}

int sam_disable_handler(isr_handle_t& handle) FL_NOEXCEPT {
    (void)handle;
    return ERR_NOT_IMPLEMENTED;  // Not implemented error
}

bool sam_is_handler_enabled(const isr_handle_t& handle) FL_NOEXCEPT {
    (void)handle;
    return false;
}

const char* sam_get_error_string(int error_code) FL_NOEXCEPT {
    switch (error_code) {
        case 0: return "Success";
        case ERR_NOT_IMPLEMENTED: return "Not implemented (SAM ISR support not yet available)";
        default: return "Unknown error";
    }
}

const char* sam_get_platform_name() FL_NOEXCEPT {
    return "SAM";
}

u32 sam_get_max_timer_frequency() FL_NOEXCEPT {
    return 0;  // No timer support yet
}

u32 sam_get_min_timer_frequency() FL_NOEXCEPT {
    return 0;  // No timer support yet
}

u8 sam_get_max_priority() FL_NOEXCEPT {
    return 0;  // No priority support yet
}

bool sam_requires_assembly_handler(u8 priority) FL_NOEXCEPT {
    (void)priority;
    return false;
}

// fl::isr::platform namespace wrappers
namespace platforms {

int attach_timer_handler(const isr_config_t& config, isr_handle_t* handle) FL_NOEXCEPT {
    return sam_attach_timer_handler(config, handle);
}

int attach_external_handler(u8 pin, const isr_config_t& config, isr_handle_t* handle) FL_NOEXCEPT {
    return sam_attach_external_handler(pin, config, handle);
}

int detach_handler(isr_handle_t& handle) FL_NOEXCEPT {
    return sam_detach_handler(handle);
}

int enable_handler(isr_handle_t& handle) FL_NOEXCEPT {
    return sam_enable_handler(handle);
}

int disable_handler(isr_handle_t& handle) FL_NOEXCEPT {
    return sam_disable_handler(handle);
}

bool is_handler_enabled(const isr_handle_t& handle) FL_NOEXCEPT {
    return sam_is_handler_enabled(handle);
}

const char* get_error_string(int error_code) FL_NOEXCEPT {
    return sam_get_error_string(error_code);
}

const char* get_platform_name() FL_NOEXCEPT {
    return sam_get_platform_name();
}

u32 get_max_timer_frequency() FL_NOEXCEPT {
    return sam_get_max_timer_frequency();
}

u32 get_min_timer_frequency() FL_NOEXCEPT {
    return sam_get_min_timer_frequency();
}

u8 get_max_priority() FL_NOEXCEPT {
    return sam_get_max_priority();
}

bool requires_assembly_handler(u8 priority) FL_NOEXCEPT {
    return sam_requires_assembly_handler(priority);
}

} // namespace platforms
} // namespace isr

// =============================================================================
// Global Interrupt Control (noInterrupts/interrupts)
// =============================================================================

/// Disable interrupts on ARM Cortex-M (SAM)
inline void interruptsDisable() FL_NOEXCEPT {
    __asm__ __volatile__("cpsid i" ::: "memory") FL_NOEXCEPT;
}

/// Enable interrupts on ARM Cortex-M (SAM)
inline void interruptsEnable() FL_NOEXCEPT {
    __asm__ __volatile__("cpsie i" ::: "memory") FL_NOEXCEPT;
}

} // namespace fl

#endif // FL_IS_SAM
