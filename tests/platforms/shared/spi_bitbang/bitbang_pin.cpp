#include "platforms/shared/spi_bitbang/bitbang_pin.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;

namespace {
template <u8 Bit> void checkSetClear() {
    for (int value = 0; value < 256; ++value) {
        volatile u32 set = 0x12345678;
        volatile u32 clear = 0x87654321;
        writeBitSetClear<Bit>(static_cast<u8>(value), 1UL << 4, &set, &clear);
        if (value & (1 << Bit)) {
            FL_CHECK_EQ(set, 1UL << 4);
            FL_CHECK_EQ(clear, 0x87654321);
        } else {
            FL_CHECK_EQ(set, 0x12345678);
            FL_CHECK_EQ(clear, 1UL << 4);
        }
    }
}
} // namespace

FL_TEST_CASE("Bit-bang set-clear writer touches exactly the requested register") {
    checkSetClear<0>();
    checkSetClear<1>();
    checkSetClear<2>();
    checkSetClear<3>();
    checkSetClear<4>();
    checkSetClear<5>();
    checkSetClear<6>();
    checkSetClear<7>();
}
} // FL_TEST_FILE
