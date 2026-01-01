// ValidationTest.h - Generic LED validation testing infrastructure
// Driver-agnostic test functions that work with any FastLED driver (RMT, SPI, PARLIO)

#pragma once

#include <FastLED.h>
#include "fl/rx_device.h"

namespace fl {

/// @brief Configuration for driver-agnostic validation testing
/// Contains all input parameters needed for validation (excludes output parameters)
struct ValidationConfig {
    const fl::ChipsetTimingConfig& timing;       ///< Chipset timing configuration to test
    const char* timing_name;                     ///< Timing name for logging (e.g., "WS2812B-V5")
    fl::span<fl::ChannelConfig> tx_configs;      ///< TX channel configurations to validate (mutable for LED manipulation)
    const char* driver_name;                     ///< Driver name for logging (e.g., "RMT", "SPI", "PARLIO")
    fl::shared_ptr<fl::RxDevice> rx_channel;     ///< RX channel for loopback capture (created in .ino, passed in)
    fl::span<uint8_t> rx_buffer;                 ///< Buffer to store received bytes
    int base_strip_size;                         ///< Base strip size (10 or 300 LEDs)
    fl::RxDeviceType rx_type;                    ///< RX device type (RMT or ISR)

    ValidationConfig(const fl::ChipsetTimingConfig& t,
                     const char* tn,
                     fl::span<fl::ChannelConfig> tc,
                     const char* dn,
                     fl::shared_ptr<fl::RxDevice> rc,
                     fl::span<uint8_t> rb,
                     int bss,
                     fl::RxDeviceType rt)
        : timing(t)
        , timing_name(tn)
        , tx_configs(tc)
        , driver_name(dn)
        , rx_channel(rc)
        , rx_buffer(rb)
        , base_strip_size(bss)
        , rx_type(rt) {}
};

/// @brief Test context for detailed error reporting
/// Aggregates all test configuration parameters for error messages
struct TestContext {
    const char* driver_name;      ///< Driver name (e.g., "RMT", "SPI", "PARLIO")
    const char* timing_name;      ///< Timing name (e.g., "WS2812B-V5")
    const char* rx_type_name;     ///< RX device type name (e.g., "RMT", "ISR")
    const char* pattern_name;     ///< Pattern name (e.g., "Pattern A (R=0xF0...)")
    int lane_count;               ///< Total number of lanes (1-8)
    int lane_index;               ///< Current lane index (0-7)
    int base_strip_size;          ///< Base strip size (10 or 300 LEDs)
    int num_leds;                 ///< Number of LEDs in this lane
    int pin_number;               ///< TX pin number for this lane
};

/// @brief LED error information for a single run
struct LEDError {
    int led_index;          ///< LED index where error occurred
    uint8_t expected_r;     ///< Expected R value
    uint8_t expected_g;     ///< Expected G value
    uint8_t expected_b;     ///< Expected B value
    uint8_t actual_r;       ///< Actual R value
    uint8_t actual_g;       ///< Actual G value
    uint8_t actual_b;       ///< Actual B value

    LEDError(int idx, uint8_t exp_r, uint8_t exp_g, uint8_t exp_b,
             uint8_t act_r, uint8_t act_g, uint8_t act_b)
        : led_index(idx)
        , expected_r(exp_r), expected_g(exp_g), expected_b(exp_b)
        , actual_r(act_r), actual_g(act_g), actual_b(act_b) {}
};

/// @brief Single run result with error tracking
struct RunResult {
    int run_number;                 ///< Run iteration number (1-based)
    int total_leds;                 ///< Total LEDs tested
    int mismatches;                 ///< Number of LED mismatches
    fl::vector<LEDError> errors;    ///< First few errors (up to 5)
    bool passed;                    ///< True if no errors

    RunResult() : run_number(0), total_leds(0), mismatches(0), passed(false) {}
};

/// @brief Multi-run test configuration
struct MultiRunConfig {
    int num_runs;               ///< Number of runs to execute
    bool print_all_runs;        ///< Print all run results (default: only errors)
    bool print_per_led_errors;  ///< Print every LED error (default: false)
    int max_errors_per_run;     ///< Max errors to store per run (default: 5)

    MultiRunConfig()
        : num_runs(10)
        , print_all_runs(false)
        , print_per_led_errors(false)
        , max_errors_per_run(5) {}
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
// - rx_channel: Shared pointer to RX device (persistent across calls)
// - rx_buffer: Buffer to store received bytes
// Returns number of bytes captured, or 0 on error
size_t capture(fl::shared_ptr<fl::RxDevice> rx_channel, fl::span<uint8_t> rx_buffer);

// Generic driver-agnostic validation test runner (single run)
// Validates all channels using the specified configuration
// @param test_name Test name for logging (e.g., "Solid Red", "Solid Green")
// @param config All validation configuration (timing, channels, driver, RX, buffer) - non-const for LED manipulation
// @param total Output parameter - total tests run (incremented)
// @param passed Output parameter - tests passed (incremented)
void runTest(const char* test_name,
             fl::ValidationConfig& config,
             int& total, int& passed);

// Multi-run validation test runner
// Runs the same test multiple times and tracks errors across runs
// @param test_name Test name for logging (e.g., "Pattern A", "Pattern B")
// @param config All validation configuration (timing, channels, driver, RX, buffer) - non-const for LED manipulation
// @param multi_config Multi-run configuration (num runs, print settings)
// @param total Output parameter - total tests run (incremented)
// @param passed Output parameter - tests passed (incremented)
void runMultiTest(const char* test_name,
                  fl::ValidationConfig& config,
                  const fl::MultiRunConfig& multi_config,
                  int& total, int& passed);

// Validate a specific chipset timing configuration
// Creates channels, runs tests, destroys channels
// @param config All validation configuration (timing, channels, driver, RX, buffer) - non-const for LED manipulation
// @param total Output parameter - total tests run (incremented)
// @param passed Output parameter - tests passed (incremented)
void validateChipsetTiming(fl::ValidationConfig& config,
                           int& total, int& passed);

// Set mixed RGB bit patterns to test MSB vs LSB handling
// @param leds LED array to fill with pattern
// @param count Number of LEDs in array
// @param pattern_id Pattern identifier (0-3):
//   0 = Pattern A: R=0xF0, G=0x0F, B=0xAA (high nibble, low nibble, alternating)
//   1 = Pattern B: R=0x55, G=0xFF, B=0x00 (alternating, all-high, all-low)
//   2 = Pattern C: R=0x0F, G=0xAA, B=0xF0 (rotated nibbles)
//   3 = Pattern D: Solid colors (Red/Green/Blue alternating every 3 LEDs)
void setMixedBitPattern(CRGB* leds, size_t count, int pattern_id);

// Get name of bit pattern for logging
// @param pattern_id Pattern identifier (0-3)
// @return Pattern name string
const char* getBitPatternName(int pattern_id);
