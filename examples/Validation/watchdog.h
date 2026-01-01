// examples/Validation/watchdog.h
//
// ESP32-C6 Watchdog Timer for Proof-of-Life Monitoring
//
// This header provides a simple watchdog timer interface for detecting
// when the main loop hangs or stops executing. Uses the ESP32 task watchdog
// framework which automatically monitors the main loop task.
//
// USAGE:
// - Call setup_watchdog() once in setup()
// - Watchdog automatically monitors loop() execution
// - If loop() hangs for 5+ seconds, watchdog triggers safe reset
// - Safe reset includes USB disconnect to prevent phantom devices
//
// PLATFORM SUPPORT:
// - ESP32-C6: Full watchdog implementation with 5-second timeout
// - Other platforms: No-op stub (function does nothing)

#pragma once

namespace validation {

/// @brief Setup custom watchdog timer for proof-of-life monitoring
/// @note ESP32-C6: Installs 5-second watchdog that monitors loop() task
/// @note Watchdog automatically fed by ESP32 framework - no manual feeding needed
/// @note On timeout: Prints "watchdog fired" message, disconnects USB, then resets
/// @note Other platforms: No-op (does nothing)
void setup_watchdog();

} // namespace validation
