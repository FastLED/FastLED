// ValidationTest.h - Generic LED validation testing infrastructure
// Driver-agnostic test functions that work with any FastLED driver (RMT, SPI, PARLIO)

#pragma once

#include <FastLED.h>
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"

namespace fl {

/// @brief Configuration for driver-agnostic validation testing
/// Contains all input parameters needed for validation (excludes output parameters)
struct ValidationConfig {
    const fl::ChipsetTimingConfig& timing;       ///< Chipset timing configuration to test
    const char* timing_name;                     ///< Timing name for logging (e.g., "WS2812B-V5")
    fl::span<fl::ChannelConfig> tx_configs;      ///< TX channel configurations to validate (mutable for LED manipulation)
    const char* driver_name;                     ///< Driver name for logging (e.g., "RMT", "SPI", "PARLIO")
    fl::shared_ptr<fl::RmtRxChannel> rx_channel; ///< RX channel for loopback capture (created in .ino, passed in)
    fl::span<uint8_t> rx_buffer;                 ///< Buffer to store received bytes

    ValidationConfig(const fl::ChipsetTimingConfig& timing_,
                     const char* timing_name_,
                     fl::span<fl::ChannelConfig> tx_configs_,
                     const char* driver_name_,
                     fl::shared_ptr<fl::RmtRxChannel> rx_channel_,
                     fl::span<uint8_t> rx_buffer_)
        : timing(timing_)
        , timing_name(timing_name_)
        , tx_configs(tx_configs_)
        , driver_name(driver_name_)
        , rx_channel(rx_channel_)
        , rx_buffer(rx_buffer_) {}
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
// Validates all channels using the specified configuration
// @param test_name Test name for logging (e.g., "Solid Red", "Solid Green")
// @param config All validation configuration (timing, channels, driver, RX, buffer) - non-const for LED manipulation
// @param total Output parameter - total tests run (incremented)
// @param passed Output parameter - tests passed (incremented)
void runTest(const char* test_name,
             fl::ValidationConfig& config,
             int& total, int& passed);

// Validate a specific chipset timing configuration
// Creates channels, runs tests, destroys channels
// @param config All validation configuration (timing, channels, driver, RX, buffer) - non-const for LED manipulation
// @param total Output parameter - total tests run (incremented)
// @param passed Output parameter - tests passed (incremented)
void validateChipsetTiming(fl::ValidationConfig& config,
                           int& total, int& passed);
