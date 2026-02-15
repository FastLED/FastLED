// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/condition_variable.h
/// @brief Platform dispatch for condition variable support
///
/// This header provides platform-specific condition variable implementations in fl::platforms::
/// namespace. It follows the coarse-to-fine delegation pattern, routing to
/// platform-specific headers based on platform detection.
///
/// Supported platforms:
/// - ESP32: FreeRTOS condition variable implementation
/// - Stub: std::condition_variable wrapper or fake CV based on pthread availability
/// - Other platforms: Use stub implementation as fallback
///
/// This header is included from fl/stl/condition_variable.h. The public API fl::condition_variable
/// in fl/stl/condition_variable.h binds to fl::platforms::condition_variable.

#include "platforms/is_platform.h"

// Platform dispatch
#ifdef FL_IS_ESP32
    #include "platforms/esp/32/condition_variable_esp32.h"
#elif defined(FL_IS_STUB) || defined(FL_IS_WASM)
    // WASM uses stub condition_variable profile (std::condition_variable with pthread support)
    #include "platforms/stub/condition_variable_stub.h"
#else
    // Default fallback: Use stub implementation for all platforms
    // TODO: Add platform-specific implementations here as needed
    #include "platforms/stub/condition_variable_stub.h"
#endif

#ifndef FASTLED_MULTITHREADED
#error "Expected FASTLED_MULTITHREADED to be defined by platform condition_variable implementation"
#endif
