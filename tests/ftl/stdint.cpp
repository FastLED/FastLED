#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "doctest.h"
#include "stdint.h" // ok include - testing FL types against standard

// Test that fl/stl/stdint.h provides standard integer types without including <stdint.h>
// This header is critical for FastLED's fast compilation strategy
// ok INT_MAX (this file tests that the numeric limit macros are properly defined)

TEST_CASE("stdint type definitions") {
    SUBCASE("uint8_t and int8_t") {
        // Verify 8-bit types exist and have correct size
        uint8_t u8 = 255;
        int8_t i8 = -128;

        CHECK_EQ(sizeof(uint8_t), 1);
        CHECK_EQ(sizeof(int8_t), 1);
        CHECK_EQ(u8, 255);
        CHECK_EQ(i8, -128);

        // Test wraparound behavior
        u8 = 255;
        u8++;
        CHECK_EQ(u8, 0);

        // Note: signed overflow is UB, so we test wraparound via unsigned arithmetic
        uint8_t u8_temp = 127;
        u8_temp++;
        i8 = static_cast<int8_t>(u8_temp);
        CHECK_EQ(i8, -128);
    }

    SUBCASE("uint16_t and int16_t") {
        // Verify 16-bit types exist and have correct size
        uint16_t u16 = 65535;
        int16_t i16 = -32768;

        CHECK_EQ(sizeof(uint16_t), 2);
        CHECK_EQ(sizeof(int16_t), 2);
        CHECK_EQ(u16, 65535);
        CHECK_EQ(i16, -32768);

        // Test wraparound behavior
        u16 = 65535;
        u16++;
        CHECK_EQ(u16, 0);

        // Note: signed overflow is UB, so we test wraparound via unsigned arithmetic
        uint16_t u16_temp = 32767;
        u16_temp++;
        i16 = static_cast<int16_t>(u16_temp);
        CHECK_EQ(i16, -32768);
    }

    SUBCASE("uint32_t and int32_t") {
        // Verify 32-bit types exist and have correct size
        uint32_t u32 = 4294967295U;
        int32_t i32 = -2147483647 - 1;

        CHECK_EQ(sizeof(uint32_t), 4);
        CHECK_EQ(sizeof(int32_t), 4);
        CHECK_EQ(u32, 4294967295U);
        CHECK_EQ(i32, -2147483648);

        // Test wraparound behavior
        u32 = 4294967295U;
        u32++;
        CHECK_EQ(u32, 0U);

        // Note: signed overflow is UB, so we test wraparound via unsigned arithmetic
        // then convert to signed
        uint32_t u32_temp = 2147483647;
        u32_temp++;
        i32 = static_cast<int32_t>(u32_temp);
        CHECK_EQ(i32, -2147483648);
    }

    SUBCASE("uint64_t and int64_t") {
        // Verify 64-bit types exist and have correct size
        uint64_t u64 = 18446744073709551615ULL;
        int64_t i64 = -9223372036854775807LL - 1;

        CHECK_EQ(sizeof(uint64_t), 8);
        CHECK_EQ(sizeof(int64_t), 8);
        CHECK_EQ(u64, 18446744073709551615ULL);
        CHECK_EQ(i64, -9223372036854775807LL - 1);

        // Test large values
        u64 = 0xFFFFFFFFFFFFFFFFULL;
        CHECK_EQ(u64, 18446744073709551615ULL);

        i64 = 0x7FFFFFFFFFFFFFFFLL;
        CHECK_EQ(i64, 9223372036854775807LL);
    }

    SUBCASE("size_t") {
        // size_t should be an unsigned type large enough for array indexing
        size_t sz = 100;
        CHECK(sz > 0);
        CHECK_EQ(sizeof(size_t), sizeof(void*));

        // Test that size_t can hold pointer values
        int dummy;
        size_t ptr_as_size = reinterpret_cast<size_t>(&dummy);
        CHECK(ptr_as_size != 0);
    }

    SUBCASE("uintptr_t and intptr_t") {
        // Pointer-sized integer types
        int dummy;
        uintptr_t uptr = reinterpret_cast<uintptr_t>(&dummy);
        intptr_t iptr = reinterpret_cast<intptr_t>(&dummy);

        CHECK_EQ(sizeof(uintptr_t), sizeof(void*));
        CHECK_EQ(sizeof(intptr_t), sizeof(void*));

        // Verify pointer round-trip
        int* recovered_ptr = reinterpret_cast<int*>(uptr);
        CHECK_EQ(recovered_ptr, &dummy);

        recovered_ptr = reinterpret_cast<int*>(iptr);
        CHECK_EQ(recovered_ptr, &dummy);
    }

    SUBCASE("ptrdiff_t") {
        // Signed type for pointer arithmetic
        int arr[10];
        ptrdiff_t diff = &arr[5] - &arr[2];

        CHECK_EQ(sizeof(ptrdiff_t), sizeof(void*));
        CHECK_EQ(diff, 3);

        // Test negative difference
        diff = &arr[2] - &arr[5];
        CHECK_EQ(diff, -3);
    }
}

TEST_CASE("stdint limit macros") {
    SUBCASE("INT8_MIN and INT8_MAX") {
        CHECK_EQ(INT8_MIN, -128);
        CHECK_EQ(INT8_MAX, 127);

        // Verify these are the actual limits
        int8_t min_val = INT8_MIN;
        int8_t max_val = INT8_MAX;
        CHECK_EQ(min_val, -128);
        CHECK_EQ(max_val, 127);
    }

    SUBCASE("INT16_MIN and INT16_MAX") {
        CHECK_EQ(INT16_MIN, -32768);
        CHECK_EQ(INT16_MAX, 32767);

        int16_t min_val = INT16_MIN;
        int16_t max_val = INT16_MAX;
        CHECK_EQ(min_val, -32768);
        CHECK_EQ(max_val, 32767);
    }

    SUBCASE("INT32_MIN and INT32_MAX") {
        CHECK_EQ(INT32_MIN, -2147483648);
        CHECK_EQ(INT32_MAX, 2147483647);

        int32_t min_val = INT32_MIN;
        int32_t max_val = INT32_MAX;
        CHECK_EQ(min_val, -2147483648);
        CHECK_EQ(max_val, 2147483647);
    }

    SUBCASE("INT64_MIN and INT64_MAX") {
        CHECK_EQ(INT64_MIN, -9223372036854775807LL - 1);
        CHECK_EQ(INT64_MAX, 9223372036854775807LL);

        int64_t min_val = INT64_MIN;
        int64_t max_val = INT64_MAX;
        CHECK_EQ(min_val, -9223372036854775807LL - 1);
        CHECK_EQ(max_val, 9223372036854775807LL);
    }

    SUBCASE("UINT8_MAX") {
        CHECK_EQ(UINT8_MAX, 0xFF);
        CHECK_EQ(UINT8_MAX, 255);

        uint8_t max_val = UINT8_MAX;
        CHECK_EQ(max_val, 255);
    }

    SUBCASE("UINT16_MAX") {
        CHECK_EQ(UINT16_MAX, 0xFFFF);
        CHECK_EQ(UINT16_MAX, 65535);

        uint16_t max_val = UINT16_MAX;
        CHECK_EQ(max_val, 65535);
    }

    SUBCASE("UINT32_MAX") {
        CHECK_EQ(UINT32_MAX, 0xFFFFFFFFU);
        CHECK_EQ(UINT32_MAX, 4294967295U);

        uint32_t max_val = UINT32_MAX;
        CHECK_EQ(max_val, 4294967295U);
    }

    SUBCASE("UINT64_MAX") {
        CHECK_EQ(UINT64_MAX, 0xFFFFFFFFFFFFFFFFULL);
        CHECK_EQ(UINT64_MAX, 18446744073709551615ULL);

        uint64_t max_val = UINT64_MAX;
        CHECK_EQ(max_val, 18446744073709551615ULL);
    }
}

TEST_CASE("stdint type relationships") {
    SUBCASE("signed and unsigned relationships") {
        // Unsigned types should have twice the positive range
        CHECK_EQ(static_cast<uint8_t>(UINT8_MAX), 255);
        CHECK_EQ(INT8_MAX, 127);
        CHECK(UINT8_MAX > static_cast<unsigned>(INT8_MAX));

        CHECK_EQ(static_cast<uint16_t>(UINT16_MAX), 65535);
        CHECK_EQ(INT16_MAX, 32767);
        CHECK(UINT16_MAX > static_cast<unsigned>(INT16_MAX));

        CHECK_EQ(UINT32_MAX, 4294967295U);
        CHECK_EQ(INT32_MAX, 2147483647);
        CHECK(UINT32_MAX > static_cast<uint32_t>(INT32_MAX));

        CHECK_EQ(UINT64_MAX, 18446744073709551615ULL);
        CHECK_EQ(INT64_MAX, 9223372036854775807LL);
        CHECK(UINT64_MAX > static_cast<uint64_t>(INT64_MAX));
    }

    SUBCASE("size progression") {
        // Each size should be double the previous
        CHECK_EQ(sizeof(uint16_t), sizeof(uint8_t) * 2);
        CHECK_EQ(sizeof(uint32_t), sizeof(uint16_t) * 2);
        CHECK_EQ(sizeof(uint64_t), sizeof(uint32_t) * 2);

        CHECK_EQ(sizeof(int16_t), sizeof(int8_t) * 2);
        CHECK_EQ(sizeof(int32_t), sizeof(int16_t) * 2);
        CHECK_EQ(sizeof(int64_t), sizeof(int32_t) * 2);
    }

    SUBCASE("pointer-sized types") {
        // size_t, uintptr_t, intptr_t, and ptrdiff_t should all be pointer-sized
        CHECK_EQ(sizeof(size_t), sizeof(void*));
        CHECK_EQ(sizeof(uintptr_t), sizeof(void*));
        CHECK_EQ(sizeof(intptr_t), sizeof(void*));
        CHECK_EQ(sizeof(ptrdiff_t), sizeof(void*));
    }
}

TEST_CASE("stdint arithmetic operations") {
    SUBCASE("8-bit arithmetic") {
        uint8_t u8 = 100;
        u8 += 50;
        CHECK_EQ(u8, 150);

        u8 += 200;  // Should wrap
        CHECK_EQ(u8, 94);  // (150 + 200) % 256 = 350 % 256 = 94

        // Test signed arithmetic (avoid UB overflow, use explicit wrapping)
        int8_t i8 = 50;
        i8 += 50;
        CHECK_EQ(i8, 100);

        // Signed overflow is UB, so use unsigned arithmetic and cast
        uint8_t u8_temp2 = static_cast<uint8_t>(i8) + 50;
        i8 = static_cast<int8_t>(u8_temp2);
        CHECK_EQ(i8, -106);  // (100 + 50) = 150, reinterpreted as int8_t = -106
    }

    SUBCASE("16-bit arithmetic") {
        uint16_t u16 = 60000;
        u16 += 10000;
        CHECK_EQ(u16, 4464);  // (70000) % 65536 = 4464

        // Signed overflow is UB, use unsigned arithmetic and cast
        int16_t i16 = 30000;
        uint16_t u16_temp = static_cast<uint16_t>(i16) + 5000;
        i16 = static_cast<int16_t>(u16_temp);
        CHECK_EQ(i16, -30536);  // (35000) reinterpreted as int16_t = -30536
    }

    SUBCASE("32-bit arithmetic") {
        uint32_t u32 = 4000000000U;
        u32 += 500000000U;
        CHECK_EQ(u32, 205032704U);  // Wraps around

        // Signed overflow is UB, use unsigned arithmetic and cast
        int32_t i32 = 2000000000;
        uint32_t u32_temp = static_cast<uint32_t>(i32) + 500000000;
        i32 = static_cast<int32_t>(u32_temp);
        CHECK_EQ(i32, -1794967296);  // (2500000000) reinterpreted as int32_t
    }

    SUBCASE("bitwise operations") {
        uint32_t mask = 0xFF00FF00U;
        uint32_t value = 0x12345678U;

        uint32_t masked = value & mask;
        CHECK_EQ(masked, 0x12005600U);

        uint32_t combined = value | mask;
        CHECK_EQ(combined, 0xFF34FF78U);

        uint32_t toggled = value ^ mask;
        CHECK_EQ(toggled, 0xED34A978U);
    }
}

TEST_CASE("stdint constexpr compatibility") {
    // Test that limit macros can be used in constexpr contexts
    SUBCASE("compile-time constants") {
        constexpr int8_t min8 = INT8_MIN;
        constexpr int8_t max8 = INT8_MAX;
        constexpr uint8_t umax8 = UINT8_MAX;

        CHECK_EQ(min8, -128);
        CHECK_EQ(max8, 127);
        CHECK_EQ(umax8, 255);

        constexpr int32_t min32 = INT32_MIN;
        constexpr int32_t max32 = INT32_MAX;
        constexpr uint32_t umax32 = UINT32_MAX;

        CHECK_EQ(min32, -2147483648);
        CHECK_EQ(max32, 2147483647);
        CHECK_EQ(umax32, 4294967295U);
    }

    SUBCASE("static assertions") {
        // These should compile successfully
        static_assert(sizeof(uint8_t) == 1, "uint8_t should be 1 byte");
        static_assert(sizeof(uint16_t) == 2, "uint16_t should be 2 bytes");
        static_assert(sizeof(uint32_t) == 4, "uint32_t should be 4 bytes");
        static_assert(sizeof(uint64_t) == 8, "uint64_t should be 8 bytes");

        static_assert(INT8_MAX == 127, "INT8_MAX should be 127");
        static_assert(UINT8_MAX == 255, "UINT8_MAX should be 255");
        static_assert(INT16_MAX == 32767, "INT16_MAX should be 32767");
        static_assert(UINT16_MAX == 65535, "UINT16_MAX should be 65535");
        static_assert(INT32_MAX == 2147483647, "INT32_MAX should be 2147483647");
        static_assert(UINT32_MAX == 4294967295U, "UINT32_MAX should be 4294967295U");
    }
}
