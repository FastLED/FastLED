// AutoResearchHelpers.cpp - Helper function implementations

#include "AutoResearchHelpers.h"
#include "fl/channels/manager.h"
#include "fl/stl/sstream.h"
#include "fl/system/pin.h"  // Platform-independent pin API
#include "fl/channels/detail/validation/rx_test.h"
#include "fl/channels/detail/validation/platform.h"
#include "fl/channels/detail/validation/result_formatter.h"

bool testRxChannel(
    fl::shared_ptr<fl::RxChannel> rx_channel,
    int pin_tx,
    int pin_rx,
    uint32_t hz,
    size_t buffer_size) {
    // Delegate to library implementation
    return fl::validation::testRxChannel(rx_channel, pin_tx, pin_rx, hz, buffer_size);
}


void autoResearchExpectedEngines() {
    // Delegate to library implementation
    fl::validation::printEngineValidation();
}

bool autoResearchSetExclusiveDriverByName(const char* name) {
    // AutoResearch resolves driver names at runtime (RPC/JSON). The caller
    // (AutoResearch.ino setup()) must already have enrolled every available
    // driver via FastLED.enableAllDrivers(). We deliberately do NOT call
    // enableAllDrivers() here — doing it per-iteration would re-add every
    // driver each call, triggering ChannelManager's "Replacing existing driver"
    // path and resetting state mid-test (#2469).
    return fl::ChannelManager::instance().setExclusiveDriverByName(name);
}

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
    fl::DriverTestResult& result) {

    // Set this driver as exclusive for testing. AutoResearch resolves driver
    // names at runtime, so we use the by-name helper (auto-enables all
    // drivers first to ensure the lookup succeeds).
    if (!autoResearchSetExclusiveDriverByName(driver_name)) {
        FL_ERROR("Failed to set " << driver_name << " as exclusive driver");
        result.skipped = true;
        return;
    }
    FL_WARN(driver_name << " driver enabled exclusively\n");

    FL_WARN("[CONFIG] Driver: " << driver_name << " (physical jumper required)\n");

    // Create TX configuration for autoresearch tests.
    // Build the ClocklessChipset explicitly so the encoder selector (carried
    // alongside timing in NamedTimingConfig, #2467) is preserved end-to-end.
    fl::ClocklessChipset chipset(pin_data, timing_config.timing, timing_config.encoder);
    fl::ChannelConfig tx_config(chipset, fl::span<CRGB>(leds, num_leds), color_order);

    FL_WARN("[INFO] Testing " << timing_config.name << " timing\n");

    // Create autoresearch configuration with all input parameters
    fl::AutoResearchConfig autoresearch_config(
        timing_config.timing,
        timing_config.name,
        fl::span<fl::ChannelConfig>(&tx_config, 1),
        driver_name,
        rx_channel,
        rx_buffer,
        base_strip_size,
        rx_type,
        timing_config.encoder
    );

    // FIRST RUN: Discard results (timing warm-up)
    // TX channel construction may have extra latency on first run
    FL_WARN("[INFO] Running warm-up frame (results will be discarded)");
    int warmup_total = 0, warmup_passed = 0;
    uint32_t warmup_duration_ms = 0;
    autoResearchChipsetTiming(autoresearch_config, warmup_total, warmup_passed, warmup_duration_ms);
    FL_WARN("[INFO] Warm-up complete (" << warmup_passed << "/" << warmup_total << " passed - discarding)");

    // SECOND RUN: Keep results (actual test)
    FL_WARN("[INFO] Running actual test frame");
    uint32_t test_duration_ms = 0;
    autoResearchChipsetTiming(autoresearch_config, result.total_tests, result.passed_tests, test_duration_ms);

    FL_WARN("\n[INFO] All timing tests complete for " << driver_name << " driver");
}

void printSummaryTable(const fl::vector<fl::DriverTestResult>& driver_results) {
    // Delegate to library implementation
    fl::validation::printSummaryTable(driver_results);
}

// Build test matrix configuration from preprocessor defines and available drivers
