// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/mgm240/init_mgm240.h
/// @brief Silicon Labs MGM240 platform initialization
///
/// MGM240 (EFR32MG24) platforms (Arduino Nano Matter, SparkFun Thing Plus Matter)
/// use lazy GPIO clock initialization. This provides a hook for future needs.

namespace fl {
namespace platforms {

/// @brief Initialize MGM240 platform
///
/// Performs one-time initialization of MGM240-specific subsystems.
///
/// Currently a no-op. GPIO clock initialization is handled lazily on first
/// pin access in fastpin_arm_mgm240.cpp. This function provides a hook for
/// future initialization needs such as:
/// - Early GPIO clock initialization
/// - Radio coexistence setup
/// - Low-power mode configuration
///
/// This function is called once during FastLED::init() and is safe to call
/// multiple times (subsequent calls are no-ops).
inline void init() {
    // No-op: GPIO clocks are initialized on first pin access
    // Future: Consider early GPIO clock initialization for predictability
}

} // namespace platforms
} // namespace fl
