/// @file fl/pin.cpp
/// Compilation boundary for platform-independent pin API
///
/// IMPORTANT: This file acts as a compilation boundary to prevent header pollution.
///
/// Architecture:
/// - fl/pin.h: Minimal public interface (enum declarations + function signatures)
///   → Users include this and get ONLY the interface (no Arduino.h, no platform headers)
///
/// - fl/pin.cpp (this file): Compilation boundary
///   → Includes platforms/pin.h which pulls in platform-specific implementations
///   → Prevents platform headers from leaking into user code
///
/// - platforms/pin.h: Trampoline dispatcher
///   → Includes the appropriate platform .hpp file based on compile-time detection
///
/// - platforms/*/pin.hpp: Platform implementations (header-only)
///   → Provides inline/constexpr implementations for zero-overhead
///   → Split into Arduino path (#ifdef ARDUINO) and non-Arduino path
///
/// Why this matters:
/// - Users can safely `#include "fl/pin.h"` without pulling in Arduino.h
/// - Platform detection and implementation selection happens at this boundary
/// - Clean separation between interface and implementation

#include "fl/pin.h"
#include "platforms/pin.h"

namespace fl {
// All implementations are inline/constexpr in platform-specific .hpp files
// This translation unit exists solely to establish the compilation boundary
}
