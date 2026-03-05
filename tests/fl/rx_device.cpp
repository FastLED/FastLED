/// @file rx_device.cpp
/// @brief Unit tests for RxDevice interface, factory, and EdgeTime packed structure

#include "fl/rx_device.h"
#include "fl/stl/cstring.h"
#include "fl/stl/new.h"
#include "fl/stl/stdint.h"
#include "test.h"
#include "fl/stl/span.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/is_platform.h"

using namespace fl;

FL_TEST_CASE("RxDevice - default template returns non-null device") {
    // Factory always returns a non-null device on all platforms
    auto device = RxDevice::create<RxDeviceType::RMT>(6);  // GPIO 6
    FL_REQUIRE(device != nullptr);
    FL_REQUIRE(device->name() != nullptr);

#ifdef FL_IS_STUB
    // On stub platform, returns NativeRxDevice
    FL_CHECK(fl::strcmp(device->name(), "native") == 0);
#elif defined(ESP32)
    // On ESP32, returns real device (name varies)
    FL_CHECK(device->name() != nullptr);
#else
    // On other platforms, returns dummy device
    FL_CHECK(fl::strcmp(device->name(), "dummy") == 0);
#endif
}

FL_TEST_CASE("RxDevice - non-stub non-esp32 returns dummy that fails") {
#if !defined(FL_IS_STUB) && !defined(ESP32)
    // On non-ESP32, non-stub platforms, factory returns dummy device
    auto device = RxDevice::create<RxDeviceType::RMT>(6);
    FL_REQUIRE(device != nullptr);
    FL_CHECK(fl::strcmp(device->name(), "dummy") == 0);

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
#endif
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
#elif defined(FL_IS_STUB)
    // On stub platform, returns NativeRxDevice
    FL_CHECK(fl::strcmp(rmt_device->name(), "native") == 0);
    FL_CHECK(fl::strcmp(isr_device->name(), "native") == 0);
#else
    // On other non-ESP32 platforms, should return dummy devices
    FL_CHECK(fl::strcmp(rmt_device->name(), "dummy") == 0);
    FL_CHECK(fl::strcmp(isr_device->name(), "dummy") == 0);
#endif
}

FL_TEST_CASE("RxDevice - non-stub non-esp32 returns singleton dummy") {
#if !defined(FL_IS_STUB) && !defined(ESP32)
    // On non-ESP32, non-stub platforms, factory returns singleton dummy
    auto dummy1 = RxDevice::create<RxDeviceType::RMT>(6);
    auto dummy2 = RxDevice::create<RxDeviceType::ISR>(7);

    FL_REQUIRE(dummy1 != nullptr);
    FL_REQUIRE(dummy2 != nullptr);

    // On non-ESP32 non-stub, both should be the same dummy singleton instance
    FL_CHECK(dummy1.get() == dummy2.get());
    FL_CHECK(fl::strcmp(dummy1->name(), "dummy") == 0);
    FL_CHECK(fl::strcmp(dummy2->name(), "dummy") == 0);
#endif
}

// ============================================================
// EdgeTime packed structure tests (merged from edge_time.cpp)
// ============================================================

FL_TEST_CASE("EdgeTime - size check") {
    // EdgeTime should be exactly 4 bytes (packed into uint32_t)
    FL_CHECK(sizeof(EdgeTime) == 4);
}

FL_TEST_CASE("EdgeTime - construction") {
    // Test construction with high=true
    EdgeTime e1(true, 400);
    uint32_t high1 = e1.high;
    uint32_t ns1 = e1.ns;
    FL_CHECK(high1 == 1);
    FL_CHECK(ns1 == 400);

    // Test construction with high=false
    EdgeTime e2(false, 850);
    uint32_t high2 = e2.high;
    uint32_t ns2 = e2.ns;
    FL_CHECK(high2 == 0);
    FL_CHECK(ns2 == 850);
}

FL_TEST_CASE("EdgeTime - default constructor") {
    EdgeTime e;
    uint32_t high = e.high;
    uint32_t ns = e.ns;
    FL_CHECK(high == 0);
    FL_CHECK(ns == 0);
}

FL_TEST_CASE("EdgeTime - max ns value") {
    // Maximum ns value is 31 bits (0x7FFFFFFF = 2147483647ns ~= 2.1 seconds)
    EdgeTime e(true, 0x7FFFFFFF);
    uint32_t high = e.high;
    uint32_t ns = e.ns;
    FL_CHECK(high == 1);
    FL_CHECK(ns == 0x7FFFFFFF);
}

FL_TEST_CASE("EdgeTime - overflow masking") {
    // Values larger than 31 bits should be masked (automatically by bit field)
    EdgeTime e(true, 0xFFFFFFFF);
    uint32_t high = e.high;
    uint32_t ns = e.ns;
    FL_CHECK(high == 1);
    FL_CHECK(ns == 0x7FFFFFFF);  // Bit field automatically masks to 31 bits
}

FL_TEST_CASE("EdgeTime - direct field access") {
    // With bit fields, we can directly modify fields
    EdgeTime e;
    e.high = 1;
    e.ns = 1250;
    uint32_t high = e.high;
    uint32_t ns = e.ns;
    FL_CHECK(high == 1);
    FL_CHECK(ns == 1250);

    // Toggle high, ns should be preserved
    e.high = 0;
    high = e.high;
    ns = e.ns;
    FL_CHECK(high == 0);
    FL_CHECK(ns == 1250);

    // Change ns, high should be preserved
    e.ns = 2500;
    high = e.high;
    ns = e.ns;
    FL_CHECK(high == 0);
    FL_CHECK(ns == 2500);
}

FL_TEST_CASE("EdgeTime - WS2812B pattern") {
    // WS2812B typical bit patterns
    // Bit 0: 400ns high, 850ns low
    EdgeTime bit0_high(true, 400);
    EdgeTime bit0_low(false, 850);

    uint32_t high0h = bit0_high.high;
    uint32_t ns0h = bit0_high.ns;
    uint32_t high0l = bit0_low.high;
    uint32_t ns0l = bit0_low.ns;

    FL_CHECK(high0h == 1);
    FL_CHECK(ns0h == 400);
    FL_CHECK(high0l == 0);
    FL_CHECK(ns0l == 850);

    // Bit 1: 800ns high, 450ns low
    EdgeTime bit1_high(true, 800);
    EdgeTime bit1_low(false, 450);

    uint32_t high1h = bit1_high.high;
    uint32_t ns1h = bit1_high.ns;
    uint32_t high1l = bit1_low.high;
    uint32_t ns1l = bit1_low.ns;

    FL_CHECK(high1h == 1);
    FL_CHECK(ns1h == 800);
    FL_CHECK(high1l == 0);
    FL_CHECK(ns1l == 450);
}

FL_TEST_CASE("EdgeTime - constexpr") {
    // Ensure constexpr constructors work at compile time
    constexpr EdgeTime e1;
    static_assert(e1.high == 0, "Default constructor should create low edge");
    static_assert(e1.ns == 0, "Default constructor should create 0ns duration");

    constexpr EdgeTime e2(true, 1000);
    static_assert(e2.high == 1, "Constructor should set high flag");
    static_assert(e2.ns == 1000, "Constructor should set ns value");

    // Runtime checks to ensure constexpr works correctly
    uint32_t high1 = e1.high;
    uint32_t ns1 = e1.ns;
    uint32_t high2 = e2.high;
    uint32_t ns2 = e2.ns;

    FL_CHECK(high1 == 0);
    FL_CHECK(ns1 == 0);
    FL_CHECK(high2 == 1);
    FL_CHECK(ns2 == 1000);
}
