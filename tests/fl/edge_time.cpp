/// @file edge_time.cpp
/// @brief Unit tests for EdgeTime packed structure

#include "fl/rx_device.h"
#include "fl/stl/stdint.h"
#include "doctest.h"

using namespace fl;

TEST_CASE("EdgeTime - size check") {
    // EdgeTime should be exactly 4 bytes (packed into uint32_t)
    CHECK(sizeof(EdgeTime) == 4);
}

TEST_CASE("EdgeTime - construction") {
    // Test construction with high=true
    EdgeTime e1(true, 400);
    uint32_t high1 = e1.high;
    uint32_t ns1 = e1.ns;
    CHECK(high1 == 1);
    CHECK(ns1 == 400);

    // Test construction with high=false
    EdgeTime e2(false, 850);
    uint32_t high2 = e2.high;
    uint32_t ns2 = e2.ns;
    CHECK(high2 == 0);
    CHECK(ns2 == 850);
}

TEST_CASE("EdgeTime - default constructor") {
    EdgeTime e;
    uint32_t high = e.high;
    uint32_t ns = e.ns;
    CHECK(high == 0);
    CHECK(ns == 0);
}

TEST_CASE("EdgeTime - max ns value") {
    // Maximum ns value is 31 bits (0x7FFFFFFF = 2147483647ns ~= 2.1 seconds)
    EdgeTime e(true, 0x7FFFFFFF);
    uint32_t high = e.high;
    uint32_t ns = e.ns;
    CHECK(high == 1);
    CHECK(ns == 0x7FFFFFFF);
}

TEST_CASE("EdgeTime - overflow masking") {
    // Values larger than 31 bits should be masked (automatically by bit field)
    EdgeTime e(true, 0xFFFFFFFF);
    uint32_t high = e.high;
    uint32_t ns = e.ns;
    CHECK(high == 1);
    CHECK(ns == 0x7FFFFFFF);  // Bit field automatically masks to 31 bits
}

TEST_CASE("EdgeTime - direct field access") {
    // With bit fields, we can directly modify fields
    EdgeTime e;
    e.high = 1;
    e.ns = 1250;
    uint32_t high = e.high;
    uint32_t ns = e.ns;
    CHECK(high == 1);
    CHECK(ns == 1250);

    // Toggle high, ns should be preserved
    e.high = 0;
    high = e.high;
    ns = e.ns;
    CHECK(high == 0);
    CHECK(ns == 1250);

    // Change ns, high should be preserved
    e.ns = 2500;
    high = e.high;
    ns = e.ns;
    CHECK(high == 0);
    CHECK(ns == 2500);
}

TEST_CASE("EdgeTime - WS2812B pattern") {
    // WS2812B typical bit patterns
    // Bit 0: 400ns high, 850ns low
    EdgeTime bit0_high(true, 400);
    EdgeTime bit0_low(false, 850);

    uint32_t high0h = bit0_high.high;
    uint32_t ns0h = bit0_high.ns;
    uint32_t high0l = bit0_low.high;
    uint32_t ns0l = bit0_low.ns;

    CHECK(high0h == 1);
    CHECK(ns0h == 400);
    CHECK(high0l == 0);
    CHECK(ns0l == 850);

    // Bit 1: 800ns high, 450ns low
    EdgeTime bit1_high(true, 800);
    EdgeTime bit1_low(false, 450);

    uint32_t high1h = bit1_high.high;
    uint32_t ns1h = bit1_high.ns;
    uint32_t high1l = bit1_low.high;
    uint32_t ns1l = bit1_low.ns;

    CHECK(high1h == 1);
    CHECK(ns1h == 800);
    CHECK(high1l == 0);
    CHECK(ns1l == 450);
}

TEST_CASE("EdgeTime - constexpr") {
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

    CHECK(high1 == 0);
    CHECK(ns1 == 0);
    CHECK(high2 == 1);
    CHECK(ns2 == 1000);
}
