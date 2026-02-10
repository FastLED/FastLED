// tests/fl/channels/validation.cpp
//
// Unit tests for validation logic

#include "test.h"
#include "fl/channels/validation.h"
#include "fl/channels/validation.cpp.hpp"

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

FL_TEST_CASE("Invalid lane count - more than 8 lanes") {
    SingleTestConfig config = makeBasicConfig();
    config.lane_sizes = {100, 100, 100, 100, 100, 100, 100, 100, 100}; // 9 lanes

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK_FALSE(result.success);
    FL_CHECK(result.error_message.has_value());
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

FL_TEST_CASE("Large lane count - 8 lanes (maximum allowed)") {
    SingleTestConfig config = makeBasicConfig();
    config.lane_sizes = {100, 100, 100, 100, 100, 100, 100, 100}; // 8 lanes

    SingleTestResult result = runSingleValidationTest(config);

    FL_CHECK(result.success);
    FL_CHECK(result.lane_count == 8);
}
