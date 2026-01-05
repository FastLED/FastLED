// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/mutex.h
/// @brief Platform dispatch for mutex support
///
/// This header provides platform-specific mutex implementations in fl::platforms::
/// namespace. It follows the coarse-to-fine delegation pattern, routing to
/// platform-specific headers based on platform detection.
///
/// Supported platforms:
/// - ESP32: FreeRTOS mutex wrappers
/// - Stub: std::mutex wrapper or fake mutex based on pthread availability
/// - Other platforms: Use stub implementation as fallback
///
/// This header is included from fl/stl/mutex.h. The public API fl::mutex and
/// fl::recursive_mutex in fl/stl/mutex.h bind to fl::platforms::mutex and
/// fl::platforms::recursive_mutex.

#include "platforms/is_platform.h"

// Platform dispatch
#ifdef FL_IS_ESP32
    #include "platforms/esp/32/mutex_esp32.h"
#elif defined(FL_IS_STUB)
    #include "platforms/stub/mutex_stub.h"
#else
    // Default fallback: Use stub implementation for all platforms
    // TODO: Add platform-specific implementations here as needed
    #include "platforms/stub/mutex_stub.h"
#endif

#ifndef FASTLED_MULTITHREADED
#error "Expected FASTLED_MULTITHREADED to be defined by platform mutex implementation"
#endif
