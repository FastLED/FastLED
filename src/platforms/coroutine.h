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
/// - ESP32: FreeRTOS tasks via task_coroutine_esp32.h
/// - Host/Stub: std::thread implementation via task_coroutine_stub.h (for testing)
/// - Other: Null implementation (no-op) via task_coroutine_null.h
///
/// Usage:
/// ```cpp
/// #include "platforms/coroutine.h"
/// // TaskCoroutine methods are now available
/// ```

// ARM platform detection
#include "platforms/arm/is_arm.h"

#if defined(ESP32)
    // ESP32: FreeRTOS task implementation
    #include "platforms/esp/32/task_coroutine_esp32.h"
#elif defined(FASTLED_STUB_IMPL)
    // Host/Stub: std::thread implementation for testing
    #include "platforms/stub/task_coroutine_stub.h"
#else
    // All other platforms: Null implementation (no-op)
    #include "platforms/shared/task_coroutine_null.h"
#endif
