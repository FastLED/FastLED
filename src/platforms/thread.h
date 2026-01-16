// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/thread.h
/// @brief Platform dispatch for thread support and mutex implementations
///
/// This header provides:
/// 1. Platform-specific thread implementations in fl::platforms:: namespace
/// 2. Platform-specific mutex implementations (for platforms with RTOS support)
///
/// Threading Support:
/// - Stub: std::thread wrapper (when pthread available) or fake thread
/// - WASM: Uses stub platform threading profile (pthread support for testing/simulation)
/// - Other platforms: Use stub no-op implementation as fallback
///
/// Mutex Support (Platform-Specific):
/// - nRF52: FreeRTOS mutex support (SoftDevice compatible)
/// - STM32: CMSIS-RTOS v1/v2 mutex support (optional, auto-detected)
/// - Teensy: FreeRTOS, TeensyThreads, or interrupt-based mutex (auto-detected)
/// - ESP32/RP2040: Already have their own mutex implementations
/// - POSIX/Windows/Stub: Use std::mutex via fl/stl/mutex.h
/// - AVR: Uses interrupt disable (no threading support)

#include "platforms/is_platform.h"
#include "platforms/arm/is_arm.h"

//------------------------------------------------------------------------------
// THREADING CONFIGURATION DISPATCH
//------------------------------------------------------------------------------

// Platform dispatch for threading
#if defined(FL_IS_STUB) || defined(FL_IS_WASM)
    // WASM uses stub threading profile (pthread-based multithreading)
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

//------------------------------------------------------------------------------
// PLATFORM-SPECIFIC MUTEX IMPLEMENTATIONS
//------------------------------------------------------------------------------
// These headers provide real mutex implementations for platforms with RTOS support.
// They are conditionally included and only activate when the appropriate RTOS is detected.
// If no RTOS is available, they fall back to fake mutex implementations.

// nRF52 - FreeRTOS mutex support (SoftDevice compatible)
#if defined(FL_IS_NRF52)
    #include "platforms/arm/nrf52/mutex_nrf52.h"
#endif

// STM32 - CMSIS-RTOS v1/v2 mutex support (optional, auto-detected)
#if defined(FL_IS_STM32)
    #include "platforms/arm/stm32/mutex_stm32_rtos.h"
#endif

// Teensy - FreeRTOS, TeensyThreads, or interrupt-based mutex (auto-detected)
#if defined(FL_IS_TEENSY)
    #include "platforms/stub/thread_stub_noop.h"
#endif

// Note: ESP32 and RP2040/RP2350 already have their own mutex implementations
// Note: POSIX/Windows/Stub use std::mutex via fl/stl/mutex.h
// Note: AVR uses interrupt disable (no threading support)

// Verify FASTLED_MULTITHREADED is defined (should be set by fl/stl/thread_config.h)
#ifndef FASTLED_MULTITHREADED
#error "Expected FASTLED_MULTITHREADED to be defined by fl/stl/thread_config.h"
#endif
