/// @file rx_device.cpp
/// @brief Unit tests for RxDevice interface and factory

#include "fl/rx_device.h"
#include "fl/stl/cstring.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/slice.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"

using namespace fl;

TEST_CASE("RxDevice - default template returns dummy device") {
    // On non-ESP32 platforms, default template returns dummy
    auto device = RxDevice::create<RxDeviceType::RMT>(6);  // GPIO 6

    REQUIRE(device != nullptr);
#ifndef ESP32
    CHECK(fl::strcmp(device->name(), "dummy") == 0);
#endif
}

TEST_CASE("RxDevice - dummy device returns failures") {
    // On non-ESP32 platforms, create() returns dummy device
    auto device = RxDevice::create<RxDeviceType::RMT>(6);  // GPIO 6
    REQUIRE(device != nullptr);

    // begin() should return false
    RxConfig config;
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

TEST_CASE("RxDevice - template factory creates devices by type") {
    // Template-based factory should work with enum
    auto rmt_device = RxDevice::create<RxDeviceType::RMT>(6);  // GPIO 6
    auto isr_device = RxDevice::create<RxDeviceType::ISR>(7);  // GPIO 7

    REQUIRE(rmt_device != nullptr);
    REQUIRE(isr_device != nullptr);

#ifdef ESP32
    // On ESP32, these should be real device names
    // Note: These may be "dummy" if creation fails, which is acceptable
    CHECK(rmt_device->name() != nullptr);
    CHECK(isr_device->name() != nullptr);
#else
    // On non-ESP32 platforms, should return dummy devices
    CHECK(fl::strcmp(rmt_device->name(), "dummy") == 0);
    CHECK(fl::strcmp(isr_device->name(), "dummy") == 0);
#endif
}

TEST_CASE("RxDevice - createDummy returns shared singleton instance") {
    // createDummy should return same instance each time (via create template default)
    auto dummy1 = RxDevice::create<RxDeviceType::RMT>(6);  // GPIO 6
    auto dummy2 = RxDevice::create<RxDeviceType::ISR>(7);  // GPIO 7

    REQUIRE(dummy1 != nullptr);
    REQUIRE(dummy2 != nullptr);

#ifndef ESP32
    // On non-ESP32 platforms, both should be the same dummy instance
    CHECK(dummy1.get() == dummy2.get());
    CHECK(fl::strcmp(dummy1->name(), "dummy") == 0);
    CHECK(fl::strcmp(dummy2->name(), "dummy") == 0);
#endif
}
