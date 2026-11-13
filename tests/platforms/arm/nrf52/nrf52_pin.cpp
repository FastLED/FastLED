/// @file nrf52_pin.cpp
/// @brief Runs the production generic software SPI writer through the real nRF52
/// _ARMPIN template and BitbangPin policy on fake P0 and P1 GPIO blocks.
///
/// Fixture pins shadow only fastset/hi/lo to record each store. port(), sport(),
/// cport(), mask(), hival() and loval() are the production accessors.

#include "test.h"

// Exercise the opt-in optimized bit-bang policy; it defaults off.
#ifndef FL_NRF52_ENABLE_FAST_SOFTWARE_SPI
#define FL_NRF52_ENABLE_FAST_SOFTWARE_SPI 1
#endif

#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"

// Stand-ins for the Nordic declarations, shaped like the vendor headers.
struct NRF_GPIO_Type { volatile fl::u32 OUT, OUTSET, OUTCLR; };
static NRF_GPIO_Type gFakeGpio[2];
enum { NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL,
       NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE };
inline void nrf_gpio_cfg(fl::u32, int, int, int, int, int) {}
#define NRF_GPIO_PIN_MAP(port, pin) (((port) << 5) | ((pin) & 0x1F))
#ifndef FASTLED_NRF52_INLINE_ATTRIBUTE
#define FASTLED_NRF52_INLINE_ATTRIBUTE FASTLED_FORCE_INLINE
#endif

#include "platforms/arm/nrf52/nrf52_pin.h"

#include "fastspi_types.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {
namespace nrf52_test {

constexpr u32 kData = 1U << 3;
constexpr u32 kClock = 1U << 17;
constexpr u32 kOther = 0x80000001U; // Unrelated output bits owned by the sketch.

enum RegKind { kDr, kSet, kClear };

template <int GRP, RegKind KIND> struct Reg {
    static volatile u32& r() {
        return KIND == kDr ? gFakeGpio[GRP].OUT : KIND == kSet ? gFakeGpio[GRP].OUTSET : gFakeGpio[GRP].OUTCLR;
    }
};

template <int GRP> struct TestPort {
    static uptr r() { return fl::bit_cast<uptr>(&gFakeGpio[GRP]); }
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

template <u32 MASK, int GRP>
using Base = _ARMPIN<MASK, TestPort<GRP>, GRP, 0>;

constexpr u8 kDataPin = 240;
constexpr u8 kClockPin = 241;
constexpr u8 kOtherPortClockPin = 242;
constexpr u32 kDivider = 4;

} // namespace nrf52_test

template <> class FastPin<nrf52_test::kDataPin>
    : public nrf52_test::Base<nrf52_test::kData, 0> {
    using B = nrf52_test::Base<nrf52_test::kData, 0>;
  public:
    static void fastset(B::port_ptr_t port, B::port_t value) {
        B::fastset(port, value);
        nrf52_test::gFullPortStores.push_back(value);
        if (value & nrf52_test::kClock) {
            nrf52_test::gEdges.push_back({(value & nrf52_test::kData) != 0, 0, 0});
        }
    }
};

// Clock pins sample the data aliases on each rising edge, then clear them so
// the next edge shows only the store made for its own bit.
template <u8 PIN, int GRP> struct TracedClock : nrf52_test::Base<nrf52_test::kClock, GRP> {
    static void hi() {
        nrf52_test::Reg<0, nrf52_test::kDr>::r() =
            nrf52_test::Reg<0, nrf52_test::kDr>::r() | nrf52_test::gConcurrentLatchBit;
        volatile u32& set = nrf52_test::Reg<0, nrf52_test::kSet>::r();
        volatile u32& clear = nrf52_test::Reg<0, nrf52_test::kClear>::r();
        const u32 s = set;
        const u32 c = clear;
        set = 0;
        clear = 0;
        // Apply the aliases to the latch, as the hardware does.
        volatile u32& latch = nrf52_test::Reg<0, nrf52_test::kDr>::r();
        latch = (latch | s) & ~c;
        // Only a real low-to-high transition counts as an edge.
        if ((nrf52_test::Reg<GRP, nrf52_test::kDr>::r() & nrf52_test::kClock) == 0) {
            nrf52_test::gEdges.push_back({s != 0, s, c});
        }
        ++nrf52_test::gClockAliasStores;
        nrf52_test::Reg<GRP, nrf52_test::kDr>::r() =
            nrf52_test::Reg<GRP, nrf52_test::kDr>::r() | nrf52_test::kClock;
    }
    static void lo() {
        ++nrf52_test::gClockAliasStores;
        nrf52_test::Reg<GRP, nrf52_test::kDr>::r() =
            nrf52_test::Reg<GRP, nrf52_test::kDr>::r() & ~nrf52_test::kClock;
    }
};

template <> class FastPin<nrf52_test::kClockPin> : public TracedClock<nrf52_test::kClockPin, 0> {};
template <> class FastPin<nrf52_test::kOtherPortClockPin> : public TracedClock<nrf52_test::kOtherPortClockPin, 1> {};

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

FL_STATIC_ASSERT(FastPin<nrf52_test::kDataPin>::validpin(), "nRF52 pins must expose validpin()");

FL_STATIC_ASSERT(!BitbangPin<FastPin<243>>::kAtomicWrite && !BitbangPin<FastPin<243>>::kTwoWriteStream,
                 "user-defined pins keep the default policy");
FL_STATIC_ASSERT(!BitbangPin<FastPin<nrf52_test::kDataPin>>::kTwoWriteStream,
                 "whole-OUT streaming stays off: BLE and RTOS tasks share the port");
FL_STATIC_ASSERT(BitbangPin<FastPin<nrf52_test::kDataPin>>::kAtomicWrite,
                 "nRF52 hardware pins must update data through OUTSET and OUTCLR");

FL_TEST_FILE(FL_FILEPATH) {
using namespace nrf52_test;

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

FL_TEST_CASE("nRF52 same-port writeBytes uses OUTSET and OUTCLR") {
    const fl::vector<fl::vector<u8>> inputs = {
        {}, {0x00}, {0xFF}, {0xAA}, {0x55}, {0x00, 0xFF, 0xAA, 0x55, 0x81, 0x7E},
    };
    for (const fl::vector<u8>& input : inputs) {
        reset();
        SamePort spi;
        fl::vector<u8> bytes = input;
        spi.writeBytes<DATA_NOP>(bytes.data(), static_cast<int>(bytes.size()));
        checkAtomicEdges(input);
        FL_CHECK_EQ(dr0() & kClock, 0U);
    }
}

FL_TEST_CASE("nRF52 same-port writeBytesValueRaw and writePixels use OUTSET and OUTCLR") {
    reset();
    SamePort::writeBytesValueRaw(0xA5, 3);
    checkAtomicEdges({0xA5, 0xA5, 0xA5});

    CRGB leds[1] = {CRGB(0x12, 0x34, 0x56)};
    reset();
    SamePort spi;
    PixelController<RGB> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.writePixels<0, DATA_NOP, RGB>(pixels);
    checkAtomicEdges({0x12, 0x34, 0x56});
}

FL_TEST_CASE("nRF52 writePixels emits start bits and uses the policy on each port layout") {
    CRGB leds[1] = {CRGB(0x12, 0x34, 0x56)};
    PixelController<RGB> samePixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    reset();
    SamePort same;
    same.writePixels<FLAG_START_BIT, DATA_NOP, RGB>(samePixels);
    FL_REQUIRE_EQ(gEdges.size(), 25U);
    FL_CHECK(gEdges[0].bit);
    FL_CHECK(gFullPortStores.empty());
    gEdges.erase(gEdges.begin());
    checkBytes({0x12, 0x34, 0x56});

    PixelController<RGB> splitPixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    reset();
    SplitPort split;
    split.writePixels<0, DATA_NOP, RGB>(splitPixels);
    checkAtomicEdges({0x12, 0x34, 0x56});
}

FL_TEST_CASE("nRF52 user-defined pins without alias registers use hi and lo") {
    FastPin<243>::sHi = 0;
    FastPin<243>::sLo = 0;
    BitbangPin<FastPin<243>>::write<0>(1);
    BitbangPin<FastPin<243>>::write<1>(1);
    FL_CHECK_EQ(FastPin<243>::sHi, 1);
    FL_CHECK_EQ(FastPin<243>::sLo, 1);
}

FL_TEST_CASE("nRF52 accessors select the pin's own port on each register") {
    // The mask is the local port bit, while nrf_pin() encodes port and bit.
    using P1Bit5 = _ARMPIN<(1U << 5), TestPort<1>, 1, 5>;
    FL_CHECK(P1Bit5::port() == &gFakeGpio[1].OUT);
    FL_CHECK(P1Bit5::sport() == &gFakeGpio[1].OUTSET);
    FL_CHECK(P1Bit5::cport() == &gFakeGpio[1].OUTCLR);
    FL_CHECK_EQ(P1Bit5::mask(), 1U << 5);
    FL_CHECK_EQ(P1Bit5::nrf_pin(), 37U);
}

FL_TEST_CASE("nRF52 public writeByte uses set/clear aliases and never snapshots the port") {
    reset();
    SamePort::writeByte(0xA5);
    checkAtomicEdges({0xA5});
    FL_CHECK_EQ(dr0() & kClock, 0U);
    FL_CHECK_EQ(dr0(), kOther | kData); // 0xA5 ends on a high data bit.
}

FL_TEST_CASE("nRF52 split-port bulk writes keep concurrent unrelated changes") {
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
