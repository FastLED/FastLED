// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/teensy/init_teensy4.h
/// @brief Teensy 4.x platform initialization
///
/// Teensy 4.0/4.1 platforms use the ObjectFLED system for parallel LED output
/// (up to 42 simultaneous strips). This initialization ensures the ObjectFLED
/// registry is created early for predictable behavior.

namespace fl {
namespace platforms {

/// @brief Initialize Teensy 4.x platform
///
/// Performs one-time initialization of Teensy 4.x-specific subsystems:
/// - ObjectFLED Registry: Global tracker for all ObjectFLED chipset groups
///
/// The ObjectFLED system allows up to 42 parallel LED strips with automatic
/// grouping by chipset timing. Initializing the registry early ensures
/// consistent behavior across strip instantiation order.
///
/// This function is called once during FastLED::init() and is safe to call
/// multiple times (subsequent calls are no-ops).
///
/// @note Implementation is in src/platforms/arm/teensy/init_teensy4.cpp
void init();

} // namespace platforms
} // namespace fl
