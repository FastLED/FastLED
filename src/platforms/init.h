// ok no namespace fl
#pragma once

/// @file platforms/init.h
/// @brief Platform dispatch for one-time initialization
///
/// This header provides platform-specific initialization functions in fl::platforms::
/// namespace. It follows the coarse-to-fine delegation pattern, routing to
/// platform-specific headers based on platform detection.
///
/// The initialization function is called once during FastLED::init() to perform
/// any platform-specific setup required before LED operations begin.
///
/// Supported platforms:
/// - ESP32: Initialize channel bus manager and SPI system
/// - Other platforms: Use stub implementation (no-op)
///
/// This header is included from FastLED.h and called during library initialization.

#include "platforms/is_platform.h"

// Platform dispatch
#ifdef FL_IS_ESP32
    #include "platforms/esp/32/init_esp32.h"
#else
    // Default fallback: Use stub implementation for all platforms
    #include "platforms/stub/init_stub.h"
#endif
