/// @file same53_pin.cpp
/// @brief Runs the production generic software SPI writer through the real
/// ClearCore pin template and BitbangPin policy, and checks the result at the
/// connector, where a cleared GPIO bit is a logical HIGH.
///
/// The fixture clock pin shadows hi/lo to apply and sample the set/clear registers,
/// and the data pin shadows fastset to model full-port stores. The accessors and
/// the writer are production code.

#include "test.h"

#include "fl/stl/int.h"

// Stand-in for the vendor PORT declaration, shaped like <sam.h>.
struct FakeReg { volatile fl::u32 reg; };
struct FakeGroup { FakeReg OUT, OUTSET, OUTCLR, OUTTGL; };
struct FakePort { FakeGroup Group[1]; };
static FakePort gFakePort;
#undef PORT
#define PORT (&gFakePort)

#include "platforms/arm/same53/same53_pin.h"

#include "fastspi_types.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {
namespace same53_test {

constexpr u8 kDataPin = 240;
constexpr u8 kClockPin = 241;
constexpr u8 kDataBit = 6;
constexpr u8 kClockBit = 7;
constexpr u32 kData = 1U << kDataBit;
constexpr u32 kClock = 1U << kClockBit;
constexpr u32 kOther = 0x80000001U; // Unrelated output bits owned by the sketch.
constexpr u32 kDivider = 4;

struct Edge {
    bool dataHigh; // Logical level at the connector.
    u32 outClr;    // OUTCLR store made for this bit, or 0.
    u32 outSet;    // OUTSET store made for this bit, or 0.
};

fl::vector<Edge> gEdges;
int gFullPortStores = 0;
int gClockHiCalls = 0;
int gClockLoCalls = 0;
u32 gConcurrentBit = 0; // Set during each rising edge, like an ISR.

inline bool clockHigh() { return (gFakePort.Group[0].OUT.reg & kClock) == 0; }
inline bool dataHigh() { return (gFakePort.Group[0].OUT.reg & kData) == 0; }

// One store of a whole-port word. A falling raw clock bit is a logical rising edge.
inline void storeOut(u32 value) {
    const bool wasHigh = clockHigh();
    gFakePort.Group[0].OUT.reg = value;
    if (!wasHigh && clockHigh()) {
        gEdges.push_back({dataHigh(), 0, 0});
    }
}

// Applies pending data register stores to OUT and returns them for the edge record.
inline void applyAliases(u32* outClr, u32* outSet) {
    *outClr = gFakePort.Group[0].OUTCLR.reg;
    *outSet = gFakePort.Group[0].OUTSET.reg;
    gFakePort.Group[0].OUTCLR.reg = 0;
    gFakePort.Group[0].OUTSET.reg = 0;
    u32 out = gFakePort.Group[0].OUT.reg;
    out = (out | *outSet) & ~*outClr;
    gFakePort.Group[0].OUT.reg = out;
}

} // namespace same53_test

template <> class FastPin<same53_test::kDataPin>
    : public ClearCoreFastPin<same53_test::kDataPin, same53_test::kDataBit> {
    using B = ClearCoreFastPin<same53_test::kDataPin, same53_test::kDataBit>;
  public:
    static void fastset(B::port_ptr_t, B::port_t value) {
        ++same53_test::gFullPortStores;
        same53_test::storeOut(value);
    }
};

template <> class FastPin<same53_test::kClockPin>
    : public ClearCoreFastPin<same53_test::kClockPin, same53_test::kClockBit> {
    using B = ClearCoreFastPin<same53_test::kClockPin, same53_test::kClockBit>;
  public:
    static void fastset(B::port_ptr_t, B::port_t value) {
        ++same53_test::gFullPortStores;
        same53_test::storeOut(value);
    }
    static void hi() {
        using namespace same53_test;
        ++gClockHiCalls;
        u32 outClr = 0;
        u32 outSet = 0;
        applyAliases(&outClr, &outSet);
        gFakePort.Group[0].OUT.reg = gFakePort.Group[0].OUT.reg | gConcurrentBit;
        if (!clockHigh()) {
            gEdges.push_back({dataHigh(), outClr, outSet});
        }
        gFakePort.Group[0].OUT.reg = gFakePort.Group[0].OUT.reg & ~kClock;
    }
    static void lo() {
        using namespace same53_test;
        ++gClockLoCalls;
        gFakePort.Group[0].OUT.reg = gFakePort.Group[0].OUT.reg | kClock;
    }
    static void hi(B::port_ptr_t) { hi(); }
    static void lo(B::port_ptr_t) { lo(); }
};

// A user-defined pin without set/clear registers must still compile and use hi()/lo().
template <> class FastPin<243> : public ValidPinBase {
  public:
    static int sHi;
    static int sLo;
    static void hi() { ++sHi; }
    static void lo() { ++sLo; }
};
int FastPin<243>::sHi = 0;
int FastPin<243>::sLo = 0;

// A user-defined, active-high data pin on the same port as the ClearCore clock
// pin. Its policy is the non-atomic default, so before #5112 the writer built
// whole-port words with the clock bit set for "high", which is backwards here.
template <> class FastPin<245> : public ValidPinBase {
  public:
    typedef volatile u32* port_ptr_t;
    typedef u32 port_t;
    static constexpr u32 kMask = same53_test::kData;
    static void setOutput() {}
    static void hi() { gFakePort.Group[0].OUT.reg = gFakePort.Group[0].OUT.reg | kMask; }
    static void lo() { gFakePort.Group[0].OUT.reg = gFakePort.Group[0].OUT.reg & ~kMask; }
    static void hi(port_ptr_t) { hi(); }
    static void lo(port_ptr_t) { lo(); }
    static void fastset(port_ptr_t, port_t value) {
        ++same53_test::gFullPortStores;
        same53_test::storeOut(value);
    }
    static port_t hival() { return gFakePort.Group[0].OUT.reg | kMask; }
    static port_t loval() { return gFakePort.Group[0].OUT.reg & ~kMask; }
    static port_ptr_t port() { return &gFakePort.Group[0].OUT.reg; }
    static port_t mask() { return kMask; }
};

FL_STATIC_ASSERT(!BitbangPin<FastPin<243>>::kAtomicWrite, "user-defined pins keep the default policy");
FL_STATIC_ASSERT(BitbangPin<FastPin<same53_test::kDataPin>>::kAtomicWrite && BitbangPin<FastPin<3>>::kAtomicWrite,
                 "ClearCore pins must use polarity-correct atomic writes");
FL_STATIC_ASSERT(!BitbangPin<FastPin<same53_test::kDataPin>>::kTwoWriteStream && !BitbangPin<FastPin<3>>::kTwoWriteStream,
                 "whole-port streaming assumes an active-high clock, so it stays off");

FL_TEST_FILE(FL_FILEPATH) {
using namespace same53_test;

using Spi = GenericSoftwareSPIOutput<kDataPin, kClockPin, kDivider>;

namespace {

void reset() {
    gFakePort.Group[0].OUT.reg = kOther | kClock; // Raw clock set: logically low.
    gFakePort.Group[0].OUTSET.reg = 0;
    gFakePort.Group[0].OUTCLR.reg = 0;
    gFakePort.Group[0].OUTTGL.reg = 0;
    gEdges.clear();
    gFullPortStores = 0;
    gClockHiCalls = 0;
    gClockLoCalls = 0;
    gConcurrentBit = 0;
}

// The connector clock rises once per bit with the right data and ends low.
void checkConnector(const fl::vector<u8>& expected) {
    FL_REQUIRE_EQ(gEdges.size(), expected.size() * 8);
    for (size_t i = 0; i < gEdges.size(); ++i) {
        const bool bit = (expected[i / 8] >> (7 - (i % 8))) & 1;
        FL_CHECK_EQ(gEdges[i].dataHigh, bit);
    }
    FL_CHECK_FALSE(clockHigh());
    FL_CHECK_EQ(gFakePort.Group[0].OUT.reg & kOther, kOther);
}

// One data store on the polarity-correct register per bit, and no OUT snapshots.
void checkStores() {
    for (const Edge& edge : gEdges) {
        FL_CHECK_EQ(edge.outClr, edge.dataHigh ? kData : 0U);
        FL_CHECK_EQ(edge.outSet, edge.dataHigh ? 0U : kData);
    }
    FL_CHECK_EQ(gFullPortStores, 0);
    FL_CHECK_EQ(gClockHiCalls, static_cast<int>(gEdges.size()));
    FL_CHECK_EQ(gClockLoCalls, static_cast<int>(gEdges.size()));
}

} // namespace

FL_TEST_CASE("ClearCore accessors keep the active-low register mapping") {
    using P = FastPin<2>;
    FL_CHECK(P::port() == &gFakePort.Group[0].OUT.reg);
    FL_CHECK(P::sport() == &gFakePort.Group[0].OUTCLR.reg);
    FL_CHECK(P::cport() == &gFakePort.Group[0].OUTSET.reg);
    FL_CHECK_EQ(P::mask(), 1U << 6);
    FL_CHECK_EQ(FastPin<3>::mask(), 1U << 7);

    reset();
    P::hi();
    FL_CHECK_EQ(gFakePort.Group[0].OUTCLR.reg, 1U << 6);
    FL_CHECK_EQ(gFakePort.Group[0].OUTSET.reg, 0U);
    P::lo();
    FL_CHECK_EQ(gFakePort.Group[0].OUTSET.reg, 1U << 6);

    // Logical high clears the GPIO bit, and only that bit.
    gFakePort.Group[0].OUT.reg = kOther | (1U << 6);
    FL_CHECK_EQ(P::hival(), kOther);
    gFakePort.Group[0].OUT.reg = kOther;
    FL_CHECK_EQ(P::loval(), kOther | (1U << 6));
    P::toggle();
    FL_CHECK_EQ(gFakePort.Group[0].OUTTGL.reg, 1U << 6);
}

FL_TEST_CASE("ClearCore writeBytes is correct at the connector") {
    fl::vector<fl::vector<u8>> inputs = {
        {}, {0x00}, {0xFF}, {0xAA}, {0x55}, {0x00, 0xFF, 0xAA, 0x55, 0x81, 0x7E},
    };
    fl::vector<u8> oneHot;
    for (int i = 0; i < 8; ++i) {
        oneHot.push_back(static_cast<u8>(1U << i));
    }
    inputs.push_back(oneHot);
    for (const fl::vector<u8>& input : inputs) {
        reset();
        Spi spi;
        fl::vector<u8> bytes = input;
        spi.writeBytes<DATA_NOP>(bytes.data(), static_cast<int>(bytes.size()));
        checkConnector(input);
        checkStores();
    }
}

FL_TEST_CASE("ClearCore writeBytesValueRaw is correct at the connector") {
    reset();
    Spi::writeBytesValueRaw(0xA5, 3);
    checkConnector({0xA5, 0xA5, 0xA5});
    checkStores();
}

FL_TEST_CASE("ClearCore writePixels keeps payload and start bit at the connector") {
    CRGB leds[2] = {CRGB(0x55, 0xAA, 0x12), CRGB(0xAA, 0x55, 0x34)};
    reset();
    Spi spi;
    PixelController<RGB> pixels(leds, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.writePixels<0, DATA_NOP, RGB>(pixels);
    checkConnector({0x55, 0xAA, 0x12, 0xAA, 0x55, 0x34});
    checkStores();

    reset();
    PixelController<RGB> startPixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.writePixels<FLAG_START_BIT, DATA_NOP, RGB>(startPixels);
    FL_REQUIRE_EQ(gEdges.size(), 25U);
    FL_CHECK(gEdges[0].dataHigh);
    FL_CHECK_EQ(gEdges[0].outClr, kData);
    const u32 rgb = 0x55AA12;
    for (int i = 0; i < 24; ++i) {
        FL_CHECK_EQ(gEdges[i + 1].dataHigh, ((rgb >> (23 - i)) & 1U) != 0);
    }
    FL_CHECK_FALSE(clockHigh());
    checkStores();
}

FL_TEST_CASE("ClearCore public writeByte is correct at the connector") {
    reset();
    Spi::writeByte(0xA5);
    checkConnector({0xA5});
    checkStores();
}

FL_TEST_CASE("ClearCore bulk writes keep concurrent unrelated changes") {
    reset();
    gConcurrentBit = 1U << 9;
    Spi::writeBytesValueRaw(0x5A, 1);
    FL_CHECK_EQ(gFakePort.Group[0].OUT.reg & gConcurrentBit, gConcurrentBit);
    FL_CHECK_EQ(gFakePort.Group[0].OUT.reg & kOther, kOther);
}

FL_TEST_CASE("ClearCore clock with a user-defined data pin on its port uses per-pin writes") {
    using MixedSpi = GenericSoftwareSPIOutput<245, kClockPin, kDivider>;
    reset();
    MixedSpi::writeBytesValueRaw(0xA5, 1);
    // The ClearCore clock must go through its own hi()/lo(): no whole-port words.
    FL_CHECK_EQ(gFullPortStores, 0);
    FL_CHECK_EQ(gClockHiCalls, 8);
    FL_CHECK_EQ(gClockLoCalls, 8);
    FL_CHECK_EQ(gEdges.size(), 8U);
    FL_CHECK_FALSE(clockHigh());
}

FL_TEST_CASE("ClearCore user-defined pins use hi and lo") {
    FastPin<243>::sHi = 0;
    FastPin<243>::sLo = 0;
    BitbangPin<FastPin<243>>::write<0>(1);
    BitbangPin<FastPin<243>>::write<1>(1);
    FL_CHECK_EQ(FastPin<243>::sHi, 1);
    FL_CHECK_EQ(FastPin<243>::sLo, 1);
}

} // FL_TEST_FILE

} // namespace fl
