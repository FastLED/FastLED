// ValidationTest.h - Generic LED validation testing infrastructure
// Driver-agnostic test functions that work with any FastLED driver (RMT, SPI, PARLIO)

#pragma once

#include <FastLED.h>
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"

namespace fl {

/// @brief Configuration for driver-agnostic validation testing
struct ValidationConfig {
    /// Driver name for logging (e.g., "RMT", "SPI", "PARLIO")
    /// This should match the driver name returned by FastLED.getDriverInfo()
    const char* driver_name;

    /// Whether physical jumper wires are required
    bool requires_physical_jumper;

    /// RX pin for shared RX channel (single-channel mode only, -1 for multi-channel auto-create)
    int rx_pin;

    ValidationConfig(const char* name, bool needs_jumper, int rx = -1)
        : driver_name(name)
        , requires_physical_jumper(needs_jumper)
        , rx_pin(rx) {}
};

/// @brief Driver test result tracking
struct DriverTestResult {
    fl::string driver_name;  ///< Driver name (e.g., "RMT", "SPI", "PARLIO")
    int total_tests;         ///< Total test count across all chipset timings
    int passed_tests;        ///< Passed test count across all chipset timings
    bool skipped;            ///< True if driver was skipped (e.g., failed to set exclusive)

    DriverTestResult(const char* name)
        : driver_name(name)
        , total_tests(0)
        , passed_tests(0)
        , skipped(false) {}

    /// @brief Check if all tests passed
    bool allPassed() const { return !skipped && total_tests > 0 && passed_tests == total_tests; }

    /// @brief Check if any tests failed
    bool anyFailed() const { return !skipped && total_tests > 0 && passed_tests < total_tests; }
};

} // namespace fl

// Capture transmitted LED data via RX loopback
// - rx_channel: Shared pointer to RMT RX channel (persistent across calls)
// - rx_buffer: Buffer to store received bytes
// Returns number of bytes captured, or 0 on error
size_t capture(fl::shared_ptr<fl::RmtRxChannel> rx_channel, fl::span<uint8_t> rx_buffer);

// Generic driver-agnostic validation test runner
// Validates all channels in tx_configs using the specified driver configuration
void runTest(const char* test_name,
             fl::span<fl::ChannelConfig> tx_configs,
             const fl::ValidationConfig& config,
             fl::shared_ptr<fl::RmtRxChannel> shared_rx_channel,
             fl::span<uint8_t> rx_buffer,
             int& total, int& passed);

// Validate a specific chipset timing configuration
// Creates channels, runs tests, destroys channels
// @param chipset_name Chipset timing name (e.g., "WS2812 Standard", "WS2812B-V5", "SK6812")
//                     Used for logging and test identification only
// @param total Output parameter - total tests run (incremented)
// @param passed Output parameter - tests passed (incremented)
void validateChipsetTiming(const fl::ChipsetTimingConfig& timing,
                           const char* chipset_name,
                           fl::span<fl::ChannelConfig> tx_configs,
                           const fl::ValidationConfig& config,
                           fl::shared_ptr<fl::RmtRxChannel> rx_channel,
                           fl::span<uint8_t> rx_buffer,
                           int& total, int& passed);

// Main validation test orchestrator
// Discovers drivers, iterates through them, and runs all chipset timing tests
void runAllValidationTests(int pin_data,
                           int pin_rx,
                           fl::span<CRGB> leds,
                           fl::shared_ptr<fl::RmtRxChannel> rx_channel,
                           fl::span<uint8_t> rx_buffer,
                           int color_order = RGB);
