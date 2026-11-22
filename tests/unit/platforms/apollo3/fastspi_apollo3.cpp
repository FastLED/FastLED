/// @file fastspi_apollo3.cpp
/// @brief Runs the production Apollo3 software SPI writer on traced fake pins.
///
/// The fixture pins replace the Ambiq fastgpio HAL calls. The writer and its
/// delay selection are production code, but on the host the counted loop is a
/// plain C++ loop, so target timing comes from disassembly instead.

#include "test.h"

#define FASTLED_APOLLO3
#include "platforms/apollo3/fastspi_apollo3.h"
#undef FASTLED_ALL_PINS_HARDWARE_SPI

#include "fl/stl/int.h"
#include "fl/stl/vector.h"

namespace fl {
namespace apollo3_test {

constexpr u8 kDataPin = 240;
constexpr u8 kClockPin = 241;
bool gData = false;
bool gClock = false;
fl::vector<u8> gBits;

} // namespace apollo3_test

template <> class FastPin<apollo3_test::kDataPin> {
  public:
    static void setOutput() {}
    static void hi() { apollo3_test::gData = true; }
    static void lo() { apollo3_test::gData = false; }
};

template <> class FastPin<apollo3_test::kClockPin> {
  public:
    static void setOutput() {}
    static void hi() {
        apollo3_test::gClock = true;
        apollo3_test::gBits.push_back(apollo3_test::gData ? 1 : 0);
    }
    static void lo() { apollo3_test::gClock = false; }
};

FL_TEST_FILE(FL_FILEPATH) {
using namespace apollo3_test;

namespace {

void reset() {
    gData = false;
    gClock = false;
    gBits.clear();
}

void check(const fl::vector<u8>& expected) {
    FL_REQUIRE_EQ(gBits.size(), expected.size() * 8);
    for (size_t i = 0; i < gBits.size(); ++i) {
        FL_CHECK_EQ(gBits[i], (expected[i / 8] >> (7 - (i % 8))) & 1);
    }
    FL_CHECK_FALSE(gClock);
}

// Exercises each delay branch: zero, odd, largest inline and counted-loop phases.
template <u32 DIVIDER> void checkDivider() {
    using Spi = APOLLO3HardwareSPIOutput<kDataPin, kClockPin, DIVIDER>;
    for (int value = 0; value < 256; ++value) {
        reset();
        Spi::writeByte(static_cast<u8>(value));
        check({static_cast<u8>(value)});
    }
    reset();
    Spi::writeBytesValueRaw(0xA5, 0);
    FL_CHECK(gBits.empty());
    Spi::writeBytesValueRaw(0xA5, 3);
    check({0xA5, 0xA5, 0xA5});

    reset();
    Spi spi;
    u8 input[] = {0x00, 0xFF, 0x81};
    spi.template writeBytes<DATA_NOP>(input, 3);
    check({0x00, 0xFF, 0x81});
}

} // namespace

FL_TEST_CASE("Apollo3 phase rounds odd dividers up") {
    FL_CHECK_EQ(apollo3SpiPhaseCycles(0), 0U);
    FL_CHECK_EQ(apollo3SpiPhaseCycles(1), 1U);
    FL_CHECK_EQ(apollo3SpiPhaseCycles(4), 2U);
    FL_CHECK_EQ(apollo3SpiPhaseCycles(5), 3U);
}

FL_TEST_CASE("Apollo3 loop count meets the cycle floor without excess") {
    FL_CHECK_EQ(apollo3SpiLoopCount(0), 0U);
    for (u32 cycles = 1; cycles <= 3000; ++cycles) {
        const u32 count = apollo3SpiLoopCount(cycles);
        FL_CHECK_GE(count * 3 - 1, cycles); // Final BNE falls through in 1 cycle.
        FL_CHECK_LT((count - 1) * 3 - (count > 1 ? 1 : 0), cycles);
    }
}

FL_TEST_CASE("Apollo3 writer keeps MSB-first data at every delay boundary") {
    checkDivider<0>();
    checkDivider<1>();
    checkDivider<5>();
    checkDivider<16>();
    checkDivider<17>();
    checkDivider<1000>();
}

FL_TEST_CASE("Apollo3 writePixels sends pixels with and without a start bit") {
    using Spi = APOLLO3HardwareSPIOutput<kDataPin, kClockPin, 4>;
    CRGB leds[1] = {CRGB(0x12, 0x34, 0x56)};
    Spi spi;
    reset();
    PixelController<RGB> plain(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.template writePixels<0, DATA_NOP, RGB>(plain);
    check({0x12, 0x34, 0x56});

    reset();
    PixelController<RGB> flagged(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.template writePixels<FLAG_START_BIT, DATA_NOP, RGB>(flagged);
    FL_REQUIRE_EQ(gBits.size(), 25U);
    FL_CHECK_EQ(gBits[0], 1);
    const u8 payload[] = {0x12, 0x34, 0x56};
    for (size_t i = 0; i < 24; ++i) {
        FL_CHECK_EQ(gBits[i + 1], (payload[i / 8] >> (7 - (i % 8))) & 1);
    }
}

} // FL_TEST_FILE

} // namespace fl
