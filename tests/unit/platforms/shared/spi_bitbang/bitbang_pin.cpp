#include "platforms/shared/spi_bitbang/bitbang_pin.h"
#include "test.h"

#include "fastspi.h"
#include "fl/stl/type_traits.h"

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;

namespace {
template <u8 Bit> void checkSetClear(u32 mask) {
    for (int value = 0; value < 256; ++value) {
        volatile u32 set = 0x12345678;
        volatile u32 clear = 0x87654321;
        writeBitSetClear<Bit>(static_cast<u8>(value), mask, &set, &clear);
        if (value & (1 << Bit)) { FL_CHECK_EQ(set, mask); } else { FL_CHECK_EQ(clear, mask); }
        volatile u32 set2 = 0;
        volatile u32 clear2 = 0;
        writeBitSetClearNoBranch<Bit>(static_cast<u8>(value), mask, &set2, &clear2);
        FL_CHECK_EQ(set2, (value & (1 << Bit)) ? mask : 0U);
        FL_CHECK_EQ(clear2, (value & (1 << Bit)) ? 0U : mask);
        if (value & (1 << Bit)) {
            FL_CHECK_EQ(set, mask);
            FL_CHECK_EQ(clear, 0x87654321);
        } else {
            FL_CHECK_EQ(set, 0x12345678);
            FL_CHECK_EQ(clear, mask);
        }
    }
}
} // namespace

FL_TEST_CASE("Bit-bang set-clear writer touches exactly the requested register") {
    const u32 masks[] = {1UL, 1UL << 4, 1UL << 15, 1UL << 31, 0x00F0F00FUL};
    for (u32 mask : masks) {
        checkSetClear<0>(mask);
        checkSetClear<1>(mask);
        checkSetClear<2>(mask);
        checkSetClear<3>(mask);
        checkSetClear<4>(mask);
        checkSetClear<5>(mask);
        checkSetClear<6>(mask);
        checkSetClear<7>(mask);
    }
}

FL_TEST_CASE("bitbangDividerFromHz converts a rate to cycles per bit") {
    FL_CHECK_EQ(bitbangDividerFromHz(1000000, 240000000), 240U);
    FL_CHECK_EQ(bitbangDividerFromHz(500000, 160000000), 320U);
    FL_CHECK_EQ(bitbangDividerFromHz(1000000, 80000000), 80U);
    FL_CHECK_EQ(bitbangDividerFromHz(7000000, 240000000), 35U);  // Rounds up from 34.3.
    // Clamped above 10, where the writer's half-period branch would invert the rate.
    FL_CHECK_EQ(bitbangDividerFromHz(21000000, 240000000), 12U);
    FL_CHECK_EQ(bitbangDividerFromHz(24000000, 240000000), 11U);
    FL_CHECK_EQ(bitbangDividerFromHz(240000000, 240000000), 11U);
    FL_CHECK_EQ(bitbangDividerFromHz(1000000000, 240000000), 11U);
    FL_CHECK_EQ(bitbangDividerFromHz(1, 240000000), 240000000U);
}

// ESP32 and the host stub both define DATA_RATE_* in Hz (fastspi.h), so the
// public SoftwareSPIOutput must convert to the divider GenericSoftwareSPIOutput
// consumes. Forwarding the Hz value made DATA_RATE_MHZ(1) a ~1e6-cycle
// divider (#5051).
template <u32 HZ>
using ExpectedSoftwareSpi = GenericSoftwareSPIOutput<2, 3, bitbangDividerFromHz(HZ, FL_CPU_FREQUENCY())>;
template <u32 HZ>
using RawHzSoftwareSpi = GenericSoftwareSPIOutput<2, 3, HZ>;

FL_TEST_CASE("SoftwareSPIOutput converts DATA_RATE_MHZ/KHZ from Hz to a divider") {
    FL_CHECK_EQ(DATA_RATE_MHZ(1), 1000000);
    FL_CHECK_EQ(DATA_RATE_KHZ(250), 250000);
    FL_CHECK((fl::is_base_of<ExpectedSoftwareSpi<1000000>, ::SoftwareSPIOutput<2, 3, DATA_RATE_MHZ(1)>>::value));
    FL_CHECK((fl::is_base_of<ExpectedSoftwareSpi<250000>, ::SoftwareSPIOutput<2, 3, DATA_RATE_KHZ(250)>>::value));
    FL_CHECK((fl::is_base_of<ExpectedSoftwareSpi<100000>, ::SoftwareSPIOutput<2, 3, DATA_RATE_KHZ(100)>>::value));
    FL_CHECK_FALSE((fl::is_base_of<RawHzSoftwareSpi<1000000>, ::SoftwareSPIOutput<2, 3, DATA_RATE_MHZ(1)>>::value));
    FL_CHECK_FALSE((fl::is_base_of<RawHzSoftwareSpi<250000>, ::SoftwareSPIOutput<2, 3, DATA_RATE_KHZ(250)>>::value));
    // The same request is four times the divider at a quarter of the rate.
    FL_CHECK_EQ(bitbangDividerFromHz(DATA_RATE_KHZ(250), 160000000), 640U);
    FL_CHECK_EQ(bitbangDividerFromHz(DATA_RATE_MHZ(1), 160000000), 160U);
}

FL_TEST_CASE("Hz phase delay keeps sub-MHz and fractional rates") {
    FL_CHECK_EQ(bitbangPhaseNsFromHz(100000), 2500U);
    FL_CHECK_EQ(bitbangPhaseNsFromHz(500000), 500U);
    FL_CHECK_EQ(bitbangPhaseNsFromHz(1000000), 250U);
    FL_CHECK_EQ(bitbangPhaseNsFromHz(1500000), 166U);
    FL_CHECK_EQ(bitbangPhaseNsFromHz(4000000), 62U);
    FL_CHECK_EQ(bitbangPhaseNsFromHz(0), 0U);
}

FL_TEST_CASE("Bit-bang phase padding meets the requested period and minimum pulse") {
    for (cycle_t requested = 0; requested <= 100; ++requested) {
        for (cycle_t work = 0; work <= 24; ++work) {
            for (cycle_t pulse = 0; pulse <= 8; ++pulse) {
                const cycle_t pad = bitbangPhaseDelay(requested, pulse, work);
                FL_REQUIRE_GE(pad, 0);
                FL_CHECK_GE(work + pad, requested);
                FL_CHECK_GE(work + pad, pulse);
                if (pad > 0) {
                    FL_CHECK((work + pad - 1 < requested) || (work + pad - 1 < pulse));
                }
            }
        }
    }
}
} // FL_TEST_FILE
