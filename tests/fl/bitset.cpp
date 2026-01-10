#include "fl/stl/bitset.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/move.h"
#include "fl/int.h"

using namespace fl;

// Test the utility functions
TEST_CASE("fl::popcount") {
    CHECK_EQ(popcount(0u), 0);
    CHECK_EQ(popcount(1u), 1);
    CHECK_EQ(popcount(3u), 2);
    CHECK_EQ(popcount(7u), 3);
    CHECK_EQ(popcount(15u), 4);
    CHECK_EQ(popcount(255u), 8);
}

TEST_CASE("fl::countr_zero") {
    CHECK_EQ(countr_zero(1u), 0);
    CHECK_EQ(countr_zero(2u), 1);
    CHECK_EQ(countr_zero(4u), 2);
    CHECK_EQ(countr_zero(8u), 3);
    CHECK_EQ(countr_zero(16u), 4);
}

// Test BitsetFixed class
TEST_CASE("fl::BitsetFixed<8> - basic operations") {
    BitsetFixed<8> bs;

    SUBCASE("construction and size") {
        CHECK_EQ(bs.size(), 8);
        CHECK(bs.none());
        CHECK(!bs.any());
    }

    SUBCASE("set and test") {
        bs.set(0);
        CHECK(bs.test(0));
        CHECK(!bs.test(1));
        CHECK(bs.any());
        CHECK_EQ(bs.count(), 1);

        bs.set(7);
        CHECK(bs.test(7));
        CHECK_EQ(bs.count(), 2);
    }

    SUBCASE("reset") {
        bs.set(0).set(3).set(7);
        CHECK_EQ(bs.count(), 3);

        bs.reset(3);
        CHECK(!bs.test(3));
        CHECK_EQ(bs.count(), 2);

        bs.reset();
        CHECK(bs.none());
    }

    SUBCASE("flip") {
        bs.flip(2);
        CHECK(bs.test(2));
        bs.flip(2);
        CHECK(!bs.test(2));

        bs.set(0).set(4);
        bs.flip();
        CHECK(!bs.test(0));
        CHECK(!bs.test(4));
        CHECK(bs.test(1));
        CHECK_EQ(bs.count(), 6);
    }
}

TEST_CASE("fl::BitsetFixed<8> - count, any, none") {
    BitsetFixed<8> bs;

    CHECK_EQ(bs.count(), 0);
    CHECK(bs.none());
    CHECK(!bs.any());

    bs.set(1).set(3).set(5);
    CHECK_EQ(bs.count(), 3);
    CHECK(bs.any());
    CHECK(!bs.none());
}

TEST_CASE("fl::BitsetFixed<8> - bitwise operators") {
    BitsetFixed<8> bs1;
    BitsetFixed<8> bs2;

    bs1.set(0).set(2).set(4);
    bs2.set(1).set(2).set(3);

    SUBCASE("AND") {
        auto result = bs1 & bs2;
        CHECK(result.test(2));
        CHECK(!result.test(0));
        CHECK(!result.test(1));
        CHECK_EQ(result.count(), 1);
    }

    SUBCASE("OR") {
        auto result = bs1 | bs2;
        CHECK(result.test(0));
        CHECK(result.test(1));
        CHECK(result.test(2));
        CHECK(result.test(3));
        CHECK(result.test(4));
        CHECK_EQ(result.count(), 5);
    }

    SUBCASE("XOR") {
        auto result = bs1 ^ bs2;
        CHECK(result.test(0));
        CHECK(result.test(1));
        CHECK(!result.test(2));
        CHECK(result.test(3));
        CHECK(result.test(4));
        CHECK_EQ(result.count(), 4);
    }

    SUBCASE("NOT") {
        auto result = ~bs1;
        CHECK(!result.test(0));
        CHECK(result.test(1));
        CHECK(!result.test(2));
        CHECK_EQ(result.count(), 5);
    }
}

TEST_CASE("fl::BitsetFixed<16> - larger size") {
    BitsetFixed<16> bs;
    CHECK_EQ(bs.size(), 16);

    for (u32 i = 0; i < 16; ++i) {
        bs.set(i);
    }
    CHECK_EQ(bs.count(), 16);
    CHECK(bs.any());
}

TEST_CASE("fl::BitsetFixed<32> - cross-block operations") {
    BitsetFixed<32> bs;

    bs.set(0);   // First block
    bs.set(15);  // End of first block
    bs.set(16);  // Second block
    bs.set(31);  // End of second block

    CHECK(bs.test(0));
    CHECK(bs.test(15));
    CHECK(bs.test(16));
    CHECK(bs.test(31));
    CHECK_EQ(bs.count(), 4);
}

TEST_CASE("fl::BitsetFixed<8> - assign") {
    BitsetFixed<8> bs;

    bs.assign(5, true);
    CHECK(bs.test(0));
    CHECK(bs.test(4));
    CHECK(!bs.test(5));
    CHECK_EQ(bs.count(), 5);

    bs.assign(3, false);
    CHECK(!bs.test(0));
    CHECK(!bs.test(2));
    CHECK(bs.test(4));  // Beyond assigned range
}

TEST_CASE("fl::BitsetFixed<16> - find_first") {
    BitsetFixed<16> bs;

    bs.set(5);
    CHECK_EQ(bs.find_first(true), 5);

    bs.set(3);
    CHECK_EQ(bs.find_first(true), 3);

    CHECK_EQ(bs.find_first(true, 4), 5);
    CHECK_EQ(bs.find_first(true, 6), -1);
}

TEST_CASE("fl::BitsetFixed<16> - find_run") {
    BitsetFixed<16> bs;

    bs.set(3).set(4).set(5).set(6);
    CHECK_EQ(bs.find_run(true, 3), 3);
    CHECK_EQ(bs.find_run(true, 4), 3);
    CHECK_EQ(bs.find_run(true, 5), -1);
}

// Test BitsetInlined class
TEST_CASE("fl::BitsetInlined<16> - basic operations") {
    BitsetInlined<16> bs;
    CHECK_EQ(bs.size(), 16);
    CHECK(bs.none());

    bs.set(5);
    CHECK(bs.test(5));
    CHECK_EQ(bs.count(), 1);
}

TEST_CASE("fl::BitsetInlined<16> - dynamic growth") {
    BitsetInlined<16> bs;

    bs.set(20);  // Forces dynamic allocation
    CHECK(bs.test(20));
    CHECK(bs.size() > 16);
}

TEST_CASE("fl::BitsetInlined<16> - preserve on growth") {
    BitsetInlined<16> bs;
    bs.set(5).set(10);
    bs.set(25);  // Triggers growth
    CHECK(bs.test(5));
    CHECK(bs.test(10));
    CHECK(bs.test(25));
    CHECK_EQ(bs.count(), 3);
}

TEST_CASE("fl::BitsetInlined<16> - resize") {
    BitsetInlined<16> bs;

    bs.set(5);
    bs.resize(32);
    CHECK(bs.test(5));
    CHECK(bs.size() >= 32);
}

TEST_CASE("fl::BitsetInlined<16> - copy and move") {
    BitsetInlined<16> bs;
    bs.set(5).set(10);

    BitsetInlined<16> bs2(bs);
    CHECK(bs2.test(5));
    CHECK(bs2.test(10));
    CHECK_EQ(bs2.count(), 2);
}

TEST_CASE("fl::BitsetInlined<16> - bitwise operators") {
    BitsetInlined<16> bs1;
    BitsetInlined<16> bs2;

    bs1.set(2).set(5).set(8);
    bs2.set(5).set(8).set(11);

    auto result_and = bs1 & bs2;
    CHECK(result_and.test(5));
    CHECK(result_and.test(8));
    CHECK_EQ(result_and.count(), 2);

    auto result_or = bs1 | bs2;
    CHECK(result_or.test(2));
    CHECK(result_or.test(5));
    CHECK(result_or.test(11));
    CHECK_EQ(result_or.count(), 4);
}

TEST_CASE("fl::BitsetInlined<16> - find_first") {
    BitsetInlined<16> bs;

    bs.set(5).set(10);
    CHECK_EQ(bs.find_first(true), 5);
    CHECK_EQ(bs.find_first(true, 6), 10);
}

TEST_CASE("fl::bitset - type alias") {
    bitset<> bs;
    CHECK_EQ(bs.size(), 16);

    bitset<32> bs32;
    CHECK_EQ(bs32.size(), 32);
}

TEST_CASE("fl::bitset_fixed - type alias") {
    bitset_fixed<8> bs;
    CHECK_EQ(bs.size(), 8);

    bitset_fixed<64> bs64;
    CHECK_EQ(bs64.size(), 64);
}
