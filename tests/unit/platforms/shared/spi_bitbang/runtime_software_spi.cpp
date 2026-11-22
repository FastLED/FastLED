/// @file runtime_software_spi.cpp
/// @brief Runs the production runtime software SPI writer on fake set/clear
/// registers. A fixture clock samples the data registers at each rising edge.

#include "test.h"

#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/runtime_software_spi.h"

namespace fl {
namespace runtime_spi_test {

constexpr u32 kData = 1U << 5;
volatile u32 gSet = 0;
volatile u32 gClear = 0;

struct Edge {
    u32 set;
    u32 clear;
};
fl::vector<Edge> gEdges;
bool gClockHigh = true; // Starts high so the writer must park it low.
int gBadEdges = 0;

struct TracedClock {
    void hi() const {
        if (gClockHigh) {
            ++gBadEdges;
        }
        gClockHigh = true;
        gEdges.push_back({gSet, gClear});
        gSet = 0;
        gClear = 0;
    }
    void lo() const { gClockHigh = false; }
};

} // namespace runtime_spi_test

FL_TEST_FILE(FL_FILEPATH) {
using namespace runtime_spi_test;

namespace {

void run(const fl::vector<u8>& bytes) {
    gEdges.clear();
    gClockHigh = true;
    gBadEdges = 0;
    gSet = 0;
    gClear = 0;
    const RuntimeGpio data{&gSet, &gClear, kData};
    writeRuntimeSoftwareSpi(TracedClock{}, data, bytes.data(), bytes.size());
}

// One store per bit on exactly one register, MSB first. Every rising edge starts
// from low, and the clock ends low.
void check(const fl::vector<u8>& bytes) {
    FL_REQUIRE_EQ(gEdges.size(), bytes.size() * 8);
    for (size_t i = 0; i < gEdges.size(); ++i) {
        const bool bit = (bytes[i / 8] >> (7 - (i % 8))) & 1;
        FL_CHECK_EQ(gEdges[i].set, bit ? kData : 0U);
        FL_CHECK_EQ(gEdges[i].clear, bit ? 0U : kData);
    }
    FL_CHECK_EQ(gBadEdges, 0);
    FL_CHECK_FALSE(gClockHigh);
}

} // namespace

FL_TEST_CASE("Runtime software SPI writes all byte values") {
    for (int value = 0; value < 256; ++value) {
        const fl::vector<u8> bytes = {static_cast<u8>(value)};
        run(bytes);
        check(bytes);
    }
}

FL_TEST_CASE("Runtime software SPI handles empty and consecutive transfers") {
    run({});
    check({});

    const fl::vector<u8> first = {0xAA, 0x55, 0x00, 0xFF};
    run(first);
    check(first);
    const fl::vector<u8> second = {0x81};
    run(second);
    check(second);
}

FL_TEST_CASE("RuntimeGpio hi and lo store the mask to their own registers") {
    gSet = 0;
    gClear = 0;
    const RuntimeGpio pin{&gSet, &gClear, kData};
    pin.hi();
    FL_CHECK_EQ(gSet, kData);
    FL_CHECK_EQ(gClear, 0U);
    pin.lo();
    FL_CHECK_EQ(gClear, kData);
}

FL_TEST_CASE("Host pins resolve to no atomic registers, so the fallback stays") {
    RuntimeGpio pin{nullptr, nullptr, 0};
    FL_CHECK_FALSE(resolveRuntimeGpio(3, &pin));
}

} // FL_TEST_FILE

} // namespace fl
