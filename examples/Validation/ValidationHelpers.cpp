// ValidationHelpers.cpp - Helper function implementations

#include "ValidationHelpers.h"
#include "fl/stl/sstream.h"
#include "fl/pin.h"  // Platform-independent pin API
#include "fl/channels/detail/validation/rx_test.h"
#include "fl/channels/detail/validation/rx_test.cpp.hpp"
#include "fl/channels/detail/validation/platform.h"
#include "fl/channels/detail/validation/platform.cpp.hpp"
#include "fl/channels/detail/validation/result_formatter.h"
#include "fl/channels/detail/validation/result_formatter.cpp.hpp"

bool testRxChannel(
    fl::shared_ptr<fl::RxDevice> rx_channel,
    int pin_tx,
    int pin_rx,
    uint32_t hz,
    size_t buffer_size) {
    // Delegate to library implementation
    return fl::validation::testRxChannel(rx_channel, pin_tx, pin_rx, hz, buffer_size);
}


void validateExpectedEngines() {
    // Delegate to library implementation
    auto drivers = FastLED.getDriverInfos();
    fl::validation::printEngineValidation(drivers);
}

void testDriver(
    const char* driver_name,
    const fl::NamedTimingConfig& timing_config,
    int pin_data,
    size_t num_leds,
    CRGB* leds,
    EOrder color_order,
    fl::shared_ptr<fl::RxDevice> rx_channel,
    fl::span<uint8_t> rx_buffer,
    int base_strip_size,
    fl::RxDeviceType rx_type,
    fl::DriverTestResult& result) {

    // Set this driver as exclusive for testing
    if (!FastLED.setExclusiveDriver(driver_name)) {
        FL_ERROR("Failed to set " << driver_name << " as exclusive driver");
        result.skipped = true;
        return;
    }
    FL_WARN(driver_name << " driver enabled exclusively\n");

    FL_WARN("[CONFIG] Driver: " << driver_name << " (physical jumper required)\n");

    // Create TX configuration for validation tests
    fl::ChannelConfig tx_config(pin_data, timing_config.timing, fl::span<CRGB>(leds, num_leds), color_order);

    FL_WARN("[INFO] Testing " << timing_config.name << " timing\n");

    // Create validation configuration with all input parameters
    fl::ValidationConfig validation_config(
        timing_config.timing,
        timing_config.name,
        fl::span<fl::ChannelConfig>(&tx_config, 1),
        driver_name,
        rx_channel,
        rx_buffer,
        base_strip_size,
        rx_type
    );

    // FIRST RUN: Discard results (timing warm-up)
    // TX channel construction may have extra latency on first run
    FL_WARN("[INFO] Running warm-up frame (results will be discarded)");
    int warmup_total = 0, warmup_passed = 0;
    uint32_t warmup_duration_ms = 0;
    validateChipsetTiming(validation_config, warmup_total, warmup_passed, warmup_duration_ms);
    FL_WARN("[INFO] Warm-up complete (" << warmup_passed << "/" << warmup_total << " passed - discarding)");

    // SECOND RUN: Keep results (actual test)
    FL_WARN("[INFO] Running actual test frame");
    uint32_t test_duration_ms = 0;
    validateChipsetTiming(validation_config, result.total_tests, result.passed_tests, test_duration_ms);

    FL_WARN("\n[INFO] All timing tests complete for " << driver_name << " driver");
}

void printSummaryTable(const fl::vector<fl::DriverTestResult>& driver_results) {
    // Delegate to library implementation
    fl::validation::printSummaryTable(driver_results);
}

// Build test matrix configuration from preprocessor defines and available drivers