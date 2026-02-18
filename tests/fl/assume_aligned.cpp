/// @file assume_aligned.cpp
/// @brief Tests for fl::assume_aligned<N>(ptr) and FL_ASSUME_ALIGNED macro

#include "test.h"
#include "fl/assume_aligned.h"
#include "fl/align.h"
#include "fl/compiler_control.h"

using namespace fl;

FL_TEST_CASE("assume_aligned - basic pointer passthrough") {
    FL_ALIGNAS(64) uint8_t buffer[128];
    uint8_t *p = fl::assume_aligned<64>(buffer);
    FL_CHECK_EQ(p, buffer);
}

FL_TEST_CASE("assume_aligned - const pointer") {
    FL_ALIGNAS(16) const uint8_t data[32] = {};
    const uint8_t *p = fl::assume_aligned<16>(data);
    FL_CHECK_EQ(p, data);
}

FL_TEST_CASE("assume_aligned - typed pointer") {
    FL_ALIGNAS(16) uint32_t values[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint32_t *p = fl::assume_aligned<16>(values);
    FL_CHECK_EQ(p[0], 1u);
    FL_CHECK_EQ(p[7], 8u);
}

FL_TEST_CASE("assume_aligned - different alignments") {
    FL_ALIGNAS(64) uint8_t buf[64];

    uint8_t *p4 = fl::assume_aligned<4>(buf);
    uint8_t *p8 = fl::assume_aligned<8>(buf);
    uint8_t *p16 = fl::assume_aligned<16>(buf);
    uint8_t *p32 = fl::assume_aligned<32>(buf);
    uint8_t *p64 = fl::assume_aligned<64>(buf);

    FL_CHECK_EQ(p4, buf);
    FL_CHECK_EQ(p8, buf);
    FL_CHECK_EQ(p16, buf);
    FL_CHECK_EQ(p32, buf);
    FL_CHECK_EQ(p64, buf);
}

FL_TEST_CASE("FL_ASSUME_ALIGNED macro") {
    FL_ALIGNAS(64) uint8_t buffer[128];
    uint8_t *p = FL_ASSUME_ALIGNED(buffer, 64);
    FL_CHECK_EQ(p, buffer);

    FL_ALIGNAS(16) const uint32_t data[4] = {10, 20, 30, 40};
    const uint32_t *q = FL_ASSUME_ALIGNED(data, 16);
    FL_CHECK_EQ(q[0], 10u);
    FL_CHECK_EQ(q[3], 40u);
}
