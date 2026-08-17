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
