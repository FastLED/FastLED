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
/// - Teensy: Cooperative context switching via coroutine_teensy.hpp (aggregate only)
/// - Other: Null implementation (no-op) via itask_coroutine.h
///
/// Platform implementations are compiled via coroutine_runtime.cpp.hpp (aggregate).
/// This header only provides the interface declarations needed by callers.

#if defined(ESP32)
    // ESP32: FreeRTOS task implementation
    #include "platforms/esp/32/coroutine_esp32.hpp"
#elif defined(FASTLED_STUB_IMPL)
    // Host/Stub: std::thread implementation for testing
    #include "platforms/stub/coroutine_stub.h"
#else
    // All other platforms (including Teensy): interface-only
    // Implementations are compiled via the aggregate (coroutine_runtime.cpp.hpp)
    #include "platforms/itask_coroutine.h"
#endif
