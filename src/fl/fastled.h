#pragma once

/// @file fl/fastled.h
/// @brief Internal FastLED header for implementation files
///
/// This is the minimal internal header used by FastLED implementation files (.cpp and .h).
/// It provides core types, utilities, and controller definitions without the cyclic
/// dependencies that come from including the full FastLED.h header.
///
/// **User sketches should use `#include <FastLED.h>` instead.**
///
/// **Internal Implementation Files:**
/// - src/*.cpp files should use: `#include "fl/fastled.h"`
/// - src/fl/*.cpp files should use: `#include "fl/fastled.h"` (after FASTLED_INTERNAL define)
/// - Headers that need to avoid cycles should use: `#include "fl/fastled.h"`


// ============================================================================
// CORE TYPE DEFINITIONS
// ============================================================================

/// Platform-agnostic integer types
#include "fl/stl/stdint.h"  // IWYU pragma: keep
#include "fl/stl/int.h"  // IWYU pragma: keep

/// Version and configuration
#include "fl/system/dll.h"  // IWYU pragma: keep
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep

// ============================================================================
// CONFIGURATION AND SYSTEM DEFINITIONS
// ============================================================================

/// FastLED configuration and platform definitions
#include "fastled_config.h"  // IWYU pragma: keep
#include "led_sysdefs.h"  // IWYU pragma: keep
#include "cpp_compat.h"  // IWYU pragma: keep

// ============================================================================
// CORE UTILITY HEADERS
// ============================================================================

/// Compiler and language features
#include "fl/math_macros.h"  // IWYU pragma: keep

/// Memory and type utilities
#include "fl/stl/bit_cast.h"  // IWYU pragma: keep
#include "fl/stl/cstring.h"  // IWYU pragma: keep

// ============================================================================
// CORE LED TYPE DEFINITIONS
// ============================================================================

/// Basic color types (CRGB, HSV)
#include "color.h"  // IWYU pragma: keep
#include "crgb.h"  // IWYU pragma: keep
#include "chsv.h"  // IWYU pragma: keep

/// Pixel types and color ordering
#include "pixeltypes.h"  // IWYU pragma: keep
#include "eorder.h"  // IWYU pragma: keep

/// Pixel utilities
#include "dither_mode.h"  // IWYU pragma: keep
#include "pixel_controller.h"  // IWYU pragma: keep
#include "pixel_iterator.h"  // IWYU pragma: keep

// ============================================================================
// CORE CONTROLLER DEFINITIONS
// ============================================================================

/// Base LED controller classes
#include "cled_controller.h"  // IWYU pragma: keep
#include "cpixel_ledcontroller.h"  // IWYU pragma: keep
#include "controller.h"  // IWYU pragma: keep

// ============================================================================
// PLATFORM SUPPORT
// ============================================================================

/// Platform-specific definitions and initialization
#include "platforms.h"  // IWYU pragma: keep

// ============================================================================
// MISCELLANEOUS CORE UTILITIES
// ============================================================================

/// Timing utilities
#include "fastled_delay.h"  // IWYU pragma: keep

/// Math utilities from lib8tion
#include "lib8tion.h"  // IWYU pragma: keep

/// Bit manipulation
#include "bitswap.h"  // IWYU pragma: keep

/// Memory utilities
#include "fastled_progmem.h"  // IWYU pragma: keep

/// Hardware pin definitions
#include "fastpin.h"  // IWYU pragma: keep

/// Color utilities and palettes
#include "colorutils.h"  // IWYU pragma: keep
#include "colorpalettes.h"  // IWYU pragma: keep

// ============================================================================
// INTERNAL UTILITY DECLARATIONS
// ============================================================================

namespace fl {
/// Get the size of the CLEDController object for introspection
u16 cled_contoller_size();
}  // namespace fl

