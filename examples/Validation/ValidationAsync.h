// examples/Validation/ValidationAsync.h
//
// Async task setup for JSON-RPC processing in validation sketch.
// Runs RPC polling on FastLED's task scheduler for non-blocking operation.

#pragma once

#include "ValidationRemote.h"
#include "fl/task.h"
#include "fl/async.h"

namespace validation {

/// @brief Setup async task for JSON-RPC processing
/// @param remote_control Reference to RemoteControl singleton
/// @param interval_ms Task interval in milliseconds (default: 10ms)
/// @return Task handle (auto-registered with scheduler)
///
/// This function creates an async task that polls the RPC system at regular intervals,
/// allowing RPC commands to be processed without blocking the main loop.
///
/// The 10ms default interval balances:
/// - Responsiveness: 115200 baud ≈ 100 bytes in 10ms
/// - CPU overhead: Minimal impact on LED peripheral operations
///
/// The task is automatically registered with the scheduler via .then() and will
/// run until the program exits.
///
/// THREAD SAFETY:
/// - Safe to capture remote_control by reference (singleton lifetime)
/// - ESP32 Arduino runs on single core - task switching is atomic
/// - No additional synchronization needed
inline fl::task setupRpcAsyncTask(ValidationRemoteControl& remote_control, int interval_ms = 10) {
    return fl::task::every_ms(interval_ms, FL_TRACE)
        .then([&remote_control]() {
            uint32_t current_time = millis();
            remote_control.tick(current_time);  // Calls Remote::update() which does pull + tick + push
        });
}

} // namespace validation
