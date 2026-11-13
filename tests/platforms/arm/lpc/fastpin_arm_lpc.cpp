/// @file fastpin_arm_lpc.cpp
/// @brief Runs the production generic software SPI writer through the real
/// modern LPC _ARMPIN template and BitbangPin policy on a fake GPIO block.
///
/// Fixture pins shadow only fastset/hi/lo to record each store. port(), sport(),
/// cport(), mask(), hival() and loval() are the production accessors. Only PORT0
/// is wired on this template, so there is no cross-port case.

#include "test.h"

// Exercise the opt-in optimized bit-bang policy; it defaults off.
#ifndef FL_LPC_ENABLE_FAST_SOFTWARE_SPI
#define FL_LPC_ENABLE_FAST_SOFTWARE_SPI 1
#endif

#include "fl/stl/int.h"

#ifndef FL_IS_ARM_LPC_845
#define FL_IS_ARM_LPC_845
#endif

struct FakeLpcGpio {
    volatile fl::u32 PIN[2], SET[2], CLR[2], NOT[2], DIRSET[2], DIRCLR[2];
};
static FakeLpcGpio gFakeGpio;
#undef GPIO
#define GPIO (&gFakeGpio)

#include "platforms/arm/lpc/fastpin_arm_lpc.h"

#include "fastspi_types.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {
namespace lpc_test {

constexpr u32 kData = 1U << 3;
constexpr u32 kClock = 1U << 17;
constexpr u32 kOther = 0x80000001U; // Unrelated output bits owned by the sketch.
constexpr u8 kDataPin = 240;
constexpr u8 kClockPin = 241;
constexpr u32 kDivider = 4;

struct Rising {
    bool bit;
    u32 setStores;   // Non-zero when data was raised through SET.
    u32 clearStores; // Non-zero when data was lowered through CLR.
};

fl::vector<u32> gFullPortStores;
fl::vector<Rising> gEdges;
int gClockAliasStores = 0;
u32 gConcurrentLatchBit = 0; // Set during each rising edge, like an ISR.

} // namespace lpc_test

template <> class FastPin<lpc_test::kDataPin> : public _ARMPIN<lpc_test::kDataPin, lpc_test::kData> {
    using B = _ARMPIN<lpc_test::kDataPin, lpc_test::kData>;
  public:
    static void fastset(B::port_ptr_t port, B::port_t value) {
        B::fastset(port, value);
        lpc_test::gFullPortStores.push_back(value);
        if (value & lpc_test::kClock) {
            lpc_test::gEdges.push_back({(value & lpc_test::kData) != 0, 0, 0});
        }
    }
};

// The clock pin samples SET and CLR on each rising edge, then clears them so the
// next edge shows only the store made for its own bit.
template <> class FastPin<lpc_test::kClockPin> : public _ARMPIN<lpc_test::kClockPin, lpc_test::kClock> {
  public:
    static void hi() {
        const u32 s = gFakeGpio.SET[0];
        const u32 c = gFakeGpio.CLR[0];
        gFakeGpio.SET[0] = 0;
        gFakeGpio.CLR[0] = 0;
        gFakeGpio.PIN[0] = ((gFakeGpio.PIN[0] | lpc_test::gConcurrentLatchBit | s) & ~c) | lpc_test::kClock;
        lpc_test::gEdges.push_back({s != 0, s, c});
        ++lpc_test::gClockAliasStores;
    }
    static void lo() {
        ++lpc_test::gClockAliasStores;
        gFakeGpio.PIN[0] = gFakeGpio.PIN[0] & ~lpc_test::kClock;
    }
};

// A user-defined pin without alias registers must still compile and use hi()/lo().
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
FL_STATIC_ASSERT(BitbangPin<FastPin<lpc_test::kDataPin>>::kTwoWriteStream,
                 "modern LPC hardware pins must stream on an exclusive bus");
FL_STATIC_ASSERT(BitbangPin<FastPin<lpc_test::kDataPin>>::kAtomicWrite,
                 "modern LPC hardware pins must update data through SET and CLR");
FL_STATIC_ASSERT(BitbangPin<FastPin<3>>::kTwoWriteStream && BitbangPin<FastPin<31>>::kAtomicWrite,
                 "real table pins, including a high legal pin, take the policy");

FL_TEST_FILE(FL_FILEPATH) {
using namespace lpc_test;

using SamePort = GenericSoftwareSPIOutput<kDataPin, kClockPin, kDivider>;

namespace {

void reset() {
    gFakeGpio.PIN[0] = kOther;
    gFakeGpio.SET[0] = 0;
    gFakeGpio.CLR[0] = 0;
    gFullPortStores.clear();
    gEdges.clear();
    gClockAliasStores = 0;
    gConcurrentLatchBit = 0;
}

// PIN reads pin state while SET reads the latch, so model an unrelated output held
// low externally: PIN shows 0 for it and the snapshot must still keep it high.
void resetForStream() {
    reset();
    gFakeGpio.SET[0] = kOther;
    gFakeGpio.PIN[0] = 0;
}

void checkBytes(const fl::vector<u8>& expected) {
    FL_REQUIRE_EQ(gEdges.size(), expected.size() * 8); // Rejects stray trailing edges.
    for (size_t i = 0; i < gEdges.size(); ++i) {
        const bool bit = (expected[i / 8] >> (7 - (i % 8))) & 1;
        FL_CHECK_EQ(gEdges[i].bit, bit);
    }
}

void checkStream(const fl::vector<u8>& input) {
    FL_CHECK_EQ(gFullPortStores.size(), input.size() * 16);
    FL_CHECK_EQ(gClockAliasStores, input.empty() ? 0 : 1); // The final clock low.
    checkBytes(input);
    for (u32 stored : gFullPortStores) {
        FL_CHECK_EQ(stored & kOther, kOther);
    }
    FL_CHECK_EQ(gFakeGpio.PIN[0] & kClock, 0U);
    if (!input.empty()) {
        FL_CHECK_EQ(gFakeGpio.PIN[0] & kOther, kOther);
    }
}

} // namespace

FL_TEST_CASE("LPC production accessors use the documented GPIO registers") {
    using P = FastPin<3>;
    FL_CHECK(P::port() == &gFakeGpio.PIN[0]);
    FL_CHECK(P::sport() == &gFakeGpio.SET[0]);
    FL_CHECK(P::cport() == &gFakeGpio.CLR[0]);
    FL_CHECK_EQ(P::mask(), 1U << 3);
    FL_CHECK_EQ(FastPin<31>::mask(), 1U << 31);
}

FL_TEST_CASE("LPC writeBytes streams two PIN stores per bit") {
    fl::vector<fl::vector<u8>> inputs = {
        {}, {0x00}, {0xFF}, {0xAA}, {0x55}, {0x00, 0xFF, 0xAA, 0x55, 0x81, 0x7E},
    };
    fl::vector<u8> oneHot;
    for (int i = 0; i < 8; ++i) {
        oneHot.push_back(static_cast<u8>(1U << i));
    }
    inputs.push_back(oneHot);
    for (const fl::vector<u8>& input : inputs) {
        resetForStream();
        SamePort spi;
        fl::vector<u8> bytes = input;
        spi.writeBytes<DATA_NOP>(bytes.data(), static_cast<int>(bytes.size()));
        checkStream(input);
    }
}

FL_TEST_CASE("LPC writeBytesValueRaw and writePixels stream") {
    resetForStream();
    SamePort::writeBytesValueRaw(0xA5, 3);
    checkStream({0xA5, 0xA5, 0xA5});

    CRGB leds[1] = {CRGB(0x12, 0x34, 0x56)};
    resetForStream();
    SamePort spi;
    PixelController<RGB> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.writePixels<0, DATA_NOP, RGB>(pixels);
    checkStream({0x12, 0x34, 0x56});
}

FL_TEST_CASE("LPC writePixels start bit streams 25 edges per pixel") {
    CRGB leds[1] = {CRGB(0x12, 0x34, 0x56)};
    resetForStream();
    SamePort spi;
    PixelController<RGB> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.writePixels<FLAG_START_BIT, DATA_NOP, RGB>(pixels);
    FL_REQUIRE_EQ(gEdges.size(), 25U);
    FL_CHECK_EQ(gFullPortStores.size(), 50U);
    FL_CHECK(gEdges[0].bit);
    const u32 rgb = 0x123456;
    for (int i = 0; i < 24; ++i) {
        FL_CHECK_EQ(gEdges[i + 1].bit, ((rgb >> (23 - i)) & 1U) != 0);
    }
}

FL_TEST_CASE("LPC public writeByte uses SET and CLR and keeps concurrent changes") {
    reset();
    gConcurrentLatchBit = 1U << 9;
    SamePort::writeByte(0xA5);
    FL_CHECK(gFullPortStores.empty());
    checkBytes({0xA5});
    for (const Rising& edge : gEdges) {
        // Exactly the data mask, on exactly one register, once per bit.
        FL_CHECK_EQ(edge.setStores, edge.bit ? kData : 0U);
        FL_CHECK_EQ(edge.clearStores, edge.bit ? 0U : kData);
    }
    FL_CHECK_EQ(gFakeGpio.PIN[0], kOther | kData | (1U << 9)); // 0xA5 ends on a high bit.
}

FL_TEST_CASE("LPC user-defined pins without alias registers use hi and lo") {
    FastPin<243>::sHi = 0;
    FastPin<243>::sLo = 0;
    BitbangPin<FastPin<243>>::write<0>(1);
    BitbangPin<FastPin<243>>::write<1>(1);
    FL_CHECK_EQ(FastPin<243>::sHi, 1);
    FL_CHECK_EQ(FastPin<243>::sLo, 1);
}

} // FL_TEST_FILE

} // namespace fl
