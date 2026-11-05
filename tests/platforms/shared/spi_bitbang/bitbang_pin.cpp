#include "platforms/shared/spi_bitbang/bitbang_pin.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;

namespace {
template <u8 Bit> void checkSetClear(u32 mask) {
    for (int value = 0; value < 256; ++value) {
        volatile u32 set = 0x12345678;
        volatile u32 clear = 0x87654321;
        writeBitSetClear<Bit>(static_cast<u8>(value), mask, &set, &clear);
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
