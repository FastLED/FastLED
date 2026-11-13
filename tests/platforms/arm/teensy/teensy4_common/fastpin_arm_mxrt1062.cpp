/// @file fastpin_arm_mxrt1062.cpp
/// @brief Runs the production generic software SPI writer through the real
/// i.MXRT1062 _ARMPIN template and BitbangPin policy on fake GPIO registers.
///
/// _ARMPIN writes through raw volatile pointers, which a host cannot intercept,
/// so these fixture pins shadow only fastset/hi/lo to record each store.

#include "test.h"

// Exercise the opt-in optimized bit-bang policy; it defaults off.
#ifndef FL_TEENSY_ENABLE_FAST_SOFTWARE_SPI
#define FL_TEENSY_ENABLE_FAST_SOFTWARE_SPI 1
#endif

#include "platforms/arm/teensy/teensy4_common/fastpin_arm_mxrt1062.h"

#include "fastspi_types.h"
#include "fl/stl/int.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {
namespace mxrt_test {

constexpr u32 kData = 1U << 3;
constexpr u32 kClock = 1U << 17;
constexpr u32 kOther = 0x80000001U; // Unrelated output bits owned by the sketch.

enum RegKind { kDr, kSet, kClear, kToggle };

template <int PORT, RegKind KIND> struct Reg {
    static volatile u32& r() {
        static volatile u32 value = 0;
        return value;
    }
};

struct Rising {
    bool bit;
    u32 setStores;   // Non-zero when data was raised through the set alias.
    u32 clearStores; // Non-zero when data was lowered through the clear alias.
};

fl::vector<u32> gFullPortStores;
fl::vector<Rising> gEdges;
int gClockAliasStores = 0;

template <u8 PIN, u32 MASK, int PORT>
using Base = _ARMPIN<PIN, 0, MASK, Reg<PORT, kDr>, Reg<PORT, kSet>, Reg<PORT, kClear>, Reg<PORT, kToggle>>;

constexpr u8 kDataPin = 240;
constexpr u8 kClockPin = 241;
constexpr u8 kOtherPortClockPin = 242;
constexpr u32 kDivider = 4;

} // namespace mxrt_test

template <> class FastPin<mxrt_test::kDataPin>
    : public mxrt_test::Base<mxrt_test::kDataPin, mxrt_test::kData, 0> {
    using B = mxrt_test::Base<mxrt_test::kDataPin, mxrt_test::kData, 0>;
  public:
    static void fastset(B::port_ptr_t port, B::port_t value) {
        B::fastset(port, value);
        mxrt_test::gFullPortStores.push_back(value);
        if (value & mxrt_test::kClock) {
            mxrt_test::gEdges.push_back({(value & mxrt_test::kData) != 0, 0, 0});
        }
    }
};

// Clock pins sample the data aliases on each rising edge, then clear them so
// the next edge shows only the store made for its own bit.
template <u8 PIN, int PORT> struct TracedClock : mxrt_test::Base<PIN, mxrt_test::kClock, PORT> {
    static void hi() {
        volatile u32& set = mxrt_test::Reg<0, mxrt_test::kSet>::r();
        volatile u32& clear = mxrt_test::Reg<0, mxrt_test::kClear>::r();
        const u32 s = set;
        const u32 c = clear;
        set = 0;
        clear = 0;
        mxrt_test::gEdges.push_back({s != 0, s, c});
        ++mxrt_test::gClockAliasStores;
        mxrt_test::Reg<PORT, mxrt_test::kDr>::r() =
            mxrt_test::Reg<PORT, mxrt_test::kDr>::r() | mxrt_test::kClock;
    }
    static void lo() {
        ++mxrt_test::gClockAliasStores;
        mxrt_test::Reg<PORT, mxrt_test::kDr>::r() =
            mxrt_test::Reg<PORT, mxrt_test::kDr>::r() & ~mxrt_test::kClock;
    }
};

template <> class FastPin<mxrt_test::kClockPin> : public TracedClock<mxrt_test::kClockPin, 0> {};
template <> class FastPin<mxrt_test::kOtherPortClockPin> : public TracedClock<mxrt_test::kOtherPortClockPin, 1> {};

FL_STATIC_ASSERT(BitbangPin<FastPin<mxrt_test::kDataPin>>::kTwoWriteStream,
                 "i.MXRT hardware pins must stream on an exclusive same-port bus");
FL_STATIC_ASSERT(BitbangPin<FastPin<mxrt_test::kDataPin>>::kAtomicWrite,
                 "i.MXRT hardware pins must update data through set/clear aliases");
FL_STATIC_ASSERT(!BitbangPin<FastPin<3>>::kTwoWriteStream && !BitbangPin<FastPin<3>>::kAtomicWrite,
                 "pins not backed by the i.MXRT _ARMPIN keep the default policy");

FL_TEST_FILE(FL_FILEPATH) {
using namespace mxrt_test;

using SamePort = GenericSoftwareSPIOutput<kDataPin, kClockPin, kDivider>;
using SplitPort = GenericSoftwareSPIOutput<kDataPin, kOtherPortClockPin, kDivider>;

namespace {

u32 dr0() { return Reg<0, kDr>::r(); }
u32 dr1() { return Reg<1, kDr>::r(); }

void reset() {
    Reg<0, kDr>::r() = kOther;
    Reg<1, kDr>::r() = kOther;
    Reg<0, kSet>::r() = 0;
    Reg<0, kClear>::r() = 0;
    gFullPortStores.clear();
    gEdges.clear();
    gClockAliasStores = 0;
}

fl::vector<u8> edgeBytes() {
    fl::vector<u8> bytes;
    u8 current = 0;
    int bits = 0;
    for (const Rising& edge : gEdges) {
        current = static_cast<u8>((current << 1) | (edge.bit ? 1 : 0));
        if (++bits == 8) {
            bytes.push_back(current);
            current = 0;
            bits = 0;
        }
    }
    return bytes;
}

void checkBytes(const fl::vector<u8>& expected) {
    const fl::vector<u8> seen = edgeBytes();
    FL_REQUIRE_EQ(seen.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        FL_CHECK_EQ(seen[i], expected[i]);
    }
}

void checkStream(const fl::vector<u8>& input) {
    FL_CHECK_EQ(gFullPortStores.size(), input.size() * 16);
    FL_CHECK_EQ(gClockAliasStores, input.empty() ? 0 : 1); // The final clock low.
    checkBytes(input);
    for (u32 stored : gFullPortStores) {
        FL_CHECK_EQ(stored & kOther, kOther);
    }
    FL_CHECK_EQ(dr0() & kClock, 0U);
    FL_CHECK_EQ(dr0() & kOther, kOther);
}

void checkAtomicEdges(const fl::vector<u8>& expected) {
    FL_CHECK(gFullPortStores.empty());
    checkBytes(expected);
    for (const Rising& edge : gEdges) {
        // Exactly the data mask, on exactly one alias, once per bit.
        FL_CHECK_EQ(edge.setStores, edge.bit ? kData : 0U);
        FL_CHECK_EQ(edge.clearStores, edge.bit ? 0U : kData);
    }
}

} // namespace

FL_TEST_CASE("i.MXRT same-port writeBytes streams two full-port stores per bit") {
    const fl::vector<fl::vector<u8>> inputs = {
        {}, {0x00}, {0xFF}, {0xAA}, {0x55}, {0x00, 0xFF, 0xAA, 0x55, 0x81, 0x7E},
    };
    for (const fl::vector<u8>& input : inputs) {
        reset();
        SamePort spi;
        fl::vector<u8> bytes = input;
        spi.writeBytes<DATA_NOP>(bytes.data(), static_cast<int>(bytes.size()));
        checkStream(input);
    }
}

FL_TEST_CASE("i.MXRT same-port writeBytesValueRaw and writePixels stream") {
    reset();
    SamePort::writeBytesValueRaw(0xA5, 3);
    checkStream({0xA5, 0xA5, 0xA5});

    CRGB leds[1] = {CRGB(0x12, 0x34, 0x56)};
    reset();
    SamePort spi;
    PixelController<RGB> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.writePixels<0, DATA_NOP, RGB>(pixels);
    checkStream({0x12, 0x34, 0x56});
}

FL_TEST_CASE("i.MXRT public writeByte uses set/clear aliases and never snapshots the port") {
    reset();
    SamePort::writeByte(0xA5);
    checkAtomicEdges({0xA5});
    FL_CHECK_EQ(dr0() & kClock, 0U);
    FL_CHECK_EQ(dr0(), kOther);
}

FL_TEST_CASE("i.MXRT split-port bulk writes keep concurrent unrelated changes") {
    reset();
    SplitPort spi;
    u8 input[] = {0xA5, 0x3C};
    spi.writeBytes<DATA_NOP>(input, 2);
    checkAtomicEdges({0xA5, 0x3C});

    // A latch bit changed while the bus runs survives: nothing rewrites DR.
    reset();
    Reg<0, kDr>::r() = kOther | (1U << 9);
    SplitPort::writeBytesValueRaw(0x5A, 1);
    FL_CHECK_EQ(dr0(), kOther | (1U << 9));
    FL_CHECK_EQ(dr1() & kClock, 0U);
}

} // FL_TEST_FILE

} // namespace fl
