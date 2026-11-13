/// @file avr_pin.cpp
/// @brief Traces the production generic software SPI writer through the real
/// _AVRPIN template and its BitbangPin policy on a fake 8-bit output latch.

#include "test.h"

// Exercise the opt-in optimized bit-bang policy; it defaults off.
#ifndef FL_AVR_ENABLE_FAST_SOFTWARE_SPI
#define FL_AVR_ENABLE_FAST_SOFTWARE_SPI 1
#endif

#include "platforms/avr/atmega/common/avr_pin.h"

#include "fastspi_types.h"
#include "fl/stl/int.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {
namespace avr_pin_test {

constexpr u8 kData = 0x08;
constexpr u8 kClock = 0x20;
constexpr u8 kOther = 0x81; // Unrelated output latch bits owned by the sketch.

struct Store {
    int port;
    u8 value;
};
fl::vector<Store> gStores;

template <int ID> struct Latch {
    u8 value = 0;
    operator u8() const { return value; }
    Latch& operator=(u8 next) {
        value = next;
        gStores.push_back(Store{ID, next});
        return *this;
    }
    Latch& operator|=(u8 bits) { return *this = value | bits; }
    Latch& operator&=(u8 bits) { return *this = value & bits; }
    volatile u8* operator&() { return reinterpret_cast<volatile u8*>(&value); }
};

template <int ID> struct Reg {
    static Latch<ID>& r() {
        static Latch<ID> latch;
        return latch;
    }
};

using PortA = Reg<0>;
using PortB = Reg<1>;
struct Unused {
    static u8& r() {
        static u8 value = 0;
        return value;
    }
};

constexpr u8 kDataPin = 240;
constexpr u8 kClockPin = 241;
constexpr u8 kOtherPortClockPin = 242;
constexpr u32 kDivider = 4;

} // namespace avr_pin_test

template <> class FastPin<avr_pin_test::kDataPin>
    : public _AVRPIN<avr_pin_test::kDataPin, avr_pin_test::kData,
                     avr_pin_test::PortA, avr_pin_test::Unused, avr_pin_test::Unused> {};
template <> class FastPin<avr_pin_test::kClockPin>
    : public _AVRPIN<avr_pin_test::kClockPin, avr_pin_test::kClock,
                     avr_pin_test::PortA, avr_pin_test::Unused, avr_pin_test::Unused> {};
template <> class FastPin<avr_pin_test::kOtherPortClockPin>
    : public _AVRPIN<avr_pin_test::kOtherPortClockPin, avr_pin_test::kClock,
                     avr_pin_test::PortB, avr_pin_test::Unused, avr_pin_test::Unused> {};

// Only real _AVRPIN hardware pins opt in; a software fallback pin never does.
FL_STATIC_ASSERT(BitbangPin<FastPin<avr_pin_test::kDataPin>>::kTwoWriteStream,
              "_AVRPIN hardware pins must stream");
FL_STATIC_ASSERT(!BitbangPin<FastPin<3>>::kTwoWriteStream,
              "pins not backed by _AVRPIN must not stream");
FL_STATIC_ASSERT(!BitbangPin<FastPin<avr_pin_test::kDataPin>>::kAtomicWrite,
              "the whole-latch path must stay selected on AVR");

FL_TEST_FILE(FL_FILEPATH) {
using namespace avr_pin_test;

using SamePort = GenericSoftwareSPIOutput<kDataPin, kClockPin, kDivider>;
using SplitPort = GenericSoftwareSPIOutput<kDataPin, kOtherPortClockPin, kDivider>;

namespace {

void reset() {
    PortA::r().value = kOther;
    PortB::r().value = kOther;
    gStores.clear();
}

// Bytes latched on each rising edge of the fake bus, MSB first.
fl::vector<u8> risingEdgeBytes(int clockPort = 0) {
    fl::vector<u8> bytes;
    u8 current = 0;
    int bits = 0;
    bool clock = false;
    u8 data = 0;
    for (const Store& s : gStores) {
        if (s.port == 0) data = s.value;
        const bool nextClock = (s.port == clockPort ? s.value & kClock : clock) != 0;
        if (nextClock && !clock) {
            current = static_cast<u8>((current << 1) | ((data & kData) ? 1 : 0));
            if (++bits == 8) {
                bytes.push_back(current);
                current = 0;
                bits = 0;
            }
        }
        clock = nextClock;
    }
    return bytes;
}

void checkStream(const fl::vector<u8>& input) {
    const size_t stores = gStores.size();
    FL_CHECK_EQ(stores, input.empty() ? size_t(0) : input.size() * 16 + 1);
    const fl::vector<u8> seen = risingEdgeBytes();
    FL_REQUIRE_EQ(seen.size(), input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        FL_CHECK_EQ(seen[i], input[i]);
    }
    for (const Store& s : gStores) {
        FL_CHECK_EQ(u8(s.value & kOther), kOther);
    }
    FL_CHECK_EQ(u8(PortA::r().value & kClock), u8(0));
    FL_CHECK_EQ(u8(PortA::r().value & kOther), kOther);
}

} // namespace

FL_TEST_CASE("AVR same-port writeBytes uses two latch stores per bit") {
    const fl::vector<fl::vector<u8>> inputs = {
        {}, {0x00}, {0xFF}, {0xAA}, {0x55}, {0x00, 0xFF, 0xAA, 0x55, 0x81, 0x7E},
    };
    for (const fl::vector<u8>& input : inputs) {
        reset();
        SamePort spi;
        fl::vector<u8> mutableInput = input;
        spi.writeBytes<DATA_NOP>(mutableInput.data(), static_cast<int>(mutableInput.size()));
        checkStream(input);
    }
}

FL_TEST_CASE("AVR same-port writeBytesValueRaw streams repeated values") {
    for (const u8 value : {u8(0x00), u8(0xFF), u8(0xA5)}) {
        for (const int count : {0, 1, 5}) {
            reset();
            SamePort::writeBytesValueRaw(value, count);
            checkStream(fl::vector<u8>(static_cast<size_t>(count), value));
        }
    }
}

FL_TEST_CASE("AVR same-port writePixels streams pixel bytes and start bits") {
    CRGB leds[2] = {CRGB(0x12, 0x34, 0x56), CRGB(0xFF, 0x00, 0xA5)};
    const ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    {
        reset();
        SamePort spi;
        PixelController<RGB> pixels(leds, 2, adjustment, DISABLE_DITHER);
        spi.writePixels<0, DATA_NOP, RGB>(pixels);
        checkStream({0x12, 0x34, 0x56, 0xFF, 0x00, 0xA5});
    }
    {
        reset();
        SamePort spi;
        PixelController<RGB> pixels(leds, 1, adjustment, DISABLE_DITHER);
        spi.writePixels<FLAG_START_BIT, DATA_NOP, RGB>(pixels);
        // One start bit plus three bytes: 25 bits of two stores, one final low.
        FL_CHECK_EQ(gStores.size(), size_t(25 * 2 + 1));
        FL_CHECK_EQ(u8(PortA::r().value & kClock), u8(0));
        FL_CHECK_EQ(u8(PortA::r().value & kOther), kOther);
    }
}

FL_TEST_CASE("AVR public writeByte keeps its per-bit path and ends clock low") {
    reset();
    SamePort::writeByte(0xA5);
    FL_CHECK_GT(gStores.size(), size_t(16));
    const fl::vector<u8> seen = risingEdgeBytes();
    FL_REQUIRE_EQ(seen.size(), size_t(1));
    FL_CHECK_EQ(seen[0], u8(0xA5));
    FL_CHECK_EQ(u8(PortA::r().value & kClock), u8(0));
}

FL_TEST_CASE("AVR split-port software SPI keeps its separate data and clock stores") {
    reset();
    SplitPort spi;
    u8 input[] = {0xA5};
    spi.writeBytes<DATA_NOP>(input, 1);
    int dataStores = 0;
    int clockStores = 0;
    for (const Store& s : gStores) {
        (s.port == 0 ? dataStores : clockStores) += 1;
    }
    FL_CHECK_EQ(dataStores, 8);
    FL_CHECK_EQ(clockStores, 16);
    const fl::vector<u8> seen = risingEdgeBytes(1);
    FL_REQUIRE_EQ(seen.size(), size_t(1));
    FL_CHECK_EQ(seen[0], u8(0xA5));
    FL_CHECK_EQ(u8(PortB::r().value & kClock), u8(0));
    FL_CHECK_EQ(u8(PortB::r().value & kOther), kOther);
}

} // FL_TEST_FILE

} // namespace fl
