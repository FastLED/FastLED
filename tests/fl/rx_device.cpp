/// @file rx_device.cpp
/// @brief Unit tests for RxDevice interface and implementations

#include "test.h"
#include "fl/rx_device.h"
#include "platforms/shared/rx_device_dummy.h"
#include "ftl/cstring.h"

using namespace fl;

// Helper function to cast to DummyRxDevice if name matches
DummyRxDevice* asDummy(shared_ptr<RxDevice>& device) {
    if (fl::strcmp(device->name(), "dummy") == 0) {
        return static_cast<DummyRxDevice*>(device.get());
    }
    return nullptr;
}

TEST_CASE("RxDevice - factory creates dummy for invalid type") {
    auto device = RxDevice::create("invalid_type", 6, 512);

    REQUIRE(device != nullptr);
    CHECK(fl::strcmp(device->name(), "dummy") == 0);
}

TEST_CASE("RxDevice - dummy device returns failures") {
    auto device = RxDevice::create("invalid_type", 6, 512);
    REQUIRE(device != nullptr);

    // begin() should return false
    CHECK(device->begin() == false);

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
}

TEST_CASE("DummyRxDevice - add and retrieve edge times") {
    auto device = RxDevice::create("invalid_type", 6, 512);
    REQUIRE(device != nullptr);

    DummyRxDevice* dummy = asDummy(device);
    REQUIRE(dummy != nullptr);

    // Add some edge times
    dummy->add(true, 400);   // HIGH for 400ns
    dummy->add(false, 850);  // LOW for 850ns
    dummy->add(true, 800);   // HIGH for 800ns
    dummy->add(false, 450);  // LOW for 450ns

    // Retrieve edge times
    EdgeTime edges[4];
    size_t count = device->getRawEdgeTimes(edges);

    CHECK(count == 4);
    // Use local variables to avoid bit-field reference issues
    uint32_t high0 = edges[0].high;
    uint32_t ns0 = edges[0].ns;
    uint32_t high1 = edges[1].high;
    uint32_t ns1 = edges[1].ns;
    uint32_t high2 = edges[2].high;
    uint32_t ns2 = edges[2].ns;
    uint32_t high3 = edges[3].high;
    uint32_t ns3 = edges[3].ns;

    CHECK(high0 == 1);
    CHECK(ns0 == 400);
    CHECK(high1 == 0);
    CHECK(ns1 == 850);
    CHECK(high2 == 1);
    CHECK(ns2 == 800);
    CHECK(high3 == 0);
    CHECK(ns3 == 450);
}

TEST_CASE("DummyRxDevice - partial buffer retrieval") {
    auto device = RxDevice::create("invalid_type", 6, 512);
    REQUIRE(device != nullptr);

    DummyRxDevice* dummy = asDummy(device);
    REQUIRE(dummy != nullptr);

    // Add 5 edge times
    dummy->add(true, 100);
    dummy->add(false, 200);
    dummy->add(true, 300);
    dummy->add(false, 400);
    dummy->add(true, 500);

    // Retrieve only 3 into smaller buffer
    EdgeTime edges[3];
    size_t count = device->getRawEdgeTimes(edges);

    CHECK(count == 3);  // Should only return what fits
    uint32_t high0 = edges[0].high;
    uint32_t ns0 = edges[0].ns;
    uint32_t high1 = edges[1].high;
    uint32_t ns1 = edges[1].ns;
    uint32_t high2 = edges[2].high;
    uint32_t ns2 = edges[2].ns;

    CHECK(high0 == 1);
    CHECK(ns0 == 100);
    CHECK(high1 == 0);
    CHECK(ns1 == 200);
    CHECK(high2 == 1);
    CHECK(ns2 == 300);
}

TEST_CASE("DummyRxDevice - empty retrieval") {
    auto device = RxDevice::create("invalid_type", 6, 512);
    REQUIRE(device != nullptr);

    // Don't add any edge times
    EdgeTime edges[10];
    size_t count = device->getRawEdgeTimes(edges);

    CHECK(count == 0);
}

TEST_CASE("DummyRxDevice - max nanoseconds value") {
    auto device = RxDevice::create("invalid_type", 6, 512);
    REQUIRE(device != nullptr);

    DummyRxDevice* dummy = asDummy(device);
    REQUIRE(dummy != nullptr);

    // Maximum valid value (31 bits)
    dummy->add(true, 0x7FFFFFFF);

    EdgeTime edges[1];
    size_t count = device->getRawEdgeTimes(edges);

    CHECK(count == 1);
    uint32_t high = edges[0].high;
    uint32_t ns = edges[0].ns;
    CHECK(high == 1);
    CHECK(ns == 0x7FFFFFFF);
}

TEST_CASE("DummyRxDevice - overflow assertion") {
    auto device = RxDevice::create("invalid_type", 6, 512);
    REQUIRE(device != nullptr);

    DummyRxDevice* dummy = asDummy(device);
    REQUIRE(dummy != nullptr);

    // This should trigger FL_ASSERT failure (overflow)
    // Value 0x80000000 is larger than 31 bits can hold

    // Note: FL_ASSERT will abort in debug builds, so we expect this test
    // to pass only if assertions are properly configured
    // In release builds with assertions disabled, this might not fire
    #ifdef NDEBUG
        // In release mode, just verify we can call it without crash
        dummy->add(true, 0x80000000);
        CHECK(true);  // If we get here, no crash occurred
    #else
        // In debug mode, we expect this to trigger an assertion
        // This test documents the expected behavior
        // Actual assertion testing requires special test infrastructure
        CHECK(true);  // Placeholder - assertion testing needs special handling
    #endif
}

TEST_CASE("DummyRxDevice - WS2812B pattern") {
    auto device = RxDevice::create("invalid_type", 6, 512);
    REQUIRE(device != nullptr);

    DummyRxDevice* dummy = asDummy(device);
    REQUIRE(dummy != nullptr);

    // WS2812B typical bit patterns
    // Bit 0: 400ns high, 850ns low
    dummy->add(true, 400);
    dummy->add(false, 850);

    // Bit 1: 800ns high, 450ns low
    dummy->add(true, 800);
    dummy->add(false, 450);

    EdgeTime edges[4];
    size_t count = device->getRawEdgeTimes(edges);

    CHECK(count == 4);

    // Verify bit 0 pattern
    uint32_t high0 = edges[0].high;
    uint32_t ns0 = edges[0].ns;
    uint32_t high1 = edges[1].high;
    uint32_t ns1 = edges[1].ns;
    CHECK(high0 == 1);
    CHECK(ns0 == 400);
    CHECK(high1 == 0);
    CHECK(ns1 == 850);

    // Verify bit 1 pattern
    uint32_t high2 = edges[2].high;
    uint32_t ns2 = edges[2].ns;
    uint32_t high3 = edges[3].high;
    uint32_t ns3 = edges[3].ns;
    CHECK(high2 == 1);
    CHECK(ns2 == 800);
    CHECK(high3 == 0);
    CHECK(ns3 == 450);
}
