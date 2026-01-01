// src/platforms/esp/32/watchdog_esp32.h
//
// ESP32 Watchdog Timer for Proof-of-Life Monitoring
//
// This header provides a simple watchdog timer interface for detecting
// when the main loop hangs or stops executing. Uses the ESP32 task watchdog
// framework which automatically monitors the main loop task.
//
// USAGE:
// - Call fl::watchdog_setup() once in setup()
// - Watchdog automatically monitors loop() execution (no manual feeding needed)
// - If loop() hangs for timeout duration, watchdog triggers safe reset
// - Safe reset includes USB disconnect to prevent phantom devices
// - Optionally provide a callback function to execute before reset
//
// PLATFORM SUPPORT:
// - ESP32 family: Full watchdog implementation with configurable timeout
// - Automatically feeds watchdog via idle task monitoring (no manual calls needed)

#pragma once

#include "fl/stl/cstdint.h"

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
/// @note Watchdog automatically fed by ESP32 framework - no manual feeding needed
/// @note On timeout: Calls user callback (if provided), prints message, disconnects USB, then resets
/// @note Callback executes in ISR context - keep it simple and avoid logging functions
void watchdog_setup(uint32_t timeout_ms = 5000,
                    watchdog_callback_t callback = nullptr,
                    void* user_data = nullptr);

} // namespace fl
