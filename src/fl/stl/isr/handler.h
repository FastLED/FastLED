/// @file fl/stl/isr/handler.h
/// @brief ISR handler types and API declarations

#pragma once

#include "fl/stl/isr/constants.h"
#include "fl/compiler_control.h"
#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"

namespace fl {
namespace isr {

/// ISR handler function signature.
typedef void (*handler_fn)(void* user_data);

/// Configuration for ISR attachment.
struct config {
    handler_fn handler;           // Handler function pointer
    void* user_data;              // User context (passed to handler)
    u32 frequency_hz;             // Timer frequency in Hz (0 = GPIO/external interrupt)
    u8 priority;                  // Priority level (platform-dependent range)
    u32 flags;                    // Platform-specific flags (see constants.h)

    config()
        : handler(nullptr)
        , user_data(nullptr)
        , frequency_hz(0)
        , priority(ISR_PRIORITY_DEFAULT)
        , flags(0)
    {}
};

/// Opaque handle to an attached ISR.
struct handle {
    void* platform_handle;        // Platform-specific handle
    handler_fn handler;           // Handler function (for validation)
    void* user_data;              // User data (for validation)
    u8 platform_id;               // Platform identifier (for runtime checks)

    handle()
        : platform_handle(nullptr)
        , handler(nullptr)
        , user_data(nullptr)
        , platform_id(0)
    {}

    bool is_valid() const { return platform_handle != nullptr; }
};

// Backward-compatible type aliases
using isr_handler_t = handler_fn;
using isr_config_t = config;
using isr_handle_t = handle;

/// Attach a timer-based ISR handler.
int attach_timer_handler(const config& cfg, handle* out_handle = nullptr);

/// Attach an external interrupt handler (GPIO-based).
int attach_external_handler(u8 pin, const config& cfg, handle* out_handle = nullptr);

/// Detach an ISR handler.
int detach_handler(handle& h);

/// Enable an ISR (after temporary disable).
int enable_handler(handle& h);

/// Disable an ISR temporarily (without detaching).
int disable_handler(handle& h);

/// Query if an ISR is currently enabled.
bool is_handler_enabled(const handle& h);

/// Get platform-specific error description.
const char* get_error_string(int error_code);

/// Get the platform name.
const char* get_platform_name();

/// Get the maximum timer frequency supported by this platform.
u32 get_max_timer_frequency();

/// Get the minimum timer frequency supported by this platform.
u32 get_min_timer_frequency();

/// Get the maximum priority level supported by this platform.
u8 get_max_priority();

/// Check if assembly is required for a given priority level.
bool requires_assembly_handler(u8 priority);

} // namespace isr
} // namespace fl
