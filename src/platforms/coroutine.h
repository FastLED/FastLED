// ok no namespace fl
#pragma once

/// @file platforms/coroutine.h
/// @brief Platform dispatch for OS-level coroutine/task support
///
/// This header provides a unified interface to platform-specific OS task/thread
/// management. It dispatches to the appropriate platform implementation based
/// on compile-time platform detection.
///
/// Supported platforms:
/// - ESP32: FreeRTOS tasks via coroutine_esp32.hpp
/// - Host/Stub: std::thread implementation via coroutine_stub.h (for testing)
/// - Teensy: Cooperative context switching via coroutine_teensy.hpp
/// - Other: Null implementation (no-op) via itask_coroutine.h
///
/// Usage:
/// ```cpp
/// #include "platforms/coroutine.h"
/// // TaskCoroutine methods are now available
/// ```

// ARM platform detection
#include "platforms/arm/is_arm.h"
#include "platforms/arm/teensy/is_teensy.h"

#if defined(ESP32)
    // ESP32: FreeRTOS task implementation
    #include "platforms/esp/32/coroutine_esp32.hpp"
#elif defined(FASTLED_STUB_IMPL)
    // Host/Stub: std::thread implementation for testing
    #include "platforms/stub/coroutine_stub.h"
#elif defined(FL_IS_TEENSY_4X)
    // Teensy: Cooperative context switching on main thread
    #include "platforms/arm/teensy/coroutine_teensy.hpp"
#else
    // All other platforms: Null implementation (no-op)
    // TaskCoroutineNull is declared in itask_coroutine.h
    #include "platforms/itask_coroutine.h"
#endif
