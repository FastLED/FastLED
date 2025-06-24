// g++ --std=c++11 test.cpp

#include "test.h"
#include "fl/bitset.h"
#include "fl/bitset_dynamic.h"


using namespace fl;

TEST_CASE("test bitset") {
    // default‐constructed bitset is empty
    bitset_fixed<10> bs;
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
    bitset_fixed<5> bs2;
    for (size_t i = 0; i < 5; ++i)
        bs2.set(i, (i % 2) == 0);
    auto bs2_flipped = ~bs2;
    for (size_t i = 0; i < 5; ++i)
        REQUIRE_EQ(bs2_flipped.test(i), !bs2.test(i));

    // all() and count()
    bitset_fixed<4> bs3;
    for (size_t i = 0; i < 4; ++i)
        bs3.set(i);
    REQUIRE_EQ(bs3.all(), true);
    REQUIRE_EQ(bs3.count(), 4);

    // check that the count can auto expand
    bs3.set(100);
    REQUIRE_EQ(bs3.count(), 4);

    // bitwise AND, OR, XOR
    bitset_fixed<4> a, b;
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
    
    // Test expected size of bitset_fixed
    REQUIRE_EQ(bitset_fixed<8>().size(), 8);
    REQUIRE_EQ(bitset_fixed<16>().size(), 16);
    REQUIRE_EQ(bitset_fixed<32>().size(), 32);
    REQUIRE_EQ(bitset_fixed<64>().size(), 64);
    REQUIRE_EQ(bitset_fixed<100>().size(), 100);
    REQUIRE_EQ(bitset_fixed<1000>().size(), 1000);
    
    // Test memory size of bitset_fixed class (sizeof)
    // For bitset_fixed<8>, we expect 1 uint64_t block (8 bytes)
    REQUIRE_EQ(sizeof(bitset_fixed<8>), 8);
    
    // For bitset_fixed<64>, we expect 1 uint64_t block (8 bytes)
    REQUIRE_EQ(sizeof(bitset_fixed<64>), 8);
    
    // For bitset_fixed<65>, we expect 2 uint64_t blocks (16 bytes)
    REQUIRE_EQ(sizeof(bitset_fixed<65>), 16);
    
    // For bitset_fixed<128>, we expect 2 uint64_t blocks (16 bytes)
    REQUIRE_EQ(sizeof(bitset_fixed<128>), 16);
    
    // For bitset_fixed<129>, we expect 3 uint64_t blocks (24 bytes)
    REQUIRE_EQ(sizeof(bitset_fixed<129>), 24);
}


TEST_CASE("compare fixed and dynamic bitsets") {
    // Test that fixed and dynamic bitsets behave the same
    bitset_fixed<10> fixed_bs;
    fl::bitset_dynamic dynamic_bs(10);
    
    // Set the same bits in both
    fixed_bs.set(1);
    fixed_bs.set(5);
    fixed_bs.set(9);
    
    dynamic_bs.set(1);
    dynamic_bs.set(5);
    dynamic_bs.set(9);
    
    // Verify they have the same state
    REQUIRE_EQ(fixed_bs.size(), dynamic_bs.size());
    REQUIRE_EQ(fixed_bs.count(), dynamic_bs.count());
    
    for (size_t i = 0; i < 10; ++i) {
        REQUIRE_EQ(fixed_bs.test(i), dynamic_bs.test(i));
    }
}



TEST_CASE("test bitset_dynamic") {
    // default-constructed bitset is empty
    bitset_dynamic bs;
    REQUIRE_EQ(bs.size(), 0);
    REQUIRE_EQ(bs.none(), true);
    REQUIRE_EQ(bs.count(), 0);
    
    // resize and test
    bs.resize(10);
    REQUIRE_EQ(bs.size(), 10);
    REQUIRE_EQ(bs.none(), true);
    
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
    
    // resize larger
    bs.set(5);
    bs.resize(20);
    REQUIRE_EQ(bs.size(), 20);
    REQUIRE_EQ(bs.test(5), true);
    REQUIRE_EQ(bs.count(), 1);
    
    // resize smaller (truncate)
    bs.resize(4);
    REQUIRE_EQ(bs.size(), 4);
    REQUIRE_EQ(bs.test(5), false); // out of range now
    REQUIRE_EQ(bs.count(), 0);
    
    // test with larger sizes that span multiple blocks
    bitset_dynamic large_bs(100);
    large_bs.set(0);
    large_bs.set(63);
    large_bs.set(64);
    large_bs.set(99);
    REQUIRE_EQ(large_bs.count(), 4);
    REQUIRE_EQ(large_bs.test(0), true);
    REQUIRE_EQ(large_bs.test(63), true);
    REQUIRE_EQ(large_bs.test(64), true);
    REQUIRE_EQ(large_bs.test(99), true);
    
    // flip all bits
    bitset_dynamic bs2(5);
    for (size_t i = 0; i < 5; ++i)
        bs2.set(i, (i % 2) == 0);
    
    bs2.flip();
    for (size_t i = 0; i < 5; ++i)
        REQUIRE_EQ(bs2.test(i), !((i % 2) == 0));
    
    // all() and count()
    bitset_dynamic bs3(4);
    for (size_t i = 0; i < 4; ++i)
        bs3.set(i);
    REQUIRE_EQ(bs3.all(), true);
    REQUIRE_EQ(bs3.count(), 4);
    
    // out-of-range ops are no-ops
    bs3.set(100);
    REQUIRE_EQ(bs3.count(), 4);
    
    // bitwise AND, OR, XOR
    bitset_dynamic a(4), b(4);
    a.set(0); a.set(2);
    b.set(1); b.set(2);
    
    auto or_ab = a | b;
    REQUIRE_EQ(or_ab.test(0), true);
    REQUIRE_EQ(or_ab.test(1), true);
    REQUIRE_EQ(or_ab.test(2), true);
    REQUIRE_EQ(or_ab.test(3), false);
    
    auto and_ab = a & b;
    REQUIRE_EQ(and_ab.test(2), true);
    REQUIRE_EQ(and_ab.test(0), false);
    
    auto xor_ab = a ^ b;
    REQUIRE_EQ(xor_ab.test(0), true);
    REQUIRE_EQ(xor_ab.test(1), true);
    REQUIRE_EQ(xor_ab.test(2), false);
    
    // reset and none()
    a.reset(); b.reset();
    REQUIRE_EQ(a.none(), true);
    REQUIRE_EQ(b.none(), true);
    
    // copy constructor
    bitset_dynamic original(10);
    original.set(3);
    original.set(7);
    
    bitset_dynamic copy(original);
    REQUIRE_EQ(copy.size(), 10);
    REQUIRE_EQ(copy.test(3), true);
    REQUIRE_EQ(copy.test(7), true);
    REQUIRE_EQ(copy.count(), 2);
    
    // move constructor
    bitset_dynamic moved(fl::move(copy));
    REQUIRE_EQ(moved.size(), 10);
    REQUIRE_EQ(moved.test(3), true);
    REQUIRE_EQ(moved.test(7), true);
    REQUIRE_EQ(moved.count(), 2);
    REQUIRE_EQ(copy.size(), 0); // moved from should be empty
    
    // assignment operator
    bitset_dynamic assigned = original;
    REQUIRE_EQ(assigned.size(), 10);
    REQUIRE_EQ(assigned.test(3), true);
    REQUIRE_EQ(assigned.test(7), true);
    
    // clear
    assigned.clear();
    REQUIRE_EQ(assigned.size(), 0);
    REQUIRE_EQ(assigned.none(), true);
    
    // Test memory size changes with resize
    bitset_dynamic small_bs(8);
    bitset_dynamic medium_bs(65);
    bitset_dynamic large_bs2(129);
    
    // These sizes should match the fixed bitset tests
    REQUIRE_EQ(small_bs.size(), 8);
    REQUIRE_EQ(medium_bs.size(), 65);
    REQUIRE_EQ(large_bs2.size(), 129);
}


