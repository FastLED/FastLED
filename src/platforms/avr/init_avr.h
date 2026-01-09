// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/avr/init_avr.h
/// @brief AVR platform initialization (no-op)
///
/// AVR platforms rely primarily on Arduino core initialization.
/// No additional platform-specific initialization is required.

namespace fl {
namespace platforms {

/// @brief AVR platform initialization (no-op)
///
/// AVR platforms (ATmega, ATtiny) rely on Arduino core for initialization.
/// This function is a no-op and exists for API consistency.
inline void init() {
    // No-op: AVR platforms rely on Arduino core initialization
}

} // namespace platforms
} // namespace fl
