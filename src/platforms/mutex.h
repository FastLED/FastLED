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
/// - RP2040/RP2350: Pico SDK spinlock-based mutexes
/// - STM32: FreeRTOS mutex wrappers (when FreeRTOS is available)
/// - SAMD21/SAMD51: CMSIS interrupt-based mutex
/// - Teensy: Interrupt-based mutex using critical sections
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
#elif defined(FL_IS_RP2040)
    #include "platforms/arm/rp/mutex_rp.h"
#elif defined(FL_IS_STM32) && __has_include("FreeRTOS.h")
    #include "platforms/arm/stm32/mutex_stm32.h"
#elif defined(FL_IS_STUB) || defined(FL_IS_WASM)
    // WASM uses stub mutex profile (std::mutex with pthread support)
    #include "platforms/stub/mutex_stub.h"
#elif defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)
    #include "platforms/arm/d21/mutex_samd.h"
#elif defined(FL_IS_TEENSY)
    // TODO: Implement Teensy mutexes
    #include "platforms/stub/mutex_stub_noop.h"
#else
    // Default fallback: Use stub implementation for all platforms
    // TODO: Add platform-specific implementations here as needed
    #include "platforms/stub/mutex_stub.h"
#endif

#ifndef FASTLED_MULTITHREADED
#error "Expected FASTLED_MULTITHREADED to be defined by platform mutex implementation"
#endif
