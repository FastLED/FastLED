/// @file fastpin_arm_lpc11_legacy.cpp
/// @brief Runs the production generic software SPI writer through the real legacy
/// LPC11xx _ARMPIN template, its register layout and its BitbangPin policy, with
/// the documented GPIO base pointed at host memory.
///
/// A clock pin fixture shadows hi/lo to sample the data pin's masked-access slot.
/// The data pin is production code apart from a fastset counter.

#include "test.h"

// Exercise the opt-in optimized bit-bang policy; it defaults off.
#ifndef FL_LPC_ENABLE_FAST_SOFTWARE_SPI
#define FL_LPC_ENABLE_FAST_SOFTWARE_SPI 1
#endif

#include "fl/stl/int.h"

#ifndef FL_IS_ARM_LPC_11_LEGACY
#define FL_IS_ARM_LPC_11_LEGACY
#endif

static fl::u32 gFakeGpioMemory[0x10000]; // Four 0x10000-byte port blocks.
#define FL_LPC11_LEGACY_GPIO_BASE reinterpret_cast<fl::uptr>(gFakeGpioMemory) // ok reinterpret cast

#include "platforms/arm/lpc/fastpin_arm_lpc11_legacy.h"

#include "fastspi_types.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {

namespace lpc11_test {

constexpr u32 kData = 1U << 3;
constexpr u32 kClock = 1U << 7;
constexpr u32 kUnset = 0xDEADBEEFU; // Slot contents when no store happened.
constexpr u32 kOther = 0x801U;      // Unrelated DATA bits owned by the sketch.
constexpr u8 kDataPin = 240;
constexpr u8 kClockPin = 241;
constexpr u8 kOtherPortClockPin = 242;
constexpr u32 kDivider = 4;

fl::vector<u32> gSlotAtEdge;
int gFullPortStores = 0;
int gClockStores = 0;

using DataBase = _ARMPIN_LPC11LEGACY<kDataPin, kData, 0>;

} // namespace lpc11_test

template <> class FastPin<lpc11_test::kDataPin> : public lpc11_test::DataBase {
  public:
    static void fastset(port_ptr_t port, port_t value) {
        lpc11_test::DataBase::fastset(port, value);
        ++lpc11_test::gFullPortStores;
    }
};

// Each rising edge samples and re-arms the data slot, so a missing store shows as
// kUnset and each edge sees only the store made for its own bit.
template <u8 PIN, u8 GROUP> struct TracedClock : _ARMPIN_LPC11LEGACY<PIN, lpc11_test::kClock, GROUP> {
    static void hi() {
        volatile u32* slot = FastPin<lpc11_test::kDataPin>::sport();
        const u32 sampled = *slot;
        lpc11_test::gSlotAtEdge.push_back(sampled);
        *slot = lpc11_test::kUnset;
        ++lpc11_test::gClockStores;
    }
    static void lo() { ++lpc11_test::gClockStores; }
};

template <> class FastPin<lpc11_test::kClockPin> : public TracedClock<lpc11_test::kClockPin, 0> {};
template <> class FastPin<lpc11_test::kOtherPortClockPin> : public TracedClock<lpc11_test::kOtherPortClockPin, 1> {};

// A user-defined pin without masked access must still compile and use hi()/lo().
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
FL_STATIC_ASSERT(BitbangPin<FastPin<2>>::kAtomicWrite && BitbangPin<FastPin<41>>::kAtomicWrite,
                 "real table pins use masked-access data writes");
FL_STATIC_ASSERT(!BitbangPin<FastPin<2>>::kTwoWriteStream && !BitbangPin<FastPin<41>>::kTwoWriteStream,
                 "full-port streaming stays off on legacy LPC11xx");

FL_TEST_FILE(FL_FILEPATH) {
using namespace lpc11_test;

using SamePort = GenericSoftwareSPIOutput<kDataPin, kClockPin, kDivider>;
using SplitPort = GenericSoftwareSPIOutput<kDataPin, kOtherPortClockPin, kDivider>;

namespace {

void reset() {
    *FastPin<kDataPin>::sport() = kUnset;
    *FastPin<kDataPin>::port() = kOther;
    gSlotAtEdge.clear();
    gFullPortStores = 0;
    gClockStores = 0;
}

// Each edge sees a fresh store of the mask or zero, in MSB-first order; a clock
// low ends each bit; and nothing touched the full-port DATA register. Host memory
// keeps only the last store, so the store count comes from target disassembly.
void checkBits(const fl::vector<u8>& expected) {
    FL_REQUIRE_EQ(gSlotAtEdge.size(), expected.size() * 8);
    for (size_t i = 0; i < gSlotAtEdge.size(); ++i) {
        const bool bit = (expected[i / 8] >> (7 - (i % 8))) & 1;
        FL_CHECK_EQ(gSlotAtEdge[i], bit ? kData : 0U);
    }
    FL_CHECK_EQ(gClockStores, static_cast<int>(gSlotAtEdge.size() * 2));
    FL_CHECK_EQ(gFullPortStores, 0);
    FL_CHECK_EQ(*FastPin<kDataPin>::port(), kOther);
}

template <class Spi> void checkBulk() {
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
        spi.template writeBytes<DATA_NOP>(bytes.data(), static_cast<int>(bytes.size()));
        checkBits(input);
    }
    reset();
    Spi::writeBytesValueRaw(0xA5, 3);
    checkBits({0xA5, 0xA5, 0xA5});
}

} // namespace

FL_TEST_CASE("LPC11 production accessors use the UM10398 layout") {
    const uptr base = FL_LPC11_LEGACY_GPIO_BASE;
    using P0 = FastPin<3>;   // PIO0_3
    using P1 = FastPin<14>;  // PIO1_2
    FL_CHECK(P0::port() == fl::bit_cast<volatile u32*>(base + 0x3FFC));
    FL_CHECK(P0::sport() == fl::bit_cast<volatile u32*>(base + ((1U << 3) << 2)));
    FL_CHECK(P1::port() == fl::bit_cast<volatile u32*>(base + 0x10000 + 0x3FFC));
    FL_CHECK(P1::sport() == fl::bit_cast<volatile u32*>(base + 0x10000 + ((1U << 2) << 2)));
    FL_CHECK_EQ(P1::mask(), 1U << 2);

    *fl::bit_cast<volatile u32*>(base + 0x8000) = 0;
    P0::setOutput();
    FL_CHECK_EQ(*fl::bit_cast<volatile u32*>(base + 0x8000), 1U << 3);
    P0::setInput();
    FL_CHECK_EQ(*fl::bit_cast<volatile u32*>(base + 0x8000), 0U);
}

FL_TEST_CASE("LPC11 same-port bulk writes use masked access, not snapshots") { checkBulk<SamePort>(); }
FL_TEST_CASE("LPC11 cross-port bulk writes use masked access") { checkBulk<SplitPort>(); }

FL_TEST_CASE("LPC11 writePixels preserves payload and start bit") {
    CRGB leds[1] = {CRGB(0x12, 0x34, 0x56)};
    reset();
    SamePort spi;
    PixelController<RGB> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.writePixels<0, DATA_NOP, RGB>(pixels);
    checkBits({0x12, 0x34, 0x56});

    reset();
    PixelController<RGB> startPixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.writePixels<FLAG_START_BIT, DATA_NOP, RGB>(startPixels);
    FL_REQUIRE_EQ(gSlotAtEdge.size(), 25U);
    FL_CHECK_EQ(gSlotAtEdge[0], kData);
    const u32 rgb = 0x123456;
    for (int i = 0; i < 24; ++i) {
        FL_CHECK_EQ(gSlotAtEdge[i + 1], ((rgb >> (23 - i)) & 1U) ? kData : 0U);
    }
}

FL_TEST_CASE("LPC11 public writeByte uses one masked store per bit") {
    reset();
    SamePort::writeByte(0xA5);
    checkBits({0xA5});
}

FL_TEST_CASE("LPC11 user-defined pins use hi and lo") {
    FastPin<243>::sHi = 0;
    FastPin<243>::sLo = 0;
    BitbangPin<FastPin<243>>::write<0>(1);
    BitbangPin<FastPin<243>>::write<1>(1);
    FL_CHECK_EQ(FastPin<243>::sHi, 1);
    FL_CHECK_EQ(FastPin<243>::sLo, 1);
}

} // FL_TEST_FILE

} // namespace fl
