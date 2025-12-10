/// @file rx_device.cpp
/// @brief Unit tests for RxDevice interface and factory

#include "test.h"
#include "fl/rx_device.h"
#include "platforms/shared/rx_device_dummy.h"
#include "ftl/cstring.h"

using namespace fl;

TEST_CASE("RxDevice - factory creates dummy for invalid type") {
    auto device = RxDevice::create("invalid_type");

    REQUIRE(device != nullptr);
    CHECK(fl::strcmp(device->name(), "dummy") == 0);
}

TEST_CASE("RxDevice - dummy device returns failures") {
    auto device = RxDevice::create("invalid_type");
    REQUIRE(device != nullptr);

    // begin() should return false
    RxConfig config;
    config.pin = 6;
    config.buffer_size = 512;
    CHECK(device->begin(config) == false);

    // finished() should return true (always "done")
    CHECK(device->finished() == true);

    // wait() should timeout
    CHECK(device->wait(100) == RxWaitResult::TIMEOUT);

    // decode() should fail
    ChipsetTiming4Phase timing{};
    uint8_t buffer[10];
    auto result = device->decode(timing, buffer);
    CHECK(!result.ok());
    CHECK(result.error() == DecodeError::INVALID_ARGUMENT);

    // getRawEdgeTimes should return 0
    EdgeTime edges[10];
    size_t count = device->getRawEdgeTimes(edges);
    CHECK(count == 0);
}
