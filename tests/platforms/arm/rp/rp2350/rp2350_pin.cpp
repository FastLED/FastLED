/// @file rp2350_pin.cpp
/// @brief Runs the production generic software SPI writer through the real
/// RP2350 _RP2350PIN template and BitbangPin policy on a fake SIO.
///
/// _RP2350PIN writes through raw volatile pointers, which a host cannot intercept,
/// so these fixture pins shadow only fastset/hi/lo to record each store.

#include "test.h"

// Exercise the opt-in optimized bit-bang policy; it defaults off.
#ifndef FL_RP_ENABLE_FAST_SOFTWARE_SPI
#define FL_RP_ENABLE_FAST_SOFTWARE_SPI 1
#endif

#include "fl/stl/int.h"

typedef unsigned int uint;
struct FakeSio {
    volatile fl::u32 gpio_out, gpio_set, gpio_clr, gpio_togl, gpio_oe_set, gpio_oe_clr;
    volatile fl::u32 gpio_hi_out, gpio_hi_set, gpio_hi_clr, gpio_hi_togl, gpio_hi_oe_set, gpio_hi_oe_clr;
};
static FakeSio gFakeSio;
#define sio_hw (&gFakeSio)
#define GPIO_FUNC_SIO 5
inline void gpio_set_function(uint, int) {}

#include "platforms/arm/rp/rp2350/rp2350_pin.h"

#include "fastspi_types.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {
namespace rp2350_test {

constexpr u32 kData = 1U << 3;
constexpr u32 kClock = 1U << 17;
constexpr u32 kOther = 0x80000001U; // Unrelated output bits owned by the sketch.

enum RegKind { kDr, kSet, kClear };

// One SIO bank: the split-port fixture pretends its clock has its own register.
template <int GRP, RegKind KIND> struct Reg {
    static volatile u32& r() {
        static volatile u32 other = 0;
        if (GRP != 0) return other;
        return KIND == kDr ? gFakeSio.gpio_out : KIND == kSet ? gFakeSio.gpio_set : gFakeSio.gpio_clr;
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
using Base = _RP2350PIN<PIN, MASK>;

constexpr u8 kDataPin = 20;
constexpr u8 kClockPin = 21;
constexpr u8 kOtherPortClockPin = 22;
constexpr u32 kDivider = 4;

} // namespace rp2350_test

template <> class FastPin<rp2350_test::kDataPin>
    : public rp2350_test::Base<rp2350_test::kDataPin, rp2350_test::kData, 0> {
    using B = rp2350_test::Base<rp2350_test::kDataPin, rp2350_test::kData, 0>;
  public:
    static void fastset(B::port_ptr_t port, B::port_t value) {
        B::fastset(port, value);
        rp2350_test::gFullPortStores.push_back(value);
        if (value & rp2350_test::kClock) {
            rp2350_test::gEdges.push_back({(value & rp2350_test::kData) != 0, 0, 0});
        }
    }
};

// Clock pins sample the data aliases on each rising edge, then clear them so
// the next edge shows only the store made for its own bit.
template <u8 PIN, int GRP> struct TracedClock : rp2350_test::Base<PIN, rp2350_test::kClock, GRP> {
    static volatile u32* port() { return &rp2350_test::Reg<GRP, rp2350_test::kDr>::r(); }
    static void hi() {
        rp2350_test::Reg<0, rp2350_test::kDr>::r() =
            rp2350_test::Reg<0, rp2350_test::kDr>::r() | rp2350_test::gConcurrentLatchBit;
        volatile u32& set = rp2350_test::Reg<0, rp2350_test::kSet>::r();
        volatile u32& clear = rp2350_test::Reg<0, rp2350_test::kClear>::r();
        const u32 s = set;
        const u32 c = clear;
        set = 0;
        clear = 0;
        // Apply the aliases to the latch, as the hardware does.
        volatile u32& latch = rp2350_test::Reg<0, rp2350_test::kDr>::r();
        latch = (latch | s) & ~c;
        // Only a real low-to-high transition counts as an edge.
        if ((rp2350_test::Reg<GRP, rp2350_test::kDr>::r() & rp2350_test::kClock) == 0) {
            rp2350_test::gEdges.push_back({s != 0, s, c});
        }
        ++rp2350_test::gClockAliasStores;
        rp2350_test::Reg<GRP, rp2350_test::kDr>::r() =
            rp2350_test::Reg<GRP, rp2350_test::kDr>::r() | rp2350_test::kClock;
    }
    static void lo() {
        ++rp2350_test::gClockAliasStores;
        rp2350_test::Reg<GRP, rp2350_test::kDr>::r() =
            rp2350_test::Reg<GRP, rp2350_test::kDr>::r() & ~rp2350_test::kClock;
    }
};

template <> class FastPin<rp2350_test::kClockPin> : public TracedClock<rp2350_test::kClockPin, 0> {};
template <> class FastPin<rp2350_test::kOtherPortClockPin> : public TracedClock<rp2350_test::kOtherPortClockPin, 1> {};

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

FL_STATIC_ASSERT(BitbangPin<FastPin<rp2350_test::kDataPin>>::kTwoWriteStream,
                 "RP2350 hardware pins must stream on an exclusive same-port bus");
FL_STATIC_ASSERT(BitbangPin<FastPin<rp2350_test::kDataPin>>::kAtomicWrite,
                 "RP2350 hardware pins must update data through set/clear aliases");
// GPIO32-47 are in the GPIO_HI bank, which _RP2350PIN cannot address.
template <> class FastPin<31> : public _RP2350PIN<31, static_cast<u32>(1ULL << 31)> {};
template <> class FastPin<32> : public _RP2350PIN<32, static_cast<u32>(1ULL << 32)> {};
FL_STATIC_ASSERT(BitbangPin<FastPin<31>>::kAtomicWrite, "GPIO31 is the last low-bank pin");
FL_STATIC_ASSERT(!BitbangPin<FastPin<32>>::kAtomicWrite && !BitbangPin<FastPin<32>>::kTwoWriteStream,
                 "high-bank pins must stay out of the optimization");
FL_STATIC_ASSERT(!BitbangPin<FastPin<3>>::kTwoWriteStream && !BitbangPin<FastPin<3>>::kAtomicWrite,
                 "pins not backed by the RP2350 _RP2350PIN keep the default policy");

FL_TEST_FILE(FL_FILEPATH) {
using namespace rp2350_test;

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

FL_TEST_CASE("RP2350 same-port writeBytes streams two full-port stores per bit") {
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

FL_TEST_CASE("RP2350 same-port writeBytesValueRaw and writePixels stream") {
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

FL_TEST_CASE("RP2350 writePixels emits start bits and uses the policy on each port layout") {
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

    // Production RP2350 always takes this atomic path; the split-port fixture is
    // how a host reaches it, because the test PCH fixes the interrupt macro.
    PixelController<RGB> startPixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    reset();
    split.writePixels<FLAG_START_BIT, DATA_NOP, RGB>(startPixels);
    FL_REQUIRE_EQ(gEdges.size(), 25U);
    FL_CHECK(gEdges[0].bit);
    gEdges.erase(gEdges.begin());
    checkAtomicEdges({0x12, 0x34, 0x56});
}

FL_TEST_CASE("RP2350 user-defined pins without alias registers use hi and lo") {
    FastPin<243>::sHi = 0;
    FastPin<243>::sLo = 0;
    BitbangPin<FastPin<243>>::write<0>(1);
    BitbangPin<FastPin<243>>::write<1>(1);
    FL_CHECK_EQ(FastPin<243>::sHi, 1);
    FL_CHECK_EQ(FastPin<243>::sLo, 1);
}

FL_TEST_CASE("RP2350 public writeByte uses set/clear aliases and never snapshots the port") {
    reset();
    SamePort::writeByte(0xA5);
    checkAtomicEdges({0xA5});
    FL_CHECK_EQ(dr0() & kClock, 0U);
    FL_CHECK_EQ(dr0(), kOther | kData); // 0xA5 ends on a high data bit.
}

FL_TEST_CASE("RP2350 split-port bulk writes keep concurrent unrelated changes") {
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

// GPIO32-47 live in the GPIO_HI bank, bit (PIN - 32) (#5100).
FL_TEST_CASE("RP2350 GPIO32-47 drive the GPIO_HI registers") {
    using Low = _RP2350PIN<5, 1U << 5>;
    using High = _RP2350PIN<33, 1U << 1>;
    gFakeSio = FakeSio();

    High::setOutput();
    High::hi();
    FL_CHECK_EQ(gFakeSio.gpio_hi_oe_set, 1U << 1);
    FL_CHECK_EQ(gFakeSio.gpio_hi_set, 1U << 1);
    High::lo();
    FL_CHECK_EQ(gFakeSio.gpio_hi_clr, 1U << 1);
    High::toggle();
    FL_CHECK_EQ(gFakeSio.gpio_hi_togl, 1U << 1);
    FL_CHECK(High::port() == &gFakeSio.gpio_hi_out);
    FL_CHECK(High::sport() == &gFakeSio.gpio_hi_set);
    FL_CHECK(High::cport() == &gFakeSio.gpio_hi_clr);
    // Nothing reached the low bank, where bit 1 is GPIO1.
    FL_CHECK_EQ(gFakeSio.gpio_set, 0U);
    FL_CHECK_EQ(gFakeSio.gpio_clr, 0U);
    FL_CHECK_EQ(gFakeSio.gpio_oe_set, 0U);

    Low::setOutput();
    Low::hi();
    FL_CHECK_EQ(gFakeSio.gpio_oe_set, 1U << 5);
    FL_CHECK_EQ(gFakeSio.gpio_set, 1U << 5);
    FL_CHECK(Low::port() == &gFakeSio.gpio_out);
}

} // FL_TEST_FILE

} // namespace fl
