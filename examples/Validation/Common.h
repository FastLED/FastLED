// Common.h - Common data structures for Validation.ino
// Shared types used across multiple files

#pragma once

#include <FastLED.h>

// ============================================================================
// Test Configuration Constants
// ============================================================================
// These can be overridden in Validation.ino if needed

#ifndef MIN_LANES
#define MIN_LANES 1  // Default: start at 1 lane
#endif

#ifndef MAX_LANES
#define MAX_LANES 8  // Default: test up to 8 lanes
#endif

#ifndef SHORT_STRIP_SIZE
#define SHORT_STRIP_SIZE 10   // Short strip: 10 LEDs
#endif

#ifndef LONG_STRIP_SIZE
#define LONG_STRIP_SIZE 300   // Long strip: 300 LEDs
#endif

namespace fl {

/// @brief Driver failure tracking with detailed error information
struct DriverFailureInfo {
    fl::string driver_name;
    fl::string failure_details;  // e.g., "Byte mismatch at offset 5: expected 0xFF, got 0x00"
    uint32_t frame_number;       // Frame/iteration number when failure occurred

    DriverFailureInfo(const char* name, const char* details, uint32_t frame)
        : driver_name(name), failure_details(details), frame_number(frame) {}
};

/// @brief Chipset timing configuration with name for testing
struct NamedTimingConfig {
    fl::ChipsetTimingConfig timing;
    const char* name;

    NamedTimingConfig(const fl::ChipsetTimingConfig& timing_, const char* name_)
        : timing(timing_), name(name_) {}
};

/// @brief Per-lane LED configuration (each lane can have different LED count)
struct LaneConfig {
    int pin;                    ///< GPIO pin for this lane
    int num_leds;               ///< Number of LEDs on this lane
    fl::vector<CRGB> leds;      ///< LED array for this lane

    LaneConfig(int p, int n)
        : pin(p), num_leds(n), leds(n) {}
};

/// @brief Test case configuration (one combination in the test matrix)
struct TestCaseConfig {
    fl::string driver_name;     ///< Driver to test (e.g., "RMT", "SPI", "PARLIO")
    int lane_count;             ///< Number of lanes (1-8)
    int base_strip_size;        ///< Base LED count (SHORT_STRIP_SIZE or LONG_STRIP_SIZE)
    fl::vector<LaneConfig> lanes;  ///< Per-lane configurations

    TestCaseConfig(const char* driver, int num_lanes, int base_size)
        : driver_name(driver), lane_count(num_lanes), base_strip_size(base_size) {
        // Generate lane configurations with decreasing LED counts
        // Lane 0: base_size, Lane 1: base_size-1, ..., Lane N: base_size-N
        for (int i = 0; i < num_lanes; i++) {
            int lane_leds = base_size - i;
            if (lane_leds < 1) lane_leds = 1;  // Ensure at least 1 LED per lane

            // For multi-lane, use consecutive GPIO pins starting from lane 0
            // This matches PARLIO multi-lane pin allocation
            int lane_pin = 0 + i;  // Will be overridden with actual TX pins later

            lanes.push_back(LaneConfig(lane_pin, lane_leds));
        }
    }
};

/// @brief Test matrix configuration - controls which test combinations to run
struct TestMatrixConfig {
    fl::vector<fl::string> enabled_drivers;  ///< Drivers to test (filtered by JUST_* defines)
    int min_lanes;                           ///< Minimum lane count to test
    int max_lanes;                           ///< Maximum lane count to test
    bool test_small_strips;                  ///< Test short strips (10 LEDs)?
    bool test_large_strips;                  ///< Test long strips (300 LEDs)?

    TestMatrixConfig()
        : min_lanes(1)
        , max_lanes(8)
        , test_small_strips(true)
        , test_large_strips(true) {}

    /// @brief Get total number of test cases in the matrix
    int getTotalTestCases() const {
        int driver_count = enabled_drivers.size();
        int lane_range = (max_lanes - min_lanes + 1);
        int strip_sizes = (test_small_strips ? 1 : 0) + (test_large_strips ? 1 : 0);
        return driver_count * lane_range * strip_sizes;
    }
};

/// @brief Test case result tracking (per matrix combination)
struct TestCaseResult {
    fl::string driver_name;      ///< Driver name (e.g., "RMT")
    int lane_count;              ///< Number of lanes tested
    int base_strip_size;         ///< Base strip size (10 or 300)
    int total_tests;             ///< Total validation tests run
    int passed_tests;            ///< Number of tests passed
    bool skipped;                ///< True if test case was skipped

    TestCaseResult(const char* driver, int lanes, int size)
        : driver_name(driver)
        , lane_count(lanes)
        , base_strip_size(size)
        , total_tests(0)
        , passed_tests(0)
        , skipped(false) {}

    /// @brief Check if all tests passed
    bool allPassed() const { return !skipped && total_tests > 0 && passed_tests == total_tests; }

    /// @brief Check if any tests failed
    bool anyFailed() const { return !skipped && total_tests > 0 && passed_tests < total_tests; }
};

} // namespace fl
