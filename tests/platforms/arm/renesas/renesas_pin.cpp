/// @file renesas_pin.cpp
/// @brief Runs the production generic software SPI writer through the real
/// Renesas _ARMPIN template and BitbangPin policy on fake GPIO registers.
///
/// The fixture pins shadow the register accessors and fastset/hi/lo, so only the
/// policy, detector and writer are production code; board builds cover the rest.

#include "test.h"

// Exercise the opt-in optimized bit-bang policy; it defaults off.
#ifndef FL_RENESAS_ENABLE_FAST_SOFTWARE_SPI
#define FL_RENESAS_ENABLE_FAST_SOFTWARE_SPI 1
#endif

#include "fl/stl/int.h"

// Stand-ins for the BSP names the pin template uses. Its accessors take a fixed
// u32 port address, which a 64-bit host cannot provide, so the fixture pins below
// shadow them and only the policy, detector and writer are production code here.
typedef int bsp_io_port_pin_t;
struct R_PORT0_Type { volatile fl::u16 PODR, POSR, PORR; };

#include "platforms/arm/renesas/renesas_pin.h"

#include "fastspi_types.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {
namespace renesas_test {

constexpr u32 kData = 1U << 3;
constexpr u32 kClock = 1U << 11;
constexpr u32 kOther = 0x8001U; // Unrelated output bits owned by the sketch.

enum RegKind { kDr, kSet, kClear };

template <int GRP, RegKind KIND> struct Reg {
    static volatile u16& r() {
        static volatile u16 value = 0;
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
u32 gConcurrentLatchBit = 0; // Set on PORT 0 during each rising edge, like an ISR.

template <u8 PIN, u32 MASK, int GRP>
struct Base : _ARMPIN<PIN, 0, 0> {
    static volatile u16* port() { return &Reg<GRP, kDr>::r(); }
    static volatile u16* sport() { return &Reg<GRP, kSet>::r(); }
    static volatile u16* cport() { return &Reg<GRP, kClear>::r(); }
    static u16 mask() { return static_cast<u16>(MASK); }
    static u16 hival() { return Reg<GRP, kDr>::r() | static_cast<u16>(MASK); }
    static u16 loval() { return Reg<GRP, kDr>::r() & static_cast<u16>(~MASK); }
    static void fastset(volatile u16* port, u16 value) { *port = value; }
    static void setOutput() {}
};

constexpr u8 kDataPin = 240;
constexpr u8 kClockPin = 241;
constexpr u8 kOtherPortClockPin = 242;
constexpr u32 kDivider = 4;

} // namespace renesas_test

template <> class FastPin<renesas_test::kDataPin>
    : public renesas_test::Base<renesas_test::kDataPin, renesas_test::kData, 0> {
    using B = renesas_test::Base<renesas_test::kDataPin, renesas_test::kData, 0>;
  public:
    static void fastset(B::port_ptr_t port, B::port_t value) {
        B::fastset(port, value);
        renesas_test::gFullPortStores.push_back(value);
        if (value & renesas_test::kClock) {
            renesas_test::gEdges.push_back({(value & renesas_test::kData) != 0, 0, 0});
        }
    }
};

// Clock pins sample the data aliases on each rising edge, then clear them so
// the next edge shows only the store made for its own bit.
template <u8 PIN, int GRP> struct TracedClock : renesas_test::Base<PIN, renesas_test::kClock, GRP> {
    static void hi() {
        renesas_test::Reg<0, renesas_test::kDr>::r() =
            renesas_test::Reg<0, renesas_test::kDr>::r() | static_cast<u16>(renesas_test::gConcurrentLatchBit);
        volatile u16& set = renesas_test::Reg<0, renesas_test::kSet>::r();
        volatile u16& clear = renesas_test::Reg<0, renesas_test::kClear>::r();
        const u16 s = set;
        const u16 c = clear;
        set = 0;
        clear = 0;
        // Apply the aliases to the latch, as the hardware does.
        volatile u16& latch = renesas_test::Reg<0, renesas_test::kDr>::r();
        latch = static_cast<u16>((latch | s) & ~c);
        // Only a real low-to-high transition counts as an edge.
        if ((renesas_test::Reg<GRP, renesas_test::kDr>::r() & renesas_test::kClock) == 0) {
            renesas_test::gEdges.push_back({s != 0, s, c});
        }
        ++renesas_test::gClockAliasStores;
        renesas_test::Reg<GRP, renesas_test::kDr>::r() =
            renesas_test::Reg<GRP, renesas_test::kDr>::r() | static_cast<u16>(renesas_test::kClock);
    }
    static void lo() {
        ++renesas_test::gClockAliasStores;
        renesas_test::Reg<GRP, renesas_test::kDr>::r() =
            renesas_test::Reg<GRP, renesas_test::kDr>::r() & static_cast<u16>(~renesas_test::kClock);
    }
};

template <> class FastPin<renesas_test::kClockPin> : public TracedClock<renesas_test::kClockPin, 0> {};
template <> class FastPin<renesas_test::kOtherPortClockPin> : public TracedClock<renesas_test::kOtherPortClockPin, 1> {};

// A user-defined pin without sport()/cport() must still compile and use hi()/lo().
template <> class FastPin<243> : public ValidPinBase {
  public:
    static int sHi;
    static int sLo;
    static void hi() { ++sHi; }
    static void lo() { ++sLo; }
};
int FastPin<243>::sHi = 0;
int FastPin<243>::sLo = 0;
FL_STATIC_ASSERT(!BitbangPin<FastPin<243>>::kAtomicWrite && !BitbangPin<FastPin<243>>::kTwoWriteStream,
                 "user-defined pins keep the default policy");

FL_STATIC_ASSERT(FastPin<renesas_test::kDataPin>::validpin(), "Renesas pins must expose validpin()");
FL_STATIC_ASSERT(BitbangPin<FastPin<renesas_test::kDataPin>>::kTwoWriteStream,
                 "Renesas hardware pins must stream on an exclusive same-port bus");
FL_STATIC_ASSERT(BitbangPin<FastPin<renesas_test::kDataPin>>::kAtomicWrite,
                 "Renesas hardware pins must update data through set/clear aliases");
FL_STATIC_ASSERT(!BitbangPin<FastPin<3>>::kTwoWriteStream && !BitbangPin<FastPin<3>>::kAtomicWrite,
                 "pins not backed by the Renesas _ARMPIN keep the default policy");

FL_TEST_FILE(FL_FILEPATH) {
using namespace renesas_test;

using SamePort = GenericSoftwareSPIOutput<kDataPin, kClockPin, kDivider>;
using SplitPort = GenericSoftwareSPIOutput<kDataPin, kOtherPortClockPin, kDivider>;

namespace {

u32 dr0() { return Reg<0, kDr>::r(); }
u32 dr1() { return Reg<1, kDr>::r(); }

void reset() {
    Reg<0, kDr>::r() = static_cast<u16>(kOther);
    Reg<1, kDr>::r() = static_cast<u16>(kOther);
    Reg<0, kSet>::r() = 0;
    Reg<0, kClear>::r() = 0;
    gFullPortStores.clear();
    gEdges.clear();
    gClockAliasStores = 0;
    gConcurrentLatchBit = 0;
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
    FL_REQUIRE_EQ(gEdges.size(), expected.size() * 8); // Rejects stray trailing edges.
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
    for (size_t i = 0; i < gFullPortStores.size(); ++i) {
        FL_CHECK_EQ(gFullPortStores[i] & kOther, kOther);
        // Each bit is one clock-low store with its data, then one clock-high store.
        FL_CHECK_EQ(gFullPortStores[i] & kClock, (i % 2) ? kClock : 0U);
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

FL_TEST_CASE("Renesas same-port writeBytes streams two full-port stores per bit") {
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

FL_TEST_CASE("Renesas same-port writeBytesValueRaw and writePixels stream") {
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

FL_TEST_CASE("Renesas writePixels emits start bits and uses the policy on each port layout") {
    CRGB leds[1] = {CRGB(0x12, 0x34, 0x56)};
    PixelController<RGB> samePixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    reset();
    SamePort same;
    same.writePixels<FLAG_START_BIT, DATA_NOP, RGB>(samePixels);
    FL_REQUIRE_EQ(gEdges.size(), 25U);
    FL_CHECK(gEdges[0].bit);
    FL_CHECK_EQ(gFullPortStores.size(), 50U);
    gEdges.erase(gEdges.begin());
    checkBytes({0x12, 0x34, 0x56});

    PixelController<RGB> splitPixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    reset();
    SplitPort split;
    split.writePixels<0, DATA_NOP, RGB>(splitPixels);
    checkAtomicEdges({0x12, 0x34, 0x56});
}

FL_TEST_CASE("Renesas user-defined pins without alias registers use hi and lo") {
    FastPin<243>::sHi = 0;
    FastPin<243>::sLo = 0;
    BitbangPin<FastPin<243>>::write<0>(1);
    BitbangPin<FastPin<243>>::write<1>(1);
    FL_CHECK_EQ(FastPin<243>::sHi, 1);
    FL_CHECK_EQ(FastPin<243>::sLo, 1);
}

FL_TEST_CASE("Renesas public writeByte uses set/clear aliases and never snapshots the port") {
    reset();
    SamePort::writeByte(0xA5);
    checkAtomicEdges({0xA5});
    FL_CHECK_EQ(dr0() & kClock, 0U);
    FL_CHECK_EQ(dr0(), kOther | kData); // 0xA5 ends on a high data bit.
}

FL_TEST_CASE("Renesas split-port bulk writes keep concurrent unrelated changes") {
    reset();
    SplitPort spi;
    u8 input[] = {0xA5, 0x3C};
    spi.writeBytes<DATA_NOP>(input, 2);
    checkAtomicEdges({0xA5, 0x3C});

    // A latch bit changed while the bus runs survives: nothing rewrites OUT.
    reset();
    gConcurrentLatchBit = 1U << 9;
    SplitPort::writeBytesValueRaw(0x5A, 1);
    FL_CHECK_EQ(dr0(), kOther | (1U << 9));
    FL_CHECK_EQ(dr1() & kClock, 0U);
}

} // FL_TEST_FILE

} // namespace fl
