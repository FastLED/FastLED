// g++ --std=c++11 test.cpp

#include "test.h"
#include "fl/bitset.h"


using namespace fl;

TEST_CASE("test bitset") {
    // default‐constructed bitset is empty
    bitset<10> bs;
    REQUIRE_EQ(bs.none(), true);
    REQUIRE_EQ(bs.count(), 0);
    REQUIRE_EQ(bs.size(), 10);

    // set a bit
    bs.set(3);
    REQUIRE_EQ(bs.test(3), true);
    REQUIRE_EQ(bs[3], true);
    REQUIRE_EQ(bs.any(), true);
    REQUIRE_EQ(bs.count(), 1);

    // reset that bit
    bs.reset(3);
    REQUIRE_EQ(bs.test(3), false);
    REQUIRE_EQ(bs.none(), true);

    // toggle a bit
    bs.flip(2);
    REQUIRE_EQ(bs.test(2), true);
    bs.flip(2);
    REQUIRE_EQ(bs.test(2), false);

    // flip all bits
    bitset<5> bs2;
    for (size_t i = 0; i < 5; ++i)
        bs2.set(i, (i % 2) == 0);
    auto bs2_flipped = ~bs2;
    for (size_t i = 0; i < 5; ++i)
        REQUIRE_EQ(bs2_flipped.test(i), !bs2.test(i));

    // all() and count()
    bitset<4> bs3;
    for (size_t i = 0; i < 4; ++i)
        bs3.set(i);
    REQUIRE_EQ(bs3.all(), true);
    REQUIRE_EQ(bs3.count(), 4);

    // out‐of‐range ops are no‐ops
    bs3.set(100);
    REQUIRE_EQ(bs3.count(), 4);

    // bitwise AND, OR, XOR
    bitset<4> a, b;
    a.set(0); a.set(2);
    b.set(1); b.set(2);

    auto or_ab  = a | b;
    REQUIRE_EQ(or_ab.test(0), true);
    REQUIRE_EQ(or_ab.test(1), true);
    REQUIRE_EQ(or_ab.test(2), true);
    REQUIRE_EQ(or_ab.test(3), false);

    auto and_ab = a & b;
    REQUIRE_EQ(and_ab.test(2), true);
    REQUIRE_EQ(and_ab.test(0), false);

    auto xor_ab  = a ^ b;
    REQUIRE_EQ(xor_ab.test(0), true);
    REQUIRE_EQ(xor_ab.test(1), true);
    REQUIRE_EQ(xor_ab.test(2), false);

    // reset and none()
    a.reset(); b.reset();
    REQUIRE_EQ(a.none(), true);
    
    // Test expected size of bitset
    REQUIRE_EQ(bitset<8>().size(), 8);
    REQUIRE_EQ(bitset<16>().size(), 16);
    REQUIRE_EQ(bitset<32>().size(), 32);
    REQUIRE_EQ(bitset<64>().size(), 64);
    REQUIRE_EQ(bitset<100>().size(), 100);
    REQUIRE_EQ(bitset<1000>().size(), 1000);
    
    // Test memory size of bitset class (sizeof)
    // For bitset<8>, we expect 1 uint64_t block (8 bytes)
    REQUIRE_EQ(sizeof(bitset<8>), 8);
    
    // For bitset<64>, we expect 1 uint64_t block (8 bytes)
    REQUIRE_EQ(sizeof(bitset<64>), 8);
    
    // For bitset<65>, we expect 2 uint64_t blocks (16 bytes)
    REQUIRE_EQ(sizeof(bitset<65>), 16);
    
    // For bitset<128>, we expect 2 uint64_t blocks (16 bytes)
    REQUIRE_EQ(sizeof(bitset<128>), 16);
    
    // For bitset<129>, we expect 3 uint64_t blocks (24 bytes)
    REQUIRE_EQ(sizeof(bitset<129>), 24);
}
