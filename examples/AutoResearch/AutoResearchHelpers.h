// AutoResearchHelpers.h - Helper functions for AutoResearch.ino
// Contains utility functions for driver testing, RX channel setup, and result reporting

#pragma once

#include <FastLED.h>
#include "Common.h"
#include "AutoResearchTest.h"

/// @brief Test RX channel with manual GPIO toggle
/// @param rx_channel RX channel to test
/// @param pin_tx GPIO pin to toggle
/// @param pin_rx GPIO pin for RX
/// @param hz Sampling frequency in Hz
/// @param buffer_size Size of RX buffer in symbols
/// @return true if test passes, false otherwise
bool testRxChannel(
    fl::shared_ptr<fl::RxChannel> rx_channel,
    int pin_tx,
    int pin_rx,
    uint32_t hz,
    size_t buffer_size);

/// @brief AutoResearch that expected engines are available for this platform
/// Prints ERROR if any expected engines are missing
void autoResearchExpectedEngines();

/// @brief AutoResearch-style helper: set an exclusive driver by name
/// @param name Driver name (case-sensitive — must match an `IChannelDriver::getName()` value)
/// @return true if the driver was found and set as exclusive
///
/// AutoResearch resolves driver names at runtime from RPC/JSON payloads, so it
/// can't use the typed `FastLED.setExclusiveDriver(fl::Bus)` form. This wrapper
/// forwards to `ChannelManager::instance().setExclusiveDriverByName(name)`.
///
/// **Precondition:** The caller must already have enrolled every available
/// driver via `FastLED.enableAllDrivers()` (typically once in `setup()`).
/// This helper deliberately does NOT call `enableAllDrivers()` itself —
/// calling it per-iteration in a discovery loop re-adds every driver each
/// time, triggering ChannelManager's "Replacing existing driver" path and
/// resetting state mid-test (#2469).
///
/// For built-in driver code that knows its driver at compile time, use
/// `FastLED.setExclusiveDriver(fl::Bus::X)` instead (typo-safe).
bool autoResearchSetExclusiveDriverByName(const char* name);

/// @brief Test a specific driver with given timing configuration
/// @param driver_name Driver name to test
/// @param timing_config Named timing configuration (includes timing and name)
/// @param pin_data TX pin number
/// @param num_leds Number of LEDs
/// @param leds LED array
/// @param color_order Color order
/// @param rx_channel RX channel for loopback
/// @param rx_buffer RX buffer for capture
/// @param base_strip_size Base strip size for error reporting (usually same as num_leds)
/// @param rx_type RX device type for error reporting
/// @param result Driver test result tracker (modified)
/// @note Automatically discards first frame (timing warm-up) on first run
void testDriver(
    const char* driver_name,
    const fl::NamedTimingConfig& timing_config,
    int pin_data,
    size_t num_leds,
    CRGB* leds,
    EOrder color_order,
    fl::shared_ptr<fl::RxChannel> rx_channel,
    fl::span<uint8_t> rx_buffer,
    int base_strip_size,
    fl::RxDeviceType rx_type,
    fl::DriverTestResult& result);

/// @brief Print driver autoresearch summary table
/// @param driver_results Vector of driver test results
void printSummaryTable(const fl::vector<fl::DriverTestResult>& driver_results);
