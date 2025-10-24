/// @file fl/fastpin.h
/// Fast pin access classes - platform independent interface
///
/// This header provides the public interface for pin access classes:
/// - Selectable: Abstract interface for selectable objects
/// - Pin: Runtime pin access using Arduino functions or platform-specific
/// - OutputPin/InputPin: Pin with predefined mode
/// - FastPin<>: Compile-time pin access with platform specializations
///
/// Platform-specific implementations (FastPin<> specializations and optimized
/// Pin implementations) are provided by platforms/fastpin.h and platform-specific
/// headers like platforms/*/fastpin_*.h

#pragma once

#include "fl/compiler_control.h"
#include "led_sysdefs.h"

/// Constant for "not a pin"
/// @todo Unused, remove?
#define NO_PIN 255

// Include base class definitions (Selectable, FastPin<>, FastPinBB, __FL_PORT_INFO, etc.)
#include "fl/fastpin_base.h"

// Platform-specific implementations:
// This include handles platform detection and provides:
// - Pin class (runtime pin access) - varies by platform
// - FastPin<> specializations - platform-specific optimization
// - For stub/WASM: no-op implementations
// - For other platforms: optimized register access or Arduino PINMAP fallback
#include "platforms/fastpin.h"
