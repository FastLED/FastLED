// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/semaphore.h
/// @brief Platform dispatch for semaphore support
///
/// This header provides platform-specific semaphore implementations in fl::platforms::
/// namespace. It follows the coarse-to-fine delegation pattern, routing to
/// platform-specific headers based on platform detection.
///
/// Supported platforms:
/// - ESP32: FreeRTOS semaphore wrappers
/// - STM32: FreeRTOS semaphore wrappers (when FreeRTOS is available)
/// - RP2040/RP2350: Pico SDK spinlock-based semaphores
/// - Teensy: Interrupt-based semaphore using critical sections
/// - Stub: C++20 semaphore, mutex/cv-based, or fake semaphore based on pthread availability
/// - Other platforms: Use stub implementation as fallback
///
/// This header is included from fl/stl/semaphore.h. The public API fl::counting_semaphore
/// and fl::binary_semaphore in fl/stl/semaphore.h bind to fl::platforms::counting_semaphore
/// and fl::platforms::binary_semaphore.

#include "platforms/is_platform.h"

// Platform dispatch
#ifdef FL_IS_STUB
    #include "platforms/stub/semaphore_stub.h"
#elif defined(ESP32)
    #include "platforms/esp/32/semaphore_esp32.h"
#elif defined(FL_IS_STM32) && __has_include("FreeRTOS.h")
    #include "platforms/arm/stm32/semaphore_stm32.h"
#elif defined(FL_IS_RP2040)
    #include "platforms/arm/rp/semaphore_rp.h"
#elif defined(FL_IS_TEENSY)
    #include "platforms/arm/teensy/semaphore_teensy.h"
#else
    // Default fallback: Use stub implementation for all platforms
    // TODO: Add platform-specific implementations here
    // Example:
    // #elif defined(FL_IS_AVR)
    //     #include "platforms/avr/semaphore_avr.h"
    #include "platforms/stub/semaphore_stub.h"
#endif

#ifndef FASTLED_MULTITHREADED
#error "Expected FASTLED_MULTITHREADED to be defined by platform semaphore implementation"
#endif
