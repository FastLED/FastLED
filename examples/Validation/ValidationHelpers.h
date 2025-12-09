// ValidationHelpers.h - Helper functions for Validation.ino
// Contains utility functions for driver testing, RX channel setup, and result reporting

#pragma once

#include <FastLED.h>
#include "Common.h"
#include "ValidationTest.h"
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"

/// @brief Create RMT RX channel with specified parameters
/// @param pin_rx RX pin number
/// @param hz Sampling frequency in Hz (e.g., 20000000 for 20MHz)
/// @param buffer_size Size of RX buffer in symbols
/// @return Shared pointer to RX channel, or nullptr if creation fails
fl::shared_ptr<fl::RmtRxChannel> createRxChannel(
    int pin_rx,
    uint32_t hz,
    size_t buffer_size);

/// @brief Test RX channel with manual GPIO toggle
/// @param rx_channel RX channel to test
/// @param pin_tx GPIO pin to toggle
/// @param pin_rx GPIO pin for RX (for logging)
/// @return true if test passes, false otherwise
bool testRxChannel(
    fl::shared_ptr<fl::RmtRxChannel> rx_channel,
    int pin_tx,
    int pin_rx);

/// @brief Validate that expected engines are available for this platform
/// Prints ERROR if any expected engines are missing
void validateExpectedEngines();

/// @brief Test a specific driver with given timing configuration
/// @param driver_name Driver name to test
/// @param timing_config Named timing configuration (includes timing and name)
/// @param pin_data TX pin number
/// @param num_leds Number of LEDs
/// @param leds LED array
/// @param color_order Color order
/// @param rx_channel RX channel for loopback
/// @param rx_buffer RX buffer for capture
/// @param result Driver test result tracker (modified)
/// @note Automatically discards first frame (timing warm-up) on first run
void testDriver(
    const char* driver_name,
    const fl::NamedTimingConfig& timing_config,
    int pin_data,
    size_t num_leds,
    CRGB* leds,
    EOrder color_order,
    fl::shared_ptr<fl::RmtRxChannel> rx_channel,
    fl::span<uint8_t> rx_buffer,
    fl::DriverTestResult& result);

/// @brief Print driver validation summary table
/// @param driver_results Vector of driver test results
void printSummaryTable(const fl::vector<fl::DriverTestResult>& driver_results);
