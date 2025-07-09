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
    // For bitset_fixed<8>, we expect 1 uint16_t block (2 bytes)
    REQUIRE_EQ(sizeof(bitset_fixed<8>), 2);
    
    // For bitset_fixed<16>, we expect 1 uint16_t block (2 bytes)
    REQUIRE_EQ(sizeof(bitset_fixed<16>), 2);
    
    // For bitset_fixed<17>, we expect 2 uint16_t blocks (4 bytes)
    REQUIRE_EQ(sizeof(bitset_fixed<17>), 4);
    
    // For bitset_fixed<32>, we expect 2 uint16_t blocks (4 bytes)
    REQUIRE_EQ(sizeof(bitset_fixed<32>), 4);
    
    // For bitset_fixed<33>, we expect 3 uint16_t blocks (6 bytes)
    REQUIRE_EQ(sizeof(bitset_fixed<33>), 6);
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


TEST_CASE("test bitset_fixed find_first") {
    // Test find_first for true bits
    bitset_fixed<64> bs;
    
    // Initially no bits are set, so find_first(true) should return -1
    REQUIRE_EQ(bs.find_first(true), -1);
    
    // find_first(false) should return 0 (first unset bit)
    REQUIRE_EQ(bs.find_first(false), 0);
    
    // Set bit at position 5
    bs.set(5);
    REQUIRE_EQ(bs.find_first(true), 5);
    REQUIRE_EQ(bs.find_first(false), 0);
    
    // Set bit at position 0
    bs.set(0);
    REQUIRE_EQ(bs.find_first(true), 0);
    REQUIRE_EQ(bs.find_first(false), 1);
    
    // Set bit at position 63 (last bit)
    bs.set(63);
    REQUIRE_EQ(bs.find_first(true), 0);
    REQUIRE_EQ(bs.find_first(false), 1);
    
    // Clear bit 0, now first set bit should be 5
    bs.reset(0);
    REQUIRE_EQ(bs.find_first(true), 5);
    REQUIRE_EQ(bs.find_first(false), 0);
    
    // Test with larger bitset
    bitset_fixed<128> bs2;
    bs2.set(100);
    REQUIRE_EQ(bs2.find_first(true), 100);
    REQUIRE_EQ(bs2.find_first(false), 0);
    
    // Test edge case: all bits set
    bitset_fixed<8> bs3;
    for (fl::u32 i = 0; i < 8; ++i) {
        bs3.set(i);
    }
    REQUIRE_EQ(bs3.find_first(true), 0);
    REQUIRE_EQ(bs3.find_first(false), -1);
    
    // Test edge case: no bits set
    bitset_fixed<8> bs4;
    REQUIRE_EQ(bs4.find_first(true), -1);
    REQUIRE_EQ(bs4.find_first(false), 0);
}

TEST_CASE("test bitset_dynamic find_first") {
    // Test find_first for dynamic bitset
    bitset_dynamic bs(64);
    
    // Initially no bits are set, so find_first(true) should return -1
    REQUIRE_EQ(bs.find_first(true), -1);
    
    // find_first(false) should return 0 (first unset bit)
    REQUIRE_EQ(bs.find_first(false), 0);
    
    // Set bit at position 5
    bs.set(5);
    REQUIRE_EQ(bs.find_first(true), 5);
    REQUIRE_EQ(bs.find_first(false), 0);
    
    // Set bit at position 0
    bs.set(0);
    REQUIRE_EQ(bs.find_first(true), 0);
    REQUIRE_EQ(bs.find_first(false), 1);
    
    // Set bit at position 63 (last bit)
    bs.set(63);
    REQUIRE_EQ(bs.find_first(true), 0);
    REQUIRE_EQ(bs.find_first(false), 1);
    
    // Clear bit 0, now first set bit should be 5
    bs.reset(0);
    REQUIRE_EQ(bs.find_first(true), 5);
    REQUIRE_EQ(bs.find_first(false), 0);
    
    // Test with all bits set
    bitset_dynamic bs2(16);
    for (fl::u32 i = 0; i < 16; ++i) {
        bs2.set(i);
    }
    REQUIRE_EQ(bs2.find_first(true), 0);
    REQUIRE_EQ(bs2.find_first(false), -1);
    
    // Test with no bits set
    bitset_dynamic bs3(16);
    REQUIRE_EQ(bs3.find_first(true), -1);
    REQUIRE_EQ(bs3.find_first(false), 0);
}

TEST_CASE("test bitset_inlined find_first") {
    // Test find_first for inlined bitset (uses fixed bitset internally for small sizes)
    bitset<64> bs;
    
    // Initially no bits are set, so find_first(true) should return -1
    REQUIRE_EQ(bs.find_first(true), -1);
    
    // find_first(false) should return 0 (first unset bit)
    REQUIRE_EQ(bs.find_first(false), 0);
    
    // Set bit at position 5
    bs.set(5);
    REQUIRE_EQ(bs.find_first(true), 5);
    REQUIRE_EQ(bs.find_first(false), 0);
    
    // Set bit at position 0
    bs.set(0);
    REQUIRE_EQ(bs.find_first(true), 0);
    REQUIRE_EQ(bs.find_first(false), 1);
    
    // Set bit at position 63 (last bit)
    bs.set(63);
    REQUIRE_EQ(bs.find_first(true), 0);
    REQUIRE_EQ(bs.find_first(false), 1);
    
    // Clear bit 0, now first set bit should be 5
    bs.reset(0);
    REQUIRE_EQ(bs.find_first(true), 5);
    REQUIRE_EQ(bs.find_first(false), 0);
    
    // Test with all bits set
    bitset<16> bs2;
    for (fl::u32 i = 0; i < 16; ++i) {
        bs2.set(i);
    }
    REQUIRE_EQ(bs2.find_first(true), 0);
    REQUIRE_EQ(bs2.find_first(false), -1);
    
    // Test with no bits set
    bitset<16> bs3;
    REQUIRE_EQ(bs3.find_first(true), -1);
    REQUIRE_EQ(bs3.find_first(false), 0);
    
    // Test with larger size that uses dynamic bitset internally
    bitset<300> bs4;
    bs4.set(150);
    REQUIRE_EQ(bs4.find_first(true), 150);
    REQUIRE_EQ(bs4.find_first(false), 0);
}


TEST_CASE("test bitset_fixed find_run") {
    // Test interesting patterns
    bitset_fixed<32> bs;
    // Set pattern: 0001 1001 0111 1100 0000 1111 0000 0011
    bs.set(3);
    bs.set(4);
    bs.set(7);
    bs.set(9);
    bs.set(10);
    bs.set(11);
    bs.set(12);
    bs.set(13);
    bs.set(20);
    bs.set(21);
    bs.set(22);
    bs.set(23);
    bs.set(30);
    bs.set(31);

    FL_WARN("bs: " << bs);

    // Find first run of length 3
    int idx = bs.find_run(true, 3);
    REQUIRE_EQ(idx, 9);  // First run at 3

    idx = bs.find_run(false, 2, 9);
    REQUIRE_EQ(idx, 14);  // First run at 3

    // off the edge
    idx = bs.find_run(true, 3, 31);
    REQUIRE_EQ(idx, -1);

}

TEST_CASE("test bitset_fixed bitstring constructor") {
    // Test basic bitstring construction
    bitset_fixed<8> bs1("10101010");
    REQUIRE_EQ(bs1.size(), 8);
    REQUIRE_EQ(bs1.count(), 4);
    REQUIRE_EQ(bs1.test(0), true);
    REQUIRE_EQ(bs1.test(1), false);
    REQUIRE_EQ(bs1.test(2), true);
    REQUIRE_EQ(bs1.test(3), false);
    REQUIRE_EQ(bs1.test(4), true);
    REQUIRE_EQ(bs1.test(5), false);
    REQUIRE_EQ(bs1.test(6), true);
    REQUIRE_EQ(bs1.test(7), false);
    
    // Test with shorter string than bitset size
    bitset_fixed<16> bs2("1100");
    REQUIRE_EQ(bs2.size(), 16);
    REQUIRE_EQ(bs2.count(), 2);
    REQUIRE_EQ(bs2.test(0), true);
    REQUIRE_EQ(bs2.test(1), true);
    REQUIRE_EQ(bs2.test(2), false);
    REQUIRE_EQ(bs2.test(3), false);
    // Remaining bits should be false
    for (fl::u32 i = 4; i < 16; ++i) {
        REQUIRE_EQ(bs2.test(i), false);
    }
    
    // Test with longer string than bitset size (should truncate)
    bitset_fixed<4> bs3("11110000");
    REQUIRE_EQ(bs3.size(), 4);
    REQUIRE_EQ(bs3.count(), 4);
    REQUIRE_EQ(bs3.test(0), true);
    REQUIRE_EQ(bs3.test(1), true);
    REQUIRE_EQ(bs3.test(2), true);
    REQUIRE_EQ(bs3.test(3), true);
    
    // Test with null pointer (should not crash)
    bitset_fixed<8> bs4(nullptr);
    REQUIRE_EQ(bs4.size(), 8);
    REQUIRE_EQ(bs4.count(), 0);
    REQUIRE_EQ(bs4.none(), true);
    
    // Test with empty string
    bitset_fixed<8> bs5("");
    REQUIRE_EQ(bs5.size(), 8);
    REQUIRE_EQ(bs5.count(), 0);
    REQUIRE_EQ(bs5.none(), true);
    
    // Test with mixed characters (should ignore non-0/1 chars)
    bitset_fixed<8> bs6("1a0b1c0d");
    REQUIRE_EQ(bs6.size(), 8);
    REQUIRE_EQ(bs6.count(), 1);
    REQUIRE_EQ(bs6.test(0), true);
    REQUIRE_EQ(bs6.test(1), false);
    REQUIRE_EQ(bs6.test(2), false);
    REQUIRE_EQ(bs6.test(3), false);
    REQUIRE_EQ(bs6.test(4), false);
    REQUIRE_EQ(bs6.test(5), false);
    REQUIRE_EQ(bs6.test(6), false);
    REQUIRE_EQ(bs6.test(7), false);
}

TEST_CASE("test bitset_dynamic bitstring constructor") {
    // Test basic bitstring construction
    bitset_dynamic bs1("10101010");
    REQUIRE_EQ(bs1.size(), 8);
    REQUIRE_EQ(bs1.count(), 4);
    REQUIRE_EQ(bs1.test(0), true);
    REQUIRE_EQ(bs1.test(1), false);
    REQUIRE_EQ(bs1.test(2), true);
    REQUIRE_EQ(bs1.test(3), false);
    REQUIRE_EQ(bs1.test(4), true);
    REQUIRE_EQ(bs1.test(5), false);
    REQUIRE_EQ(bs1.test(6), true);
    REQUIRE_EQ(bs1.test(7), false);
    
    // Test with longer string
    bitset_dynamic bs2("1111000011110000");
    REQUIRE_EQ(bs2.size(), 16);
    REQUIRE_EQ(bs2.count(), 8);
    for (fl::u32 i = 0; i < 4; ++i) {
        REQUIRE_EQ(bs2.test(i), true);
    }
    for (fl::u32 i = 4; i < 8; ++i) {
        REQUIRE_EQ(bs2.test(i), false);
    }
    for (fl::u32 i = 8; i < 12; ++i) {
        REQUIRE_EQ(bs2.test(i), true);
    }
    for (fl::u32 i = 12; i < 16; ++i) {
        REQUIRE_EQ(bs2.test(i), false);
    }
    
    // Test with very long string (should handle multiple blocks)
    bitset_dynamic bs3("1111111111111111111111111111111111111111111111111111111111111111");
    REQUIRE_EQ(bs3.size(), 64);
    REQUIRE_EQ(bs3.count(), 64);
    REQUIRE_EQ(bs3.all(), true);
    
    // Test with null pointer (should not crash)
    bitset_dynamic bs4(nullptr);
    REQUIRE_EQ(bs4.size(), 0);
    REQUIRE_EQ(bs4.count(), 0);
    
    // Test with empty string
    bitset_dynamic bs5("");
    REQUIRE_EQ(bs5.size(), 0);
    REQUIRE_EQ(bs5.count(), 0);
    
    // Test with mixed characters (should ignore non-0/1 chars)
    bitset_dynamic bs6("1a0b1c0d");
    REQUIRE_EQ(bs6.size(), 8);
    REQUIRE_EQ(bs6.count(), 2);
    REQUIRE_EQ(bs6.test(0), true);
    REQUIRE_EQ(bs6.test(1), false);
    REQUIRE_EQ(bs6.test(2), true);
    REQUIRE_EQ(bs6.test(3), false);
    REQUIRE_EQ(bs6.test(4), false);
    REQUIRE_EQ(bs6.test(5), false);
    REQUIRE_EQ(bs6.test(6), false);
    REQUIRE_EQ(bs6.test(7), false);
}

TEST_CASE("test bitset_inlined bitstring constructor") {
    // Test with small size (uses fixed bitset internally)
    bitset<8> bs1("10101010");
    REQUIRE_EQ(bs1.size(), 8);
    REQUIRE_EQ(bs1.count(), 4);
    REQUIRE_EQ(bs1.test(0), true);
    REQUIRE_EQ(bs1.test(1), false);
    REQUIRE_EQ(bs1.test(2), true);
    REQUIRE_EQ(bs1.test(3), false);
    REQUIRE_EQ(bs1.test(4), true);
    REQUIRE_EQ(bs1.test(5), false);
    REQUIRE_EQ(bs1.test(6), true);
    REQUIRE_EQ(bs1.test(7), false);
    
    // Test with larger size (uses dynamic bitset internally)
    bitset<100> bs2("1111000011110000");
    REQUIRE_EQ(bs2.size(), 100);
    REQUIRE_EQ(bs2.count(), 8);
    for (fl::u32 i = 0; i < 4; ++i) {
        REQUIRE_EQ(bs2.test(i), true);
    }
    for (fl::u32 i = 4; i < 8; ++i) {
        REQUIRE_EQ(bs2.test(i), false);
    }
    for (fl::u32 i = 8; i < 12; ++i) {
        REQUIRE_EQ(bs2.test(i), true);
    }
    for (fl::u32 i = 12; i < 16; ++i) {
        REQUIRE_EQ(bs2.test(i), false);
    }
    // Remaining bits should be false
    for (fl::u32 i = 16; i < 100; ++i) {
        REQUIRE_EQ(bs2.test(i), false);
    }
    
    // Test with very long string
    bitset<200> bs3("1111111111111111111111111111111111111111111111111111111111111111");
    REQUIRE_EQ(bs3.size(), 200);
    REQUIRE_EQ(bs3.count(), 64);
    for (fl::u32 i = 0; i < 64; ++i) {
        REQUIRE_EQ(bs3.test(i), true);
    }
    for (fl::u32 i = 64; i < 200; ++i) {
        REQUIRE_EQ(bs3.test(i), false);
    }
    
    // Test with null pointer (should not crash)
    bitset<16> bs4(nullptr);
    REQUIRE_EQ(bs4.size(), 16);
    REQUIRE_EQ(bs4.count(), 0);
    REQUIRE_EQ(bs4.none(), true);
    
    // Test with empty string
    bitset<16> bs5("");
    REQUIRE_EQ(bs5.size(), 16);
    REQUIRE_EQ(bs5.count(), 0);
    REQUIRE_EQ(bs5.none(), true);
    
    // Test with mixed characters (should ignore non-0/1 chars)
    bitset<8> bs6("1a0b1c0d");
    REQUIRE_EQ(bs6.size(), 8);
    REQUIRE_EQ(bs6.count(), 1);
    REQUIRE_EQ(bs6.test(0), true);
    REQUIRE_EQ(bs6.test(1), false);
    REQUIRE_EQ(bs6.test(2), false);
    REQUIRE_EQ(bs6.test(3), false);
    REQUIRE_EQ(bs6.test(4), false);
    REQUIRE_EQ(bs6.test(5), false);
    REQUIRE_EQ(bs6.test(6), false);
    REQUIRE_EQ(bs6.test(7), false);
}

TEST_CASE("test bitstring serialization roundtrip") {
    // Test that we can construct from a bitstring and serialize back to the same string
    
    // Test bitset_fixed
    {
        bitset_fixed<8> bs("10101010");
        fl::string result;
        bs.to_string(&result);
        REQUIRE_EQ(result, "10101010");
    }
    
    {
        bitset_fixed<4> bs("1100");
        fl::string result;
        bs.to_string(&result);
        REQUIRE_EQ(result, "1100");
    }
    
    // Test bitset_dynamic
    {
        bitset_dynamic bs("10101010");
        fl::string result;
        bs.to_string(&result);
        REQUIRE_EQ(result, "10101010");
    }
    
    {
        bitset_dynamic bs("1100");
        fl::string result;
        bs.to_string(&result);
        REQUIRE_EQ(result, "1100");
    }
    
    {
        bitset_dynamic bs("1010101010101010");
        fl::string result;
        bs.to_string(&result);
        REQUIRE_EQ(result, "1010101010101010");
    }
    
    // Test bitset_inlined
    {
        bitset<8> bs("10101010");
        fl::string result;
        bs.to_string(&result);
        REQUIRE_EQ(result, "10101010");
    }
    
    {
        bitset<16> bs("1100");
        fl::string result;
        bs.to_string(&result);
        REQUIRE_EQ(result, "1100000000000000");
    }
    
    {
        bitset<8> bs("1010101010101010");  // Should use dynamic
        fl::string result;
        bs.to_string(&result);
        REQUIRE_EQ(result, "1010101010101010");
    }
    
    // Test edge cases
    {
        bitset_fixed<8> bs("");
        fl::string result;
        bs.to_string(&result);
        REQUIRE_EQ(result, "00000000");
    }
    
    {
        bitset_dynamic bs("");
        fl::string result;
        bs.to_string(&result);
        REQUIRE_EQ(result, "");
    }
    
    {
        bitset<8> bs("");
        fl::string result;
        bs.to_string(&result);
        REQUIRE_EQ(result, "00000000");
    }
}
