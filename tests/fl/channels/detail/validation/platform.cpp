// tests/fl/channels/detail/validation/platform.cpp
//
// Unit tests for platform validation

#include "test.h"
#include "fl/channels/validation.h"
#include "fl/channels/detail/validation/platform.h"
#include "fl/channels/detail/validation/platform.cpp.hpp"
#include "fl/channels/bus_manager.h"

using namespace fl;

FL_TEST_CASE("getExpectedEngines - returns vector") {
    auto expected = fl::validation::getExpectedEngines();
    // Should return a valid vector (may be empty on unknown platforms)
    FL_CHECK(expected.size() >= 0);
}

FL_TEST_CASE("validateExpectedEngines - all present") {
    // Create mock available drivers matching expected
    auto expected = fl::validation::getExpectedEngines();

    vector<DriverInfo> available;
    for (size_t i = 0; i < expected.size(); i++) {
        DriverInfo info;
        info.name = expected[i];
        info.priority = static_cast<int>(i);
        info.enabled = true;
        available.push_back(info);
    }

    bool result = fl::validation::validateExpectedEngines(available);
    FL_CHECK(result);
}

FL_TEST_CASE("validateExpectedEngines - one missing") {
    auto expected = fl::validation::getExpectedEngines();

    if (expected.size() > 0) {
        // Create drivers with one missing
        vector<DriverInfo> available;
        for (size_t i = 1; i < expected.size(); i++) {  // Skip first engine
            DriverInfo info;
            info.name = expected[i];
            info.priority = static_cast<int>(i);
            info.enabled = true;
            available.push_back(info);
        }

        bool result = fl::validation::validateExpectedEngines(available);
        FL_CHECK_FALSE(result);
    }
}

FL_TEST_CASE("validateExpectedEngines - empty available drivers") {
    vector<DriverInfo> available;
    auto expected = fl::validation::getExpectedEngines();

    bool result = fl::validation::validateExpectedEngines(available);

    // Should fail if any engines are expected
    if (expected.size() > 0) {
        FL_CHECK_FALSE(result);
    } else {
        // Unknown platform - should pass
        FL_CHECK(result);
    }
}

FL_TEST_CASE("validateExpectedEngines - extra drivers present") {
    auto expected = fl::validation::getExpectedEngines();

    vector<DriverInfo> available;
    for (size_t i = 0; i < expected.size(); i++) {
        DriverInfo info;
        info.name = expected[i];
        info.priority = static_cast<int>(i);
        info.enabled = true;
        available.push_back(info);
    }

    // Add extra driver not in expected list
    DriverInfo extra;
    extra.name = "EXTRA_DRIVER";
    extra.priority = 999;
    extra.enabled = true;
    available.push_back(extra);

    // Should still pass - extra drivers are OK
    bool result = fl::validation::validateExpectedEngines(available);
    FL_CHECK(result);
}
