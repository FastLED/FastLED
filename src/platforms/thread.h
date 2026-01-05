// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/thread.h
/// @brief Platform dispatch for thread support
///
/// This header provides platform-specific thread implementations in fl::platforms::
/// namespace. It follows the coarse-to-fine delegation pattern, routing to
/// platform-specific headers based on platform detection.
///
/// Supported platforms:
/// - Stub: std::thread wrapper (when pthread available) or fake thread
/// - Other platforms: Use stub no-op implementation as fallback
///
/// This header is included from fl/stl/thread.h. The public API fl::thread in
/// fl/stl/thread.h binds to fl::platforms::thread.

#include "platforms/is_platform.h"

// Platform dispatch
#ifdef FL_IS_STUB
    #include "platforms/stub/thread_stub.h"
#else
    // Default fallback: Use stub no-op implementation for platforms without threading
    // TODO: Add platform-specific implementations here
    // Example:
    // #elif defined(FL_IS_ESP32)
    //     #include "platforms/esp/thread_esp.h"
    // #elif defined(FL_IS_AVR)
    //     #include "platforms/avr/thread_avr.h"
    #include "platforms/stub/thread_stub_noop.h"
#endif


#ifndef FASTLED_MULTITHREADED
#error "Expected FASTLED_MULTITHREADED to be defined"
#endif
