// src/platforms/esp/32/watchdog_esp32.h
//
// ESP32 Watchdog Timer for Proof-of-Life Monitoring
//
// This header provides a simple watchdog timer interface for detecting
// when the main loop hangs or stops executing. Uses the ESP32 task watchdog
// framework. The unified Watchdog wrapper subscribes the calling Arduino loop
// task and feeds it explicitly, while the ESP backend also keeps idle-task
// monitoring active for CPU-starvation detection.
//
// USAGE:
// - Call fl::watchdog_setup() once in setup()
// - Watchdog monitors loop() execution through FastLED.watchdog().feed()
// - If loop() hangs for timeout duration, watchdog triggers safe reset
// - Safe reset includes USB disconnect to prevent phantom devices
// - Optionally provide a callback function to execute before reset
//
// PLATFORM SUPPORT:
// - ESP32 family: Full watchdog implementation with configurable timeout
// - The unified fl::Watchdog wrapper feeds the subscribed loop task explicitly.

#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief User callback function type for watchdog timeout events
/// @param user_data Optional user-provided context pointer
/// @note Called from ISR context when watchdog fires - keep it fast and simple
/// @note Do NOT use logging functions (FL_WARN, FL_DBG) inside this callback
typedef void (*watchdog_callback_t)(void* user_data);

/// @brief Setup ESP32 watchdog timer for proof-of-life monitoring
/// @param timeout_ms Watchdog timeout in milliseconds (default: 5000ms)
/// @param callback Optional callback function to execute when watchdog fires (default: nullptr)
/// @param user_data Optional user data pointer passed to callback (default: nullptr)
/// @note ESP32: Installs watchdog that monitors loop() task execution
/// @note Use FastLED.watchdog().feed() or FL_WATCHDOG_AUTO() to reset the subscribed loop task
/// @note On timeout: Calls user callback (if provided), prints message, disconnects USB, then resets
/// @note Callback executes in ISR context - keep it simple and avoid logging functions
void watchdog_setup(u32 timeout_ms = 5000,
                    watchdog_callback_t callback = nullptr,
                    void* user_data = nullptr) FL_NOEXCEPT;

/// @brief Tear down the ESP32 task watchdog.
/// @return true if teardown succeeded (TWDT is now disabled and callbacks
///         cleared); false if `esp_task_wdt_deinit()` failed (e.g., other
///         tasks still subscribed, scheduler not running, or already
///         deinitialized — in any of those cases the TWDT remains active).
/// @note Wraps `esp_task_wdt_deinit()` and propagates its return.
/// @note Used by `fl::Watchdog::disable()` to honor the Tier-0 disable contract.
bool watchdog_disable() FL_NOEXCEPT;

} // namespace fl
