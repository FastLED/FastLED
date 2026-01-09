// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/renesas/init_renesas.h
/// @brief Renesas platform initialization
///
/// Renesas RA4M1 platforms (Arduino UNO R4 Minima/WiFi) have minimal
/// initialization needs. This provides a hook for future requirements.

namespace fl {
namespace platforms {

/// @brief Initialize Renesas platform
///
/// Performs one-time initialization of Renesas-specific subsystems.
///
/// Currently a no-op. Renesas RA4M1 platforms (Arduino UNO R4) rely on
/// Arduino core for most initialization. This function provides a hook for
/// future initialization needs such as:
/// - Early peripheral setup
/// - Clock configuration
/// - WiFi module initialization (UNO R4 WiFi)
///
/// This function is called once during FastLED::init() and is safe to call
/// multiple times (subsequent calls are no-ops).
inline void init() {
    // No-op: Renesas platforms rely on Arduino core initialization
    // Future: Consider WiFi module or peripheral initialization for UNO R4
}

} // namespace platforms
} // namespace fl
