/// @file fastpin_no_pinmap.cpp
/// @brief Runs the real GenericSoftwareSPIOutput on NoPinMapPin, sampling the stub
/// GPIO at each rising clock edge. Pins share a null port, so a whole-port path would
/// fault or corrupt the trace.

#include "test.h"

#include "FastLED.h"
#include "fl/stl/vector.h"
#include "fl/system/fastpin_no_pinmap.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"
#include "platforms/stub/stub_gpio.h"

namespace fl {

constexpr u8 kNoMapData = 240;
constexpr u8 kNoMapClock = 241;

template <> class FastPin<kNoMapData> : public NoPinMapPin<kNoMapData> {};
template <> class FastPin<kNoMapClock> : public NoPinMapPin<kNoMapClock> {};

FL_TEST_FILE(FL_FILEPATH) {

namespace {

using Output = GenericSoftwareSPIOutput<kNoMapData, kNoMapClock, 2>;

struct Trace {
    fl::vector<u8> bits;
    int falling = 0;
    int repeatedHigh = 0;
    bool clockHigh = false;
    Trace() {
        fl::stub::setPinEdgeCallback(kNoMapClock, [this](bool high, u32) FL_NO_EXCEPT {
            if (!high) {
                ++falling;
                clockHigh = false;
                return;
            }
            if (clockHigh) {
                ++repeatedHigh;
            }
            clockHigh = true;
            bits.push_back(fl::digitalRead(kNoMapData) == PinValue::High ? 1 : 0);
        });
    }
    ~Trace() { fl::stub::clearPinEdgeCallback(kNoMapClock); }
};

void checkBytes(const Trace& trace, const fl::vector<u8>& bytes) {
    FL_REQUIRE_EQ(trace.bits.size(), bytes.size() * 8);
    for (fl::size i = 0; i < trace.bits.size(); ++i) {
        FL_CHECK_EQ(trace.bits[i], static_cast<u8>((bytes[i / 8] >> (7 - (i % 8))) & 1));
    }
    FL_CHECK(fl::digitalRead(kNoMapClock) == PinValue::Low);
    FL_CHECK_EQ(trace.repeatedHigh, 0);
    FL_CHECK_GE(trace.falling, static_cast<int>(trace.bits.size()));
}

} // namespace

FL_TEST_CASE("No-pin-map pins have no port and always take per-pin writes") {
    FL_CHECK(FastPin<kNoMapData>::port() == nullptr);
    FL_CHECK(isNoPinMapPin(static_cast<const FastPin<kNoMapData>*>(nullptr)));
    FL_CHECK_FALSE(isNoPinMapPin(static_cast<const void*>(nullptr)));
}

FL_TEST_CASE("toggle and strobe use the last level written") {
    FastPin<kNoMapClock>::setOutput();
    FastPin<kNoMapClock>::lo();
    Trace trace;
    FastPin<kNoMapClock>::toggle();
    FL_CHECK(fl::digitalRead(kNoMapClock) == PinValue::High);
    FastPin<kNoMapClock>::toggle();
    FL_CHECK(fl::digitalRead(kNoMapClock) == PinValue::Low);
    FastPin<kNoMapClock>::strobe();
    FL_CHECK_EQ(trace.bits.size(), 2U);
    FL_CHECK_EQ(trace.falling, 2);
}

FL_TEST_CASE("writeByte sends all 256 values MSB first") {
    Output spi;
    spi.init();
    for (int value = 0; value < 256; ++value) {
        Trace trace;
        spi.writeByte(static_cast<u8>(value));
        checkBytes(trace, {static_cast<u8>(value)});
    }
}

FL_TEST_CASE("writeBytes and repeated values, including empty") {
    Output spi;
    spi.init();
    {
        Trace trace;
        u8 data[] = {0xA5, 0x00, 0xFF, 0x81};
        spi.writeBytes(data, 4);
        checkBytes(trace, {0xA5, 0x00, 0xFF, 0x81});
    }
    {
        Trace trace;
        u8 none = 0;
        spi.writeBytes(&none, 0);
        FL_CHECK_EQ(trace.bits.size(), 0U);
    }
    {
        Trace trace;
        spi.writeBytesValueRaw(0x3C, 3);
        checkBytes(trace, {0x3C, 0x3C, 0x3C});
    }
}

FL_TEST_CASE("writeBit selects each bit position") {
    Output spi;
    spi.init();
    Trace trace;
    spi.template writeBit<7>(0x80);
    spi.template writeBit<0>(0xFE);
    spi.template writeBit<3>(0x08);
    FL_REQUIRE_EQ(trace.bits.size(), 3U);
    FL_CHECK_EQ(trace.bits[0], 1);
    FL_CHECK_EQ(trace.bits[1], 0);
    FL_CHECK_EQ(trace.bits[2], 1);
}

FL_TEST_CASE("writePixels with and without a start bit") {
    Output spi;
    spi.init();
    CRGB leds[2] = {CRGB(0x12, 0x34, 0x56), CRGB(0x9A, 0xBC, 0xDE)};
    {
        Trace trace;
        PixelController<RGB> pixels(leds, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        spi.template writePixels<0, DATA_NOP, RGB>(pixels);
        checkBytes(trace, {0x12, 0x34, 0x56, 0x9A, 0xBC, 0xDE});
    }
    {
        Trace trace;
        PixelController<RGB> pixels(leds, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        spi.template writePixels<FLAG_START_BIT, DATA_NOP, RGB>(pixels);
        // Each pixel is a 1 start bit, then its three bytes.
        const u8 expected[2][3] = {{0x12, 0x34, 0x56}, {0x9A, 0xBC, 0xDE}};
        FL_REQUIRE_EQ(trace.bits.size(), 50U);
        for (int p = 0; p < 2; ++p) {
            FL_CHECK_EQ(trace.bits[p * 25], 1);
            for (int i = 0; i < 24; ++i) {
                FL_CHECK_EQ(trace.bits[p * 25 + 1 + i], static_cast<u8>((expected[p][i / 8] >> (7 - (i % 8))) & 1));
            }
        }
    }
}

} // FL_TEST_FILE

} // namespace fl
