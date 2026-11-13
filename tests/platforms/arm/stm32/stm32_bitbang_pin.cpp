/// @file stm32_bitbang_pin.cpp
/// @brief Runs the production STM32SPIOutput and generic software SPI writers
/// through the real unified STM32 pin template and its BitbangPin policy on a fake
/// GPIO block, for families with and without a BRR register.
///
/// A fixture clock pin shadows hi/lo to apply and sample the data registers. The
/// data pin is production code apart from a fastset counter.

#include "test.h"

// Exercise the opt-in optimized bit-bang policy; it defaults off.
#ifndef FL_STM32_ENABLE_FAST_SOFTWARE_SPI
#define FL_STM32_ENABLE_FAST_SOFTWARE_SPI 1
#endif

#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"

// Target builds use C++17 for this template's `if constexpr`; host tests do not.
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(c++17-extensions)
// The host stubs overload ::pinMode, so give the template's constants an exact match.
#undef OUTPUT
#undef INPUT
#define OUTPUT static_cast<fl::u8>(1)
#define INPUT static_cast<fl::u8>(0)
#include "platforms/arm/stm32/pins/core/armpin_template.h"
#undef OUTPUT
#undef INPUT
#define OUTPUT 1
#define INPUT 0
FL_DISABLE_WARNING_POP

#include "fastspi_types.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/arm/stm32/fastspi_arm_stm32.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {
namespace stm32_test {

struct FakeGpio {
    volatile u32 ODR, BSRR, BRR;
};
FakeGpio gGpio;
struct Port {
    static FakeGpio* r() { return &gGpio; }
};

constexpr u8 kDataBit = 3;
constexpr u32 kData = 1U << kDataBit;
constexpr u32 kClock = 1U << 12;
constexpr u32 kOther = 0x8001U; // Unrelated output bits owned by the sketch.
constexpr u32 kDivider = 4;

struct Edge {
    bool dataHigh;
    u32 bsrr; // BSRR store made for this bit, or 0.
    u32 brr;  // BRR store made for this bit, or 0.
};

fl::vector<Edge> gEdges;
int gFullPortStores = 0;
int gClockLows = 0;
u32 gConcurrentBit = 0; // Set during each rising edge, like an ISR.

template <u8 PIN, bool HAS_BRR> using DataBase = _ARMPIN_STM32<PIN, kDataBit, kData, Port, HAS_BRR>;
template <u8 PIN, bool HAS_BRR> using ClockBase = _ARMPIN_STM32<PIN, 12, kClock, Port, HAS_BRR>;

template <u8 PIN, bool HAS_BRR> struct TracedData : DataBase<PIN, HAS_BRR> {
    using B = DataBase<PIN, HAS_BRR>;
    static void fastset(typename B::port_ptr_t port, typename B::port_t value) {
        B::fastset(port, value);
        ++gFullPortStores;
    }
};

// The clock applies pending stores to ODR, as hardware does, and clears them so
// the next edge shows only the store made for its own bit.
template <u8 PIN, bool HAS_BRR> struct TracedClock : ClockBase<PIN, HAS_BRR> {
    static void hi() {
        const u32 bsrr = gGpio.BSRR;
        const u32 brr = gGpio.BRR;
        gGpio.BSRR = 0;
        gGpio.BRR = 0;
        u32 odr = gGpio.ODR | gConcurrentBit;
        odr = (odr | (bsrr & 0xFFFFU)) & ~(bsrr >> 16) & ~brr;
        gGpio.ODR = odr | kClock;
        gEdges.push_back({(odr & kData) != 0, bsrr, brr});
    }
    static void lo() {
        ++gClockLows;
        gGpio.ODR = gGpio.ODR & ~kClock;
    }
};

} // namespace stm32_test

template <> class FastPin<240> : public stm32_test::TracedData<240, true> {};
template <> class FastPin<241> : public stm32_test::TracedClock<241, true> {};
template <> class FastPin<242> : public stm32_test::TracedData<242, false> {};
template <> class FastPin<243> : public stm32_test::TracedClock<243, false> {};

// A user-defined pin without the BSRR accessor must still compile and use hi()/lo().
template <> class FastPin<244> : public ValidPinBase {
  public:
    static int sHi;
    static int sLo;
    static void hi() { ++sHi; }
    static void lo() { ++sLo; }
};
int FastPin<244>::sHi = 0;
int FastPin<244>::sLo = 0;

FL_STATIC_ASSERT(!BitbangPin<FastPin<244>>::kAtomicWrite, "user-defined pins keep the default policy");
FL_STATIC_ASSERT(BitbangPin<FastPin<240>>::kAtomicWrite && BitbangPin<FastPin<242>>::kAtomicWrite,
                 "unified STM32 pins use BSRR data writes");
FL_STATIC_ASSERT(!BitbangPin<FastPin<240>>::kTwoWriteStream && !BitbangPin<FastPin<242>>::kTwoWriteStream,
                 "whole-port streaming stays off on STM32");

FL_TEST_FILE(FL_FILEPATH) {
using namespace stm32_test;

namespace {

void reset() {
    gGpio.ODR = kOther;
    gGpio.BSRR = 0;
    gGpio.BRR = 0;
    gEdges.clear();
    gFullPortStores = 0;
    gClockLows = 0;
    gConcurrentBit = 0;
}

// One rising edge per bit with the right data, one 32-bit BSRR store per bit on
// every family, no BRR, no whole-port store, and the clock ends low.
void check(const fl::vector<u8>& expected) {
    FL_REQUIRE_EQ(gEdges.size(), expected.size() * 8);
    for (size_t i = 0; i < gEdges.size(); ++i) {
        const bool bit = (expected[i / 8] >> (7 - (i % 8))) & 1;
        FL_CHECK_EQ(gEdges[i].dataHigh, bit);
        FL_CHECK_EQ(gEdges[i].bsrr, bit ? kData : (kData << 16));
        FL_CHECK_EQ(gEdges[i].brr, 0U);
    }
    FL_CHECK_EQ(gFullPortStores, 0);
    FL_CHECK_EQ(gClockLows, static_cast<int>(gEdges.size()));
    FL_CHECK_EQ(gGpio.ODR & kClock, 0U);
    FL_CHECK_EQ(gGpio.ODR & kOther, kOther);
}

fl::vector<fl::vector<u8>> patterns() {
    fl::vector<fl::vector<u8>> inputs = {
        {}, {0x00}, {0xFF}, {0xAA}, {0x55}, {0x00, 0xFF, 0xAA, 0x55, 0x81, 0x7E},
    };
    fl::vector<u8> oneHot;
    for (int i = 0; i < 8; ++i) {
        oneHot.push_back(static_cast<u8>(1U << i));
    }
    inputs.push_back(oneHot);
    return inputs;
}

template <class Spi> void checkWriter() {
    for (const fl::vector<u8>& input : patterns()) {
        reset();
        Spi spi;
        fl::vector<u8> bytes = input;
        spi.template writeBytes<DATA_NOP>(bytes.data(), static_cast<int>(bytes.size()));
        check(input);
    }

    reset();
    Spi spi;
    spi.writeByte(0xA5);
    check({0xA5});

    reset();
    spi.writeBytesValueRaw(0xA5, 3);
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
    FL_CHECK_EQ(gEdges[0].bsrr, kData);
    const u32 rgb = 0x55AA12;
    for (int i = 0; i < 24; ++i) {
        FL_CHECK_EQ(gEdges[i + 1].dataHigh, ((rgb >> (23 - i)) & 1U) != 0);
    }

    // A bit changed while the bus runs survives: nothing rewrites ODR from a snapshot.
    reset();
    gConcurrentBit = 1U << 9;
    spi.writeBytesValueRaw(0x5A, 1);
    FL_CHECK_EQ(gGpio.ODR & gConcurrentBit, gConcurrentBit);
}

} // namespace

FL_TEST_CASE("STM32SPIOutput uses one BSRR store per bit with BRR") {
    checkWriter<STM32SPIOutput<240, 241, kDivider>>();
}
FL_TEST_CASE("STM32SPIOutput uses one BSRR store per bit without BRR") {
    checkWriter<STM32SPIOutput<242, 243, kDivider>>();
}
FL_TEST_CASE("Generic software SPI uses one BSRR store per bit with BRR") {
    checkWriter<GenericSoftwareSPIOutput<240, 241, kDivider>>();
}
FL_TEST_CASE("Generic software SPI uses one BSRR store per bit without BRR") {
    checkWriter<GenericSoftwareSPIOutput<242, 243, kDivider>>();
}

FL_TEST_CASE("STM32 user-defined pins use hi and lo") {
    FastPin<244>::sHi = 0;
    FastPin<244>::sLo = 0;
    BitbangPin<FastPin<244>>::write<0>(1);
    BitbangPin<FastPin<244>>::write<1>(1);
    FL_CHECK_EQ(FastPin<244>::sHi, 1);
    FL_CHECK_EQ(FastPin<244>::sLo, 1);
}

} // FL_TEST_FILE

} // namespace fl
