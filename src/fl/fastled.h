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
#include "fl/stl/stdint.h"
#include "fl/int.h"

/// Version and configuration
#include "fl/dll.h"
#include "fl/force_inline.h"

// ============================================================================
// CONFIGURATION AND SYSTEM DEFINITIONS
// ============================================================================

/// FastLED configuration and platform definitions
#include "fastled_config.h"
#include "led_sysdefs.h"
#include "cpp_compat.h"

// ============================================================================
// CORE UTILITY HEADERS
// ============================================================================

/// Compiler and language features
#include "fl/compiler_control.h"
#include "fl/math_macros.h"

/// Memory and type utilities
#include "fl/stl/bit_cast.h"
#include "fl/stl/cstring.h"

// ============================================================================
// CORE LED TYPE DEFINITIONS
// ============================================================================

/// Basic color types (CRGB, HSV)
#include "color.h"
#include "crgb.h"
#include "chsv.h"

/// Pixel types and color ordering
#include "pixeltypes.h"
#include "eorder.h"

/// Pixel utilities
#include "dither_mode.h"
#include "pixel_controller.h"
#include "pixel_iterator.h"

// ============================================================================
// CORE CONTROLLER DEFINITIONS
// ============================================================================

/// Base LED controller classes
#include "cled_controller.h"
#include "cpixel_ledcontroller.h"
#include "controller.h"

// ============================================================================
// PLATFORM SUPPORT
// ============================================================================

/// Platform-specific definitions and initialization
#include "platforms.h"

// ============================================================================
// MISCELLANEOUS CORE UTILITIES
// ============================================================================

/// Timing utilities
#include "fastled_delay.h"

/// Math utilities from lib8tion
#include "lib8tion.h"

/// Bit manipulation
#include "bitswap.h"

/// Memory utilities
#include "fastled_progmem.h"

/// Hardware pin definitions
#include "fastpin.h"

/// Color utilities and palettes
#include "colorutils.h"
#include "colorpalettes.h"

// ============================================================================
// INTERNAL UTILITY DECLARATIONS
// ============================================================================

namespace fl {
/// Get the size of the CLEDController object for introspection
u16 cled_contoller_size();
}  // namespace fl

