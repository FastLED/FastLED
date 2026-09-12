// tests/fl/channels/validation.cpp
//
// Unit tests for validation logic

#include "test.h"
#include "fl/channels/validation.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// Basic test configuration
SingleTestConfig makeBasicConfig() {
    SingleTestConfig config;
    config.driver_name = "PARLIO";
    config.lane_sizes = {100};
    config.pattern = "MSB_LSB_A";
    config.iterations = 1;
    config.pin_tx = 1;
    config.pin_rx = 0;
    return config;
}

FL_TEST_CASE("Basic configuration succeeds") {
    SingleTestConfig config = makeBasicConfig();
    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK(result.success);
    FL_CHECK(result.driver == "PARLIO");
    FL_CHECK(result.lane_count == 1);
    FL_CHECK(result.lane_sizes.size() == 1u);
    FL_CHECK(result.lane_sizes[0] == 100);
}

FL_TEST_CASE("Multi-lane configuration") {
    SingleTestConfig config = makeBasicConfig();
    config.lane_sizes = {100, 200, 150};

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK(result.success);
    FL_CHECK(result.lane_count == 3);
    FL_CHECK(result.lane_sizes.size() == 3u);
    FL_CHECK(result.lane_sizes[0] == 100);
    FL_CHECK(result.lane_sizes[1] == 200);
    FL_CHECK(result.lane_sizes[2] == 150);
}

FL_TEST_CASE("Invalid lane count - 0 lanes") {
    SingleTestConfig config = makeBasicConfig();
    config.lane_sizes.clear();

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK_FALSE(result.success);
    FL_CHECK(result.error_message.has_value());
}

FL_TEST_CASE("Invalid lane count - more than 16 lanes") {
    SingleTestConfig config = makeBasicConfig();
    config.lane_sizes = {
        100, 100, 100, 100, 100, 100, 100, 100, 100,
        100, 100, 100, 100, 100, 100, 100, 100}; // 17 lanes

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK_FALSE(result.success);
    FL_CHECK(result.error_message.has_value());
    FL_CHECK(result.error_message.value() == "Lane count cannot exceed 16");
}

FL_TEST_CASE("Multiple iterations") {
    SingleTestConfig config = makeBasicConfig();
    config.iterations = 3;

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK(result.success);
    FL_CHECK(result.total_tests == 12); // 4 patterns × 3 iterations
}

FL_TEST_CASE("Different drivers") {
    const char* drivers[] = {"PARLIO", "RMT", "SPI"};

    for (const char* driver : drivers) {
        SingleTestConfig config = makeBasicConfig();
        config.driver_name = driver;

        SingleTestResult result = runSingleValidationTest(config);

        FL_CHECK(result.success);
        FL_CHECK(result.driver == driver);
    }
}

FL_TEST_CASE("RMT internal loopback requires the same TX and RX GPIO") {
    FL_CHECK_TRUE(validation::useRmtInternalLoopback(true, 0, 0));
    FL_CHECK_FALSE(validation::useRmtInternalLoopback(true, 0, 1));
    FL_CHECK_FALSE(validation::useRmtInternalLoopback(false, 0, 0));
}

FL_TEST_CASE("C6 PARLIO validation avoids the conflicted RMT RX backend") {
    // PARLIO_RX, not ISR: the GPIO ISR backend cannot resolve WS2812-rate
    // edges on C6 (measured ~2 us capture ceiling vs the ~300 ns needed),
    // so C6 PARLIO validation oversamples into DMA instead (#3586).
    FL_CHECK(validation::resolveCaptureBackend(
                 RxBackend::PLATFORM_DEFAULT, false, true, true) ==
             RxBackend::PARLIO_RX);
    FL_CHECK(validation::resolveCaptureBackend(
                 RxBackend::PLATFORM_DEFAULT, false, true, false) ==
             RxBackend::PLATFORM_DEFAULT);
    FL_CHECK(validation::resolveCaptureBackend(
                 RxBackend::PLATFORM_DEFAULT, false, false, true) ==
             RxBackend::PLATFORM_DEFAULT);
    FL_CHECK(validation::resolveCaptureBackend(
                 RxBackend::RMT, true, true, true) ==
             RxBackend::RMT);
}

FL_TEST_CASE("ISR validation capture uses a frame-sized power-of-two buffer") {
    FL_CHECK_EQ(validation::captureEdgeCapacity(
                    3300, 30, RxBackend::ISR),
                512u);
    FL_CHECK_EQ(validation::captureEdgeCapacity(
                    3300, 300, RxBackend::ISR),
                8192u);
    FL_CHECK_EQ(validation::captureEdgeCapacity(
                    3300, 30, RxBackend::RMT),
                26400u);
}

// The RP PIO sampler's own constants, so the expectations below are the
// numbers the device actually runs with (rx_pio_channel.h). They are repeated
// rather than included because that header is RP-only and this test builds on
// the host.
constexpr size_t kEdgeCapacity = 100u * 3u * 16u + 1u;  // 4801
constexpr size_t kDmaTailWords = 64u;
constexpr u32 kSamplesPerWord = 32u;
constexpr u32 kSamplePeriodNs = 50u;   // 20 MHz
constexpr u32 kIdleTailNs = 100000u;   // RxChannelConfig default
constexpr u32 kArmingLeadInNs = 64000u;

size_t maxWireBytesAt(u32 bit_period_ns) {
    return validation::rpPioMaxWireBytes(kEdgeCapacity, kDmaTailWords,
                                         kSamplesPerWord, kSamplePeriodNs,
                                         kIdleTailNs, kArmingLeadInNs,
                                         bit_period_ns);
}

FL_TEST_CASE("RP PIO capture bound reproduces the measured hardware ceilings") {
    // Measured on an RP2350W (PIO0, GPIO0 -> GPIO1), 3/3 per boundary --
    // FastLED#4371. Each row is the largest frame that captures, expressed in
    // wire bytes so the chipsets are comparable.

    // 800 kHz class: the edge pool binds. 4801 / 16 phases per byte = 300.
    FL_CHECK_EQ(maxWireBytesAt(1200u), 300u);  // WS2818,     100 LEDs
    FL_CHECK_EQ(maxWireBytesAt(1225u), 300u);  // WS2812B-V5, 100 LEDs
    FL_CHECK_EQ(maxWireBytesAt(1250u), 300u);  // UCS7604,     47 LEDs (15 + 6n)
    FL_CHECK_EQ(maxWireBytesAt(1280u), 300u);  // WS2814,     100 LEDs

    // 400 kHz: the sample-time budget binds well below the pool.
    // 63 LEDs = 189 bytes passes, 64 LEDs = 192 bytes fails.
    FL_CHECK_EQ(maxWireBytesAt(2500u), 189u);
}

FL_TEST_CASE("RP PIO capture bound is the smaller of the two ceilings") {
    // Below the crossover the pool is the answer and the bound is flat;
    // above it the clock takes over and the bound falls with the bit period.
    FL_CHECK_EQ(maxWireBytesAt(1u), 300u);
    FL_CHECK(maxWireBytesAt(2000u) < 300u);
    FL_CHECK(maxWireBytesAt(2500u) < maxWireBytesAt(2000u));

    // A bit period long enough to exhaust the whole budget leaves nothing.
    FL_CHECK_EQ(maxWireBytesAt(1000000u), 0u);
}

FL_TEST_CASE("RP PIO capture bound rejects degenerate inputs") {
    FL_CHECK_EQ(maxWireBytesAt(0u), 0u);
    FL_CHECK_EQ(validation::rpPioMaxWireBytes(0, kDmaTailWords, kSamplesPerWord,
                                              kSamplePeriodNs, kIdleTailNs,
                                              kArmingLeadInNs, 1225u),
                0u);
    FL_CHECK_EQ(validation::rpPioMaxWireBytes(kEdgeCapacity, kDmaTailWords, 0u,
                                              kSamplePeriodNs, kIdleTailNs,
                                              kArmingLeadInNs, 1225u),
                0u);
    FL_CHECK_EQ(validation::rpPioMaxWireBytes(kEdgeCapacity, kDmaTailWords,
                                              kSamplesPerWord, 0u, kIdleTailNs,
                                              kArmingLeadInNs, 1225u),
                0u);
    // Reserves that swallow the entire budget yield nothing, not an underflow.
    FL_CHECK_EQ(validation::rpPioMaxWireBytes(kEdgeCapacity, kDmaTailWords,
                                              kSamplesPerWord, kSamplePeriodNs,
                                              4000000u, 0u, 1225u),
                0u);
}

FL_TEST_CASE("Invalid driver name - empty") {
    SingleTestConfig config = makeBasicConfig();
    config.driver_name = "";

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK_FALSE(result.success);
    FL_CHECK(result.error_message.has_value());
}

FL_TEST_CASE("Invalid lane size - zero") {
    SingleTestConfig config = makeBasicConfig();
    config.lane_sizes = {100, 0, 100};

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK_FALSE(result.success);
    FL_CHECK(result.error_message.has_value());
}

FL_TEST_CASE("Invalid lane size - negative") {
    SingleTestConfig config = makeBasicConfig();
    config.lane_sizes = {100, -50, 100};

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK_FALSE(result.success);
    FL_CHECK(result.error_message.has_value());
}

FL_TEST_CASE("Invalid iterations - zero") {
    SingleTestConfig config = makeBasicConfig();
    config.iterations = 0;

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK_FALSE(result.success);
    FL_CHECK(result.error_message.has_value());
}

FL_TEST_CASE("Invalid iterations - negative") {
    SingleTestConfig config = makeBasicConfig();
    config.iterations = -1;

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK_FALSE(result.success);
    FL_CHECK(result.error_message.has_value());
}

FL_TEST_CASE("Invalid pattern - empty") {
    SingleTestConfig config = makeBasicConfig();
    config.pattern = "";

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK_FALSE(result.success);
    FL_CHECK(result.error_message.has_value());
}

FL_TEST_CASE("Large lane count - 16 lanes (maximum allowed)") {
    SingleTestConfig config = makeBasicConfig();
    config.lane_sizes = {
        100, 100, 100, 100, 100, 100, 100, 100,
        100, 100, 100, 100, 100, 100, 100, 100}; // 16 lanes

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK(result.success);
    FL_CHECK(result.lane_count == 16);
}

} // FL_TEST_FILE
