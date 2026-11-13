/// @file sam_pin.cpp
/// @brief Runs the production generic software SPI writer through the real Due
/// _DUEPIN template and BitbangPin policy on fake PIO registers.
///
/// A fixture clock pin shadows hi/lo to apply and sample SODR/CODR, and the data
/// pin adds a fastset counter. The accessors and the writer are production code.

#include "test.h"

// Exercise the opt-in optimized bit-bang policy; it defaults off.
#ifndef FL_SAM_ENABLE_FAST_SOFTWARE_SPI
#define FL_SAM_ENABLE_FAST_SOFTWARE_SPI 1
#endif

#include "fl/stl/int.h"
#include "platforms/arm/sam/sam_pin.h"

#include "fastspi_types.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {
namespace sam_test {

constexpr u32 kData = 1U << 8;
constexpr u32 kClock = 1U << 9;
constexpr u32 kOther = 0x80000001U; // Unrelated output bits owned by the sketch.
constexpr u32 kDivider = 4;
constexpr int kPortA = 0;
constexpr int kPortB = 1;
constexpr int kOdsr = 0;
constexpr int kSodr = 1;
constexpr int kCodr = 2;
constexpr int kOer = 3;

volatile u32 gRegs[2][4];

template <int PORT, int REG> struct Reg {
    static volatile u32& r() { return gRegs[PORT][REG]; }
};

template <u8 PIN, u32 MASK, int PORT>
using Base = _DUEPIN<PIN, MASK, Reg<PORT, kOdsr>, Reg<PORT, kSodr>, Reg<PORT, kCodr>, Reg<PORT, kOer>>;

struct Edge {
    bool dataHigh;
    u32 sodr; // SODR store made for this bit, or 0.
    u32 codr; // CODR store made for this bit, or 0.
};

fl::vector<Edge> gEdges;
int gFullPortStores = 0;
int gClockLows = 0;
u32 gConcurrentBit = 0; // Set during each rising edge, like an ISR.

inline void applyData(u32* sodr, u32* codr) {
    *sodr = gRegs[kPortA][kSodr];
    *codr = gRegs[kPortA][kCodr];
    gRegs[kPortA][kSodr] = 0;
    gRegs[kPortA][kCodr] = 0;
    const u32 odsr = gRegs[kPortA][kOdsr];
    gRegs[kPortA][kOdsr] = ((odsr | *sodr) & ~*codr) | gConcurrentBit;
    gConcurrentBit = 0; // Injected once, so a later stale store would lose it.
}

template <u8 PIN, int PORT> struct TracedClock : Base<PIN, kClock, PORT> {
    static void hi() {
        u32 sodr = 0;
        u32 codr = 0;
        applyData(&sodr, &codr);
        gEdges.push_back({(gRegs[kPortA][kOdsr] & kData) != 0, sodr, codr});
        gRegs[PORT][kOdsr] = gRegs[PORT][kOdsr] | kClock;
    }
    static void lo() {
        ++gClockLows;
        gRegs[PORT][kOdsr] = gRegs[PORT][kOdsr] & ~kClock;
    }
};

} // namespace sam_test

template <> class FastPin<240> : public sam_test::Base<240, sam_test::kData, sam_test::kPortA> {
    using B = sam_test::Base<240, sam_test::kData, sam_test::kPortA>;
  public:
    static void fastset(B::port_ptr_t port, B::port_t value) {
        B::fastset(port, value);
        ++sam_test::gFullPortStores;
    }
};
template <> class FastPin<241> : public sam_test::TracedClock<241, sam_test::kPortA> {};
template <> class FastPin<242> : public sam_test::TracedClock<242, sam_test::kPortB> {};

// A user-defined pin without SODR/CODR must still compile and use hi()/lo().
template <> class FastPin<243> : public ValidPinBase {
  public:
    static int sHi;
    static int sLo;
    static void hi() { ++sHi; }
    static void lo() { ++sLo; }
};
int FastPin<243>::sHi = 0;
int FastPin<243>::sLo = 0;

FL_STATIC_ASSERT(!BitbangPin<FastPin<243>>::kAtomicWrite, "user-defined pins keep the default policy");
FL_STATIC_ASSERT(BitbangPin<FastPin<240>>::kAtomicWrite, "Due pins use SODR and CODR data writes");
FL_STATIC_ASSERT(!BitbangPin<FastPin<240>>::kTwoWriteStream, "ODSR streaming stays off until OWSR state is proven");

FL_TEST_FILE(FL_FILEPATH) {
using namespace sam_test;

namespace {

void reset() {
    for (auto& port : gRegs) {
        for (auto& reg : port) {
            reg = 0;
        }
    }
    gRegs[kPortA][kOdsr] = kOther;
    gRegs[kPortB][kOdsr] = kOther;
    gEdges.clear();
    gFullPortStores = 0;
    gClockLows = 0;
    gConcurrentBit = 0;
}

// One rising edge per bit, one mask store on the right register, no ODSR writes.
void check(const fl::vector<u8>& expected) {
    FL_REQUIRE_EQ(gEdges.size(), expected.size() * 8);
    for (size_t i = 0; i < gEdges.size(); ++i) {
        const bool bit = (expected[i / 8] >> (7 - (i % 8))) & 1;
        FL_CHECK_EQ(gEdges[i].dataHigh, bit);
        FL_CHECK_EQ(gEdges[i].sodr, bit ? kData : 0U);
        FL_CHECK_EQ(gEdges[i].codr, bit ? 0U : kData);
    }
    FL_CHECK_EQ(gFullPortStores, 0);
    FL_CHECK_EQ(gClockLows, static_cast<int>(gEdges.size()));
    FL_CHECK_EQ(gRegs[kPortA][kOdsr] & kClock, 0U);
    FL_CHECK_EQ(gRegs[kPortB][kOdsr] & kClock, 0U);
    FL_CHECK_EQ(gRegs[kPortA][kOdsr] & kOther, kOther);
    FL_CHECK_EQ(gRegs[kPortB][kOdsr] & kOther, kOther);
}

template <u8 CLOCK> void checkWriter() {
    using Spi = GenericSoftwareSPIOutput<240, CLOCK, kDivider>;
    fl::vector<fl::vector<u8>> inputs = {
        {}, {0x00}, {0xFF}, {0xAA}, {0x55}, {0x00, 0xFF, 0xAA, 0x55, 0x81, 0x7E},
    };
    fl::vector<u8> oneHot;
    for (int i = 0; i < 8; ++i) {
        oneHot.push_back(static_cast<u8>(1U << i));
    }
    inputs.push_back(oneHot);
    Spi spi;
    for (const fl::vector<u8>& input : inputs) {
        reset();
        fl::vector<u8> bytes = input;
        spi.template writeBytes<DATA_NOP>(bytes.data(), static_cast<int>(bytes.size()));
        check(input);
    }

    reset();
    Spi::writeByte(0xA5);
    check({0xA5});

    reset();
    Spi::writeBytesValueRaw(0xA5, 3);
    check({0xA5, 0xA5, 0xA5});

    CRGB leds[2] = {CRGB(0x55, 0xAA, 0x12), CRGB(0xAA, 0x55, 0x34)};
    reset();
    PixelController<RGB> pixels(leds, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.template writePixels<0, DATA_NOP, RGB>(pixels);
    check({0x55, 0xAA, 0x12, 0xAA, 0x55, 0x34});

    reset();
    PixelController<RGB> startPixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.template writePixels<FLAG_START_BIT, DATA_NOP, RGB>(startPixels);
    FL_REQUIRE_EQ(gEdges.size(), 25U);
    FL_CHECK(gEdges[0].dataHigh);
    FL_CHECK_EQ(gEdges[0].sodr, kData);
    const u32 rgb = 0x55AA12;
    for (int i = 0; i < 24; ++i) {
        FL_CHECK_EQ(gEdges[i + 1].dataHigh, ((rgb >> (23 - i)) & 1U) != 0);
    }

    // A bit changed while the bus runs survives a snapshot-prone bulk path.
    reset();
    gConcurrentBit = 1U << 20;
    Spi::writeBytesValueRaw(0x5A, 1);
    FL_CHECK_EQ(gRegs[kPortA][kOdsr] & (1U << 20), 1U << 20);
    FL_CHECK_EQ(gFullPortStores, 0);
}

} // namespace

FL_TEST_CASE("Due same-PIO writer uses SODR and CODR, never ODSR") { checkWriter<241>(); }
FL_TEST_CASE("Due different-PIO writer uses SODR and CODR, never ODSR") { checkWriter<242>(); }

FL_TEST_CASE("Due accessors select the template's registers") {
    using P = FastPin<240>;
    FL_CHECK(P::port() == &gRegs[kPortA][kOdsr]);
    FL_CHECK(P::sport() == &gRegs[kPortA][kSodr]);
    FL_CHECK(P::cport() == &gRegs[kPortA][kCodr]);
    FL_CHECK_EQ(P::mask(), kData);

    // The production hi()/lo() store the mask to SODR/CODR and leave ODSR alone.
    reset();
    P::hi();
    FL_CHECK_EQ(gRegs[kPortA][kSodr], kData);
    FL_CHECK_EQ(gRegs[kPortA][kCodr], 0U);
    P::lo();
    FL_CHECK_EQ(gRegs[kPortA][kCodr], kData);
    FL_CHECK_EQ(gRegs[kPortA][kOdsr], kOther);
}

FL_TEST_CASE("Due user-defined pins use hi and lo") {
    FastPin<243>::sHi = 0;
    FastPin<243>::sLo = 0;
    BitbangPin<FastPin<243>>::write<0>(1);
    BitbangPin<FastPin<243>>::write<1>(1);
    FL_CHECK_EQ(FastPin<243>::sHi, 1);
    FL_CHECK_EQ(FastPin<243>::sLo, 1);
}

} // FL_TEST_FILE

} // namespace fl
