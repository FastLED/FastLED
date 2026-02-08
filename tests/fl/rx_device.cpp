/// @file rx_device.cpp
/// @brief Unit tests for RxDevice interface and factory

#include "fl/rx_device.h"
#include "fl/stl/cstring.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/slice.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"

using namespace fl;

FL_TEST_CASE("RxDevice - default template returns dummy device") {
    // On non-ESP32 platforms, default template returns dummy
    auto device = RxDevice::create<RxDeviceType::RMT>(6);  // GPIO 6

    FL_REQUIRE(device != nullptr);
#ifndef ESP32
    FL_CHECK(fl::strcmp(device->name(), "dummy") == 0);
#endif
}

FL_TEST_CASE("RxDevice - dummy device returns failures") {
    // On non-ESP32 platforms, create() returns dummy device
    auto device = RxDevice::create<RxDeviceType::RMT>(6);  // GPIO 6
    FL_REQUIRE(device != nullptr);

    // begin() should return false
    RxConfig config;
    config.buffer_size = 512;
    FL_CHECK(device->begin(config) == false);

    // finished() should return true (always "done")
    FL_CHECK(device->finished() == true);

    // wait() should timeout
    FL_CHECK(device->wait(100) == RxWaitResult::TIMEOUT);

    // decode() should fail
    ChipsetTiming4Phase timing{};
    uint8_t buffer[10];
    auto result = device->decode(timing, buffer);
    FL_CHECK(!result.ok());
    FL_CHECK(result.error() == DecodeError::INVALID_ARGUMENT);

    // getRawEdgeTimes should return 0
    EdgeTime edges[10];
    size_t count = device->getRawEdgeTimes(edges);
    FL_CHECK(count == 0);
}

FL_TEST_CASE("RxDevice - template factory creates devices by type") {
    // Template-based factory should work with enum
    auto rmt_device = RxDevice::create<RxDeviceType::RMT>(6);  // GPIO 6
    auto isr_device = RxDevice::create<RxDeviceType::ISR>(7);  // GPIO 7

    FL_REQUIRE(rmt_device != nullptr);
    FL_REQUIRE(isr_device != nullptr);

#ifdef ESP32
    // On ESP32, these should be real device names
    // Note: These may be "dummy" if creation fails, which is acceptable
    FL_CHECK(rmt_device->name() != nullptr);
    FL_CHECK(isr_device->name() != nullptr);
#else
    // On non-ESP32 platforms, should return dummy devices
    FL_CHECK(fl::strcmp(rmt_device->name(), "dummy") == 0);
    FL_CHECK(fl::strcmp(isr_device->name(), "dummy") == 0);
#endif
}

FL_TEST_CASE("RxDevice - createDummy returns shared singleton instance") {
    // createDummy should return same instance each time (via create template default)
    auto dummy1 = RxDevice::create<RxDeviceType::RMT>(6);  // GPIO 6
    auto dummy2 = RxDevice::create<RxDeviceType::ISR>(7);  // GPIO 7

    FL_REQUIRE(dummy1 != nullptr);
    FL_REQUIRE(dummy2 != nullptr);

#ifndef ESP32
    // On non-ESP32 platforms, both should be the same dummy instance
    FL_CHECK(dummy1.get() == dummy2.get());
    FL_CHECK(fl::strcmp(dummy1->name(), "dummy") == 0);
    FL_CHECK(fl::strcmp(dummy2->name(), "dummy") == 0);
#endif
}
