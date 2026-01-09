// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/silabs/init_silabs.h
/// @brief Silicon Labs (general) platform initialization
///
/// Silicon Labs EFM32/EFR32 platforms use lazy GPIO system initialization.
/// This provides a hook for future initialization needs.

namespace fl {
namespace platforms {

/// @brief Initialize Silicon Labs platform
///
/// Performs one-time initialization of Silicon Labs-specific subsystems.
///
/// Currently a no-op. GPIO system initialization is handled lazily on first
/// pin access in pin_silabs.hpp. This function provides a hook for future
/// initialization needs such as:
/// - Early GPIO system initialization
/// - Clock management setup
/// - Low-power peripheral configuration
///
/// This function is called once during FastLED::init() and is safe to call
/// multiple times (subsequent calls are no-ops).
inline void init() {
    // No-op: GPIO system is initialized on first pin access
    // Future: Consider early GPIO system initialization for predictability
}

} // namespace platforms
} // namespace fl
