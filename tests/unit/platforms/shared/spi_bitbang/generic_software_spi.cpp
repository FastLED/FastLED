/// @file generic_software_spi.cpp
/// @brief Runs the real GenericSoftwareSPIOutput from a clock latch left high before
/// init. A simulated port samples data only on real low-to-high clock transitions.

#include "test.h"

#include "FastLED.h"
#include "fastspi_types.h"
#include "fl/stl/int.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {
namespace spi_init_test {

constexpr u32 kDataMask = 1U << 3;
constexpr u32 kClockMask = 1U << 17;
constexpr u32 kUnrelated = 0x80000001U;

volatile u32 gPort = 0;
volatile u32 gOtherPort = 0;
bool gClockOnOtherPort = false;
bool gClockInverted = false;
fl::vector<u8> gSamples;

bool clockLevel() {
    const bool high = ((gClockOnOtherPort ? gOtherPort : gPort) & kClockMask) != 0;
    return gClockInverted ? !high : high;
}

void store(volatile u32* port, u32 next) {
    const bool before = clockLevel();
    *port = next;
    if (!before && clockLevel()) {
        gSamples.push_back((gPort & kDataMask) ? 1 : 0);
    }
}

// One pin on a simulated port. INVERTED pins drive the opposite physical level.
template <u32 MASK, bool INVERTED, bool OTHER_PORT = false> struct FakePin {
    typedef volatile u32* port_ptr_t;
    typedef u32 port_t;
    static constexpr bool validpin() { return true; }
    static void setOutput() {} // Keeps the latch, as the bug requires.
    static volatile u32* reg() { return OTHER_PORT ? &gOtherPort : &gPort; }
    static void setPhysical(bool high) { store(reg(), high ? (*reg() | MASK) : (*reg() & ~MASK)); }
    static void hi() { setPhysical(!INVERTED); }
    static void lo() { setPhysical(INVERTED); }
    static void hi(port_ptr_t) { hi(); }
    static void lo(port_ptr_t) { lo(); }
    static void fastset(port_ptr_t port, port_t value) { store(port, value); }
    static port_ptr_t port() { return reg(); }
    static port_t hival() { return INVERTED ? (*reg() & ~MASK) : (*reg() | MASK); }
    static port_t loval() { return INVERTED ? (*reg() | MASK) : (*reg() & ~MASK); }
    static port_t mask() { return MASK; }
};

} // namespace spi_init_test

using spi_init_test::kClockMask;
using spi_init_test::kDataMask;

// 226/227: active-low clock with whole-port snapshots.
template <> class FastPin<226> : public spi_init_test::FakePin<kDataMask, false> {};
template <> class FastPin<227> : public spi_init_test::FakePin<kClockMask, true> {};
// 228/229: active-low clock with same-port streaming.
template <> class FastPin<228> : public spi_init_test::FakePin<kDataMask, false> {};
template <> class FastPin<229> : public spi_init_test::FakePin<kClockMask, true> {};

// 230/231: default policy, whole-port snapshots.
template <> class FastPin<230> : public spi_init_test::FakePin<kDataMask, false> {};
template <> class FastPin<231> : public spi_init_test::FakePin<kClockMask, false> {};
// 232/233: atomic per-pin writes.
template <> class FastPin<232> : public spi_init_test::FakePin<kDataMask, false> {};
template <> class FastPin<233> : public spi_init_test::FakePin<kClockMask, false> {};
// 234/235: same-port streaming.
template <> class FastPin<234> : public spi_init_test::FakePin<kDataMask, false> {};
template <> class FastPin<235> : public spi_init_test::FakePin<kClockMask, false> {};
// 236/237: atomic per-pin writes with an inverted clock.
template <> class FastPin<236> : public spi_init_test::FakePin<kDataMask, false> {};
template <> class FastPin<237> : public spi_init_test::FakePin<kClockMask, true> {};
// 238/239: default policy with the clock on another port.
template <> class FastPin<238> : public spi_init_test::FakePin<kDataMask, false> {};
template <> class FastPin<239> : public spi_init_test::FakePin<kClockMask, false, true> {};

template <> struct BitbangPin<FastPin<228>> : BitbangPinDefaults<FastPin<228>> {
    static constexpr bool kAtomicWrite = true;
    static constexpr bool kTwoWriteStream = true;
};
template <> struct BitbangPin<FastPin<229>> : BitbangPinDefaults<FastPin<229>> {
    static constexpr bool kAtomicWrite = true;
    static constexpr bool kTwoWriteStream = true;
};

template <> struct BitbangPin<FastPin<232>> : BitbangPinDefaults<FastPin<232>> {
    static constexpr bool kAtomicWrite = true;
};
template <> struct BitbangPin<FastPin<233>> : BitbangPinDefaults<FastPin<233>> {
    static constexpr bool kAtomicWrite = true;
};
template <> struct BitbangPin<FastPin<234>> : BitbangPinDefaults<FastPin<234>> {
    static constexpr bool kAtomicWrite = true;
    static constexpr bool kTwoWriteStream = true;
};
template <> struct BitbangPin<FastPin<235>> : BitbangPinDefaults<FastPin<235>> {
    static constexpr bool kAtomicWrite = true;
    static constexpr bool kTwoWriteStream = true;
};
template <> struct BitbangPin<FastPin<236>> : BitbangPinDefaults<FastPin<236>> {
    static constexpr bool kAtomicWrite = true;
};
template <> struct BitbangPin<FastPin<237>> : BitbangPinDefaults<FastPin<237>> {
    static constexpr bool kAtomicWrite = true;
};

FL_TEST_FILE(FL_FILEPATH) {
using namespace spi_init_test;

namespace {

void checkSamples(u8 value) {
    FL_REQUIRE_EQ(gSamples.size(), 8U);
    for (int i = 0; i < 8; ++i) {
        FL_CHECK_EQ(gSamples[i], static_cast<u8>((value >> (7 - i)) & 1));
    }
}

// Primes the clock logically high with an unrelated pin set, then runs init.
template <typename Output> void primeAndInit(Output& output, bool inverted, bool otherPort) {
    gClockInverted = inverted;
    gClockOnOtherPort = otherPort;
    gPort = kUnrelated;
    gOtherPort = kUnrelated;
    (otherPort ? gOtherPort : gPort) |= (inverted ? 0U : kClockMask);
    gSamples.clear();
    output.init();
    FL_REQUIRE_FALSE(clockLevel()); // No select or transaction was needed.
    FL_CHECK_EQ(gPort & kUnrelated, kUnrelated);
    FL_CHECK_EQ(gSamples.size(), 0U);
}

template <u8 DATA, u8 CLOCK> void checkFirstByte(bool inverted, bool otherPort = false) {
    using Output = GenericSoftwareSPIOutput<DATA, CLOCK, 2>;
    {
        Output output;
        primeAndInit(output, inverted, otherPort);
        Output::writeByte(0xA5);
        checkSamples(0xA5);
        FL_CHECK_EQ(gPort & kUnrelated, kUnrelated);
    }
    {
        Output output;
        primeAndInit(output, inverted, otherPort);
        u8 data[] = {0xA5};
        output.writeBytes(data, 1);
        checkSamples(0xA5);
    }
    {
        Output output;
        primeAndInit(output, inverted, otherPort);
        output.writeBytesValueRaw(0xA5, 1);
        checkSamples(0xA5);
    }
    {
        Output output;
        primeAndInit(output, inverted, otherPort);
        CRGB led(0x81, 0, 0);
        PixelController<RGB> pixels(&led, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        output.template writePixels<FLAG_START_BIT, DATA_NOP, RGB>(pixels);
        FL_REQUIRE_EQ(gSamples.size(), 25U); // The start bit is the first edge.
        FL_CHECK_EQ(gSamples[0], 1);
        FL_CHECK_EQ(gSamples[1], 1);
        FL_CHECK_EQ(gSamples[8], 1);
        FL_CHECK_EQ(gSamples[9], 0);
    }
}

} // namespace

FL_TEST_CASE("whole-port snapshots honor an active-low clock") { checkFirstByte<226, 227>(true); }
FL_TEST_CASE("same-port streaming honors an active-low clock") { checkFirstByte<228, 229>(true); }
FL_TEST_CASE("init parks a high clock low: snapshot pins") { checkFirstByte<230, 231>(false); }
FL_TEST_CASE("init parks a high clock low: atomic pins") { checkFirstByte<232, 233>(false); }
FL_TEST_CASE("init parks a high clock low: streaming pins") { checkFirstByte<234, 235>(false); }
FL_TEST_CASE("init parks a high clock low: inverted clock") { checkFirstByte<236, 237>(true); }
FL_TEST_CASE("init parks a high clock low: clock on another port") { checkFirstByte<238, 239>(false, true); }

} // FL_TEST_FILE

} // namespace fl
